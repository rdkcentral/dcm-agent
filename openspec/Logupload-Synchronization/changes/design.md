# Design: Reboot Log Upload Correctness

## Architecture

All changes are confined to the `uploadstblogs` library. No changes to `reboot-reason-fetcher`, `systemtimemgr`, or `dcm-agent` main.

```
strategies.c                archive_manager.c
  reboot_setup()              generate_archive_name()
       │                              │
       │  (existing 330s sleep)       │  time(NULL)  ← PROBLEM
       ▼                              │
  reboot_archive()                    │
       │                              │
       ├─ wait_for_ntp_sync()  ───────┘  NEW: poll /tmp/stt_received
       │    (with timeout)
       └─ create_archive()
             │
             └─ generate_archive_name()  now called after NTP confirmed

  reboot_upload()
       │
       ├─ wait_for_reboot_reason()  NEW: poll /tmp/Update_rebootInfo_invoked
       │    (with timeout)
       └─ fopen(previousreboot.info)  existing logic unchanged
```

## New Functions

### `wait_for_flag(const char* flag_path, int timeout_seconds)` — `file_operations.c`

```c
/**
 * @brief Poll for a flag file to appear, up to a timeout.
 * @param flag_path  Absolute path to the sentinel file.
 * @param timeout_s  Maximum seconds to wait. 0 = check once only.
 * @return true if flag appeared within timeout, false if timed out.
 */
bool wait_for_flag(const char* flag_path, int timeout_s);
```

- Polls `access(flag_path, F_OK)` every 5 seconds.
- Returns `true` immediately if flag already exists.
- Returns `false` after `timeout_s` seconds; caller decides how to proceed.
- Uses `O_NOFOLLOW`-safe check (calls `open()` with `O_RDONLY | O_NOFOLLOW`, treats `ELOOP` as absent).
- Exposed in `file_operations.h`.

### Constants — `strategies.c` (internal `#define`)

```c
#define NTP_SYNC_FLAG          "/tmp/stt_received"
#define REBOOT_REASON_READY_FLAG "/tmp/Update_rebootInfo_invoked"
#define NTP_WAIT_TIMEOUT_S     120   /* seconds to wait for NTP after uptime sleep */
#define REBOOT_REASON_WAIT_TIMEOUT_S 120
```

These can be overridden at compile time via `CFLAGS` for testing.

## Changes to `reboot_archive()` — `strategies.c`

**Before** (paraphrased):
```c
static int reboot_archive(RuntimeContext* ctx, SessionState* session) {
    collect_pcap_logs(...);
    int ret = create_archive(ctx, session, ctx->prev_log_path);
    sleep(60);
    return ret;
}
```

**After**:
```c
static int reboot_archive(RuntimeContext* ctx, SessionState* session) {
    collect_pcap_logs(...);

    // Wait for NTP sync before stamping the archive filename.
    // If /tmp/stt_received is absent, archive timestamp may be inaccurate.
    if (!wait_for_flag(NTP_SYNC_FLAG, NTP_WAIT_TIMEOUT_S)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                "[%s:%d] NTP sync flag not present after %d s; "
                "archive timestamp may be inaccurate\n",
                __FUNCTION__, __LINE__, NTP_WAIT_TIMEOUT_S);
        t2_count_notify("UPLOADSTB_WARN_NTP_TIMEOUT");
    }

    int ret = create_archive(ctx, session, ctx->prev_log_path);
    sleep(60);
    return ret;
}
```

The check is placed **before** `create_archive()` so the archive name is generated after the wait resolves. The existing 60-second post-archive sleep is preserved unchanged.

## Changes to `reboot_upload()` — `strategies.c`

**Before** (paraphrased):
```c
static int reboot_upload(RuntimeContext* ctx, SessionState* session) {
    const char* reboot_info_path = "/opt/secure/reboot/previousreboot.info";
    bool is_scheduled_reboot = false;
    FILE* reboot_file = fopen(reboot_info_path, "r");
    if (reboot_file) { /* parse */ }
    else { RDK_LOG(RDK_LOG_WARN, ...); }
    ...
}
```

**After**:
```c
static int reboot_upload(RuntimeContext* ctx, SessionState* session) {
    const char* reboot_info_path = "/opt/secure/reboot/previousreboot.info";

    // Wait for reboot-reason-fetcher to finish writing previousreboot.info.
    // The sentinel /tmp/Update_rebootInfo_invoked is created at the end of
    // update_previous_reboot_info.sh (and rebootreason_main).
    if (!wait_for_flag(REBOOT_REASON_READY_FLAG, REBOOT_REASON_WAIT_TIMEOUT_S)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                "[%s:%d] Reboot reason flag not present after %d s; "
                "reboot reason classification may be incorrect\n",
                __FUNCTION__, __LINE__, REBOOT_REASON_WAIT_TIMEOUT_S);
        t2_count_notify("UPLOADSTB_WARN_REBOOTREASON_TIMEOUT");
    }

    bool is_scheduled_reboot = false;
    FILE* reboot_file = fopen(reboot_info_path, "r");
    /* rest of existing logic unchanged */
    ...
}
```

## Fix: UTC-only timestamps in `reboot_setup()` — `strategies.c`

Replace:
```c
struct tm* tm_info = localtime(&now);           // not thread-safe, local TZ
strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p-logbackup", tm_info);
```

With:
```c
struct tm tm_info;
if (gmtime_r(&now, &tm_info) == NULL) {
    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] gmtime_r failed\n",
            __FUNCTION__, __LINE__);
    return -1;
}
strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p-logbackup", &tm_info);
```

This makes backup directory names and archive filenames use the same UTC reference, eliminating the UTC-offset discrepancy.

## Timeout Behavior Summary

| Situation | NTP wait result | Reboot reason wait result | Outcome |
|---|---|---|---|
| Both ready within timeout | flag present | flag present | Correct timestamp + correct reason |
| NTP slow but reason ready | timeout → WARN + T2 marker | flag present | Best-effort timestamp, correct reason |
| Reason slow but NTP ready | flag present | timeout → WARN + T2 marker | Correct timestamp, best-effort reason |
| Both slow (severe boot failure) | timeout → WARN | timeout → WARN | Both WARNs logged; upload proceeds with best-effort data |
| Both ready before upload even starts | immediate | immediate | No added latency |

In all cases the upload **is not blocked** — the timeouts are advisory. This preserves the existing behavior of "upload something" even in degraded boot conditions, while adding observability for when the data quality is uncertain.

## Unit Test Approach

New tests in `strategies_gtest.cpp` and `file_operations_gtest.cpp`:

- `wait_for_flag`: flag present immediately, flag appears after N polls, timeout expires.
- `reboot_archive`: mock `wait_for_flag` to return false; verify `t2_count_notify` called with `UPLOADSTB_WARN_NTP_TIMEOUT`; verify `create_archive` still called.
- `reboot_upload`: mock `wait_for_flag` to return false; verify `t2_count_notify` called with `UPLOADSTB_WARN_REBOOTREASON_TIMEOUT`; verify upload proceeds.
- `reboot_setup` timestamp: verify backup directory name uses UTC not local time.

All tests use the existing `L2_TEST_ENABLED` macro convention to skip real sleeps.
