# Low Level Design – `uploadSTBLogs` (Strict Diagram Alignment)

## 1. Functional Partitioning (Diagram Nodes → Functions)

| Node | Function(s) |
|------|-------------|
| Main Entry Point | `int main(int, char**)` |
| Context Initialization | `init_context(RuntimeContext*)` |
| System Validation | `validate_system(RuntimeContext*)` |
| Early Return Checks | `Strategy early_checks(RuntimeContext*)` |
| Strategy Selector | `Strategy select_strategy(RuntimeContext*)` |
| Selected Strategy | Stored in `SessionState.strategy` |
| Archive Manager | `prepare_archive(RuntimeContext*)`, `prepare_rrd_archive(RuntimeContext*)` |
| Upload Execution Engine | `execute_upload_cycle(RuntimeContext*, SessionState*)` |
| Direct Upload Path | `presign_direct()`, `upload_direct()` |
| CodeBig Upload Path | `presign_codebig()`, `upload_codebig()` |
| Fallback Handler | Integrated in `execute_upload_cycle()` |
| MTLS Authentication | `setup_mtls(SecurityContext*)` |
| OAuth Authentication | `setup_oauth(SecurityContext*)` |
| Retry Logic | Loops in `execute_upload_cycle()` |
| HTTP/HTTPS Transfer | Libcurl calls |
| Upload Verification | Status checks within upload cycle |
| Cleanup & Notification | `finalize(RuntimeContext*, SessionState*)` |

## 2. Core Structures

```c
typedef struct {
    int rrd_flag;
    int dcm_flag;
    int flag;
    int upload_on_reboot;
    int trigger_type;
    bool privacy_do_not_share;
    bool ocsp_enabled;
    bool encryption_enable;
    bool direct_blocked;
    bool codebig_blocked;
    char log_path[256];
    char prev_log_path[256];
    char archive_path[256];
    char rrd_file[256];
    char endpoint_url[512];
    char upload_http_link[512];
} RuntimeContext;

typedef enum {
    STRAT_RRD,
    STRAT_PRIVACY_ABORT,
    STRAT_NO_LOGS,
    STRAT_NON_DCM,
    STRAT_ONDEMAND,
    STRAT_REBOOT,
    STRAT_DCM
} Strategy;

typedef enum {
    PATH_DIRECT,
    PATH_CODEBIG
} UploadPath;

typedef struct {
    Strategy strategy;
    UploadPath primary;
    UploadPath fallback;
    int direct_attempts;
    int codebig_attempts;
    int http_code;
    int curl_code;
    bool used_fallback;
    bool success;
} SessionState;
```

## 3. Early Return Checks

```c
Strategy early_checks(RuntimeContext* ctx) {
    if (ctx->rrd_flag == 1) return STRAT_RRD;
    if (ctx->privacy_do_not_share) return STRAT_PRIVACY_ABORT;
    if (!logs_exist(ctx->prev_log_path)) return STRAT_NO_LOGS;
    return STRAT_DCM; // provisional, replaced by select_strategy if continue
}
```

## 4. Strategy Selector (Continue Path)

```c
Strategy select_strategy(RuntimeContext* ctx) {
    if (ctx->rrd_flag == 1) return STRAT_RRD;
    if (ctx->privacy_do_not_share) return STRAT_PRIVACY_ABORT;
    if (!logs_exist(ctx->prev_log_path)) return STRAT_NO_LOGS;
    if (ctx->trigger_type == 5) return STRAT_ONDEMAND;
    if (ctx->dcm_flag == 0) return STRAT_NON_DCM;
    if (ctx->upload_on_reboot == 1 && ctx->flag == 1) return STRAT_REBOOT;
    return STRAT_DCM;
}
```

## 5. Archive Manager

- `prepare_archive`:
  - Timestamp rename (except OnDemand, Privacy, RRD paths).
  - Include DRI logs if present.
  - Include latest PCAP capture.
  - Create `.tgz` via streaming.

```c
bool prepare_archive(RuntimeContext* ctx) {
    if (!collect_files(ctx->log_path)) return false;
    if (needs_timestamp(ctx)) timestamp_prefix(ctx->log_path);
    return create_tgz(ctx->log_path, ctx->archive_path);
}
```

## 6. Upload Execution Cycle

```c
void execute_upload_cycle(RuntimeContext* ctx, SessionState* st) {
    decide_paths(ctx, st); // sets primary/fallback
    UploadPath attempts[2] = { st->primary, st->fallback };
    for (int i = 0; i < 2; ++i) {
        UploadPath path = attempts[i];
        if (path == PATH_DIRECT && ctx->direct_blocked) continue;
        if (path == PATH_CODEBIG && ctx->codebig_blocked) continue;

        if (!presign_request(ctx, st, path)) {
            if (terminal_presign(st->http_code)) break;
            continue; // fallback or end
        }
        if (upload_archive(ctx, st, path)) {
            st->success = true;
            if (i == 1) st->used_fallback = true;
            update_blocks_on_success(ctx, path);
            return;
        }
        if (terminal_upload(st->http_code)) break;
    }
    st->success = false;
    update_blocks_on_failure(ctx, st);
}
```

## 7. Presign Request (Direct vs CodeBig)

```c
bool presign_request(RuntimeContext* ctx, SessionState* st, UploadPath path) {
    if (path == PATH_DIRECT) {
        setup_mtls();
        // perform POST/GET; set st->http_code, st->curl_code
    } else {
        setup_oauth();
        // signed request; set codes
    }
    return st->http_code == 200;
}
```

Terminal conditions:
- HTTP 404 → terminal failure (no fallback).
- Other non-200 → eligible for fallback unless attempts exceed.

## 8. Upload Archive

```c
bool upload_archive(RuntimeContext* ctx, SessionState* st, UploadPath path) {
    // Use S3 URL from presign response
    // libcurl PUT archive
    // Set st->http_code & st->curl_code
    return (st->http_code == 200 && st->curl_code == 0);
}
```

## 9. Retry Logic
Contained within loop; attempts variable (direct vs codebig). Only one fallback iteration.

## 10. Verification
Success criteria: HTTP 200 + curl code 0.
Failure classification:
- Cert error codes trigger telemetry.
- 404 ends cycle immediately.

## 11. Cleanup & Notification

```c
void finalize(RuntimeContext* ctx, SessionState* st) {
    if (file_exists(ctx->archive_path)) unlink(ctx->archive_path);
    if (st->success) emit_success(st);
    else if (st->strategy == STRAT_PRIVACY_ABORT) emit_privacy_abort();
    else if (st->strategy == STRAT_NO_LOGS) emit_no_logs();
    else emit_failure(st);
}
```

## 12. Block Marker Updates

| Condition | Action |
|-----------|--------|
| Success via CodeBig | Set direct block (24h) |
| Failure CodeBig | Set codebig block (30m) |
| Success via Direct | Clear expired codebig block if necessary |
| Failure Direct | No immediate block unless policy requires |

## 13. Telemetry Emission

| Event | Trigger |
|-------|---------|
| logupload_success | Final success |
| logupload_failed | Final failure |
| logupload_fallback | `used_fallback == true` |
| logupload_privacy_abort | Strategy PRIVACY_ABORT |
| logupload_no_logs | Strategy NO_LOGS |
| logupload_cert_error | Cert error codes encountered |

## 14. Security Layer

```c
void setup_mtls() {
    // Configure curl easy handle: cert, key, TLSv1.2, OCSP if enabled
}

void setup_oauth() {
    // Acquire signed URL via service function, build Authorization header
}
```

Signature redaction before logging:
```c
const char* redact_signature(const char* url);
```

## 15. Privacy Enforcement

```c
void enforce_privacy(const char* path) {
    for each file in path: open O_TRUNC then close
}
```

## 16. Minimal Error Handling Map

| Source | Reaction |
|--------|----------|
| Missing archive creation | Failure event |
| Presign 404 | Immediate failure |
| Curl timeout (28) | Retry if attempts left |
| Cert error | Log + telemetry; continue attempts |
| Abort signal (if used) | Convert to failure (or aborted classification) |

## 17. Pseudocode (Combined Execution)

```c
int run(RuntimeContext* ctx) {
    Strategy s = select_strategy(ctx);
    SessionState st = { .strategy = s };

    if (s == STRAT_PRIVACY_ABORT) { enforce_privacy(ctx->log_path); finalize(ctx, &st); return 0; }
    if (s == STRAT_NO_LOGS) { finalize(ctx, &st); return 0; }

    if (s == STRAT_RRD) {
        if (!prepare_rrd_archive(ctx)) { finalize(ctx, &st); return 1; }
    } else {
        if (!prepare_archive(ctx)) { finalize(ctx, &st); return 1; }
    }

    decide_paths(ctx, &st);
    execute_upload_cycle(ctx, &st);
    finalize(ctx, &st);
    return st.success ? 0 : 1;
}
```

## 18. Constants

```c
#define DIRECT_MAX_ATTEMPTS_REBOOT 3
#define DIRECT_MAX_ATTEMPTS_PLUGIN 1
#define CODEBIG_MAX_ATTEMPTS 1
#define DIRECT_RETRY_SLEEP_SEC 60
#define CODEBIG_RETRY_SLEEP_SEC 10
#define DIRECT_BLOCK_SECONDS 86400
#define CODEBIG_BLOCK_SECONDS 1800
```

## 19. Logging Strategy (Essential Only)

```c
void log_info(const char* msg);
void log_error(const char* msg);
void log_cert_error(int code);
```

No extended levels; keep minimal to match simplicity of diagram nodes.

## 20. Acceptance Mapping
Every diagram component maps directly to implemented functions without extra abstraction layers.
