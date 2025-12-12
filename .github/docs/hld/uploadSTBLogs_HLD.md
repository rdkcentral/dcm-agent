# High Level Design – `uploadSTBLogs` (Strict Diagram Alignment)

## 1. Architectural Nodes (From Diagram)

| Node | Description |
|------|-------------|
| Main Entry Point | Program start, argument parse, lock acquisition |
| Context Initialization | Load environment, TR-181/RFC values, paths |
| System Validation | Verify required directories, binaries, configuration |
| Early Return Checks | Decide: RRD, Privacy, No Logs, or Continue |
| Strategy Selector | Choose one concrete strategy among Non-DCM, OnDemand, Reboot, DCM |
| Selected Strategy | Holds chosen strategy outcome |
| Non-DCM Strategy | Upload on reboot without DCM batching |
| OnDemand Strategy | Immediate log packaging and upload request |
| Reboot Strategy | Reboot-triggered upload with potential initial delay |
| DCM Strategy | Batching / scheduled accumulation case |
| Archive Manager | Timestamp adjustments, collection, packaging |
| Upload Execution Engine | Orchestrates path decision, retries, fallback |
| Direct Upload Path | mTLS pre-sign & upload route |
| CodeBig Upload Path | OAuth pre-sign & upload route |
| Fallback Handler | Switch between paths when allowed |
| MTLS Authentication | Cert-based secure channel configuration |
| OAuth Authentication | Authorization header via signing function |
| Retry Logic | Controlled loops per path attempts |
| HTTP/HTTPS Transfer | Pre-sign request + S3 PUT |
| Upload Verification | Interpret HTTP/curl status |
| Cleanup & Notification | Archive removal, restore state, emit events |

Support Modules (grouped in diagram):
- Configuration Manager
- Log Collector
- File Operations
- Event Manager

Security Layer:
- Certificate Management
- TLS/MTLS Handler
- OCSP Validation

## 2. Flow Summary
1. Main Entry → Initialize context.
2. Validate system prerequisites.
3. Perform early checks:
   - If RRD → RRD strategy (single file upload).
   - If Privacy mode → abort.
   - If No logs → exit.
   - Else → Strategy Selector.
4. Selected Strategy directs Archive Manager behavior (timestamp, inclusion rules).
5. Upload Execution Engine:
   - Decide initial path (Direct vs CodeBig) respecting block states.
   - Perform pre-sign request (Authentication included).
   - Apply Retry Logic; fallback if non-terminal failure and alternate available.
   - Execute upload to S3.
6. Verify upload result.
7. Cleanup & Notification: remove archive, update block markers, telemetry/events.

## 3. Strategy Conditions (Exact Mapping)

| Strategy | Condition |
|----------|-----------|
| RRD | `RRD_FLAG == 1` |
| Privacy Abort | Privacy mode == `DO_NOT_SHARE` |
| No Logs | Previous logs directory empty |
| Non-DCM | `DCM_FLAG == 0` |
| OnDemand | `TriggerType == 5` |
| Reboot | `UploadOnReboot == 1 && FLAG == 1 && DCM_FLAG == 1` |
| DCM | Remaining continuation path |

## 4. Core Data Structures

```c
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
```

## 5. Path & Fallback Rules
- Primary selection: Prefer Direct if not blocked; else CodeBig if not blocked.
- Fallback Handler engaged only on:
  - Non-terminal failure (not HTTP 404).
  - Alternate path is unblocked.
- Single fallback cycle permitted (no ping-pong loops).

## 6. Upload Execution Steps
1. Pre-sign Request (Direct mTLS or CodeBig OAuth).
2. Evaluate HTTP code:
   - 200: proceed with S3 PUT.
   - 404: terminal failure (no retry).
   - Other: retry within allowed attempts or fallback.
3. S3 Upload (PUT) with TLS/MTLS or standard TLS (CodeBig).
4. Verification: Success if curl success and HTTP 200.

## 7. Retry Logic
| Path | Attempts | Delay |
|------|----------|-------|
| Direct | N (1 or 3 per trigger context) | 60s |
| CodeBig | M (1 default) | 10s |

Stops early on success; fallback evaluated after attempts exhausted.

## 8. Authentication Layer
| Path | Mechanism |
|------|-----------|
| Direct | mTLS certificates (xPKI) |
| CodeBig | OAuth header from signed service URL |
| OCSP | Add stapling if marker files present |

## 9. Archive Manager Functions
- Timestamp insertion for non OnDemand/Privacy/RRD cases requiring renaming.
- Collect `.log`/`.txt`, optionally PCAP and DRI.
- Create `.tgz` archive (streaming).
- Reverse timestamp if needed post-upload (reboot strategy parity).

## 10. Verification & Cleanup
- Upload Verification: interpret `curl_code` and `http_code`.
- Cleanup:
  - Delete archive file.
  - Manage block markers (success on CodeBig → block direct 24h; failure on CodeBig → block codebig 30m).
  - Remove temporary directories.
  - Emit events and telemetry.

## 11. Telemetry (Minimal)
| Key | Trigger |
|-----|---------|
| logupload_success | Upload verified success |
| logupload_failed | Terminal failure |
| logupload_fallback | Fallback engaged |
| logupload_privacy_abort | Privacy early exit |
| logupload_no_logs | Empty log early exit |
| logupload_cert_error | TLS cert error codes |
| logupload_rrd | RRD upload executed |

## 12. Events
- Success: `LogUploadEvent` success code.
- Failure: `LogUploadEvent` failure code.
- Aborted (privacy/no logs): `LogUploadEvent` aborted code.

## 13. Security Layer Notes
- Certificate Management: load paths once at init.
- TLS Handler: enforce TLSv1.2 and optional OCSP.
- Signature Redaction: remove signature query param from any logged URL.

## 14. Pseudocode (Condensed)

```c
int main(int argc, char** argv) {
    RuntimeContext ctx = {0};
    SessionState st = {0};

    if (!parse_args(argc, argv, &ctx)) return 1;
    if (!acquire_lock("/tmp/.log-upload.lock")) return 1;

    init_context(&ctx);
    if (!validate_system(&ctx)) { release_lock(); return 1; }

    Strategy s = early_checks(&ctx);
    st.strategy = s;

    switch (s) {
        case STRAT_PRIVACY_ABORT: enforce_privacy(ctx.log_path); emit_privacy_abort(); release_lock(); return 0;
        case STRAT_NO_LOGS: emit_no_logs(); release_lock(); return 0;
        case STRAT_RRD: prepare_rrd_archive(&ctx); break;
        default: prepare_archive(&ctx); break;
    }

    decide_paths(&ctx, &st);
    execute_upload_cycle(&ctx, &st);
    finalize(&ctx, &st);

    release_lock();
    return st.success ? 0 : 1;
}
```

## 15. Acceptance Mapping
| Diagram Node | Implemented Element |
|--------------|---------------------|
| Early Return Checks | `early_checks` |
| Strategy Selector | `decide_paths` + strategy enum |
| Archive Manager | `prepare_archive` / `prepare_rrd_archive` |
| Upload Execution Engine | `execute_upload_cycle` |
| Authentication nodes | Path-specific setup inside execution cycle |
| Retry Logic | Loop constructs in upload cycle |
| Verification | `st.http_code`, `st.curl_code` evaluation |
| Cleanup & Notification | `finalize` |

## 16. Constraints Enforcement
- No extra managers beyond diagram.
- Linear flow; minimal abstraction.
- mTLS and OAuth limited to diagram scope.

## 17. Risks & Mitigations
| Risk | Mitigation |
|------|------------|
| Large log size | Stream file packaging |
| Fallback mis-config | Single fallback attempt rule |
| Race on block files | Single-process lock holds entire run |
| Missed privacy enforcement | Early truncation and exit path log |

## 18. Non-Extended Design Choices
Excluded any unrelated enhancements (alternate compression, multi-protocol expansion, scheduler integration) to preserve diagram fidelity.

```
