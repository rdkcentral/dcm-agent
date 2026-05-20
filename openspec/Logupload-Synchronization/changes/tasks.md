# Tasks: Reboot Sequence Synchronization

> Execute tasks in the order listed. Tasks within the same group can be parallelized
> across contributors. Cross-repo tasks are marked with their target repository:
> `[reboot-manager]` for the reboot-manager repo, `[telemetry]` for the telemetry repo.

---

## Group A — Sentinel Infrastructure (Must complete before Groups B and C)

### TASK-A1 — Define `BACKUP_LOGS_DONE_FLAG` constant in backup_logs header
**Repo**: `dcm-agent`  
**File**: `backup_logs/include/backup_logs.h`  
**Spec**: REQ-SYNC-006

Add the sentinel path constant and a cross-repo interface comment:

```c
/** Sentinel written by backup_logs after successful completion.
 *  Cross-repo interface: also referenced by reboot-manager's update-prev-reboot-info.
 *  Any path change MUST be coordinated with the reboot-manager repository. */
#define BACKUP_LOGS_DONE_FLAG  "/tmp/.backup_logs_done"
```

---

### TASK-A2 — Define `PATH_FLAG_BACKUP_LOGS_DONE` and poll constants in reboot-manager header
**Repo**: `reboot-manager`  
**File**: `reboot-reason-fetcher/include/update-reboot-info.h`  
**Spec**: REQ-SYNC-006

Add alongside existing sentinel constants (`STT_FLAG`, `PATH_FLAG_REBOOT_INFO_UPDATED`):

```c
/** Sentinel written by backup_logs (dcm-agent) on successful completion.
 *  Cross-repo interface: path is also defined in
 *  dcm-agent/backup_logs/include/backup_logs.h.
 *  Any change here MUST be coordinated with dcm-agent. */
#define PATH_FLAG_BACKUP_LOGS_DONE     "/tmp/.backup_logs_done"
#define BACKUP_LOGS_POLL_INTERVAL_S    1u
#define BACKUP_LOGS_POLL_TIMEOUT_S     60u
```

---

## Group B — backup_logs Sentinel Write

### TASK-B1 — Write `/tmp/.backup_logs_done` on successful backup
**Repo**: `dcm-agent`  
**File**: `backup_logs/src/backup_logs.c`  
**Spec**: REQ-SYNC-001  
**Depends on**: TASK-A1

In `backup_logs_main()`, immediately after `backup_logs_execute()` returns `BACKUP_SUCCESS`
and before `backup_logs_cleanup()`, write the sentinel file:

```c
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

/* After backup_logs_execute() success: */
{
    int sentinel_fd = open(BACKUP_LOGS_DONE_FLAG, O_CREAT | O_WRONLY, 0644);
    if (sentinel_fd < 0) {
        LOG_WARN("Failed to create sentinel %s: %s",
                 BACKUP_LOGS_DONE_FLAG, strerror(errno));
        /* Non-fatal: downstream update-prev-reboot-info will timeout and exit cleanly */
    } else {
        close(sentinel_fd);
        LOG_INFO("Sentinel written: %s", BACKUP_LOGS_DONE_FLAG);
    }
}
```

**Do NOT** write the sentinel if `backup_logs_execute()` returns any error code.

---

## Group C — update-prev-reboot-info Sentinel Poll [reboot-manager]

### TASK-C1 — Add `poll_for_sentinel()` helper
**Repo**: `reboot-manager`  
**File**: `reboot-reason-fetcher/src/rebootreason_main.c` (or a utility file)  
**Spec**: REQ-SYNC-002  
**Depends on**: TASK-A2

Add a static helper function to poll for a sentinel file by repeatedly calling `stat()`:

```c
#include <sys/stat.h>
#include <unistd.h>

/**
 * poll_for_sentinel - Wait for a sentinel file to appear.
 * @path:     Absolute path of the sentinel file.
 * @interval: Poll interval in seconds.
 * @timeout:  Maximum wait time in seconds.
 *
 * Returns 0 when the sentinel is found, -1 on timeout.
 */
static int poll_for_sentinel(const char *path, unsigned int interval,
                              unsigned int timeout)
{
    unsigned int elapsed = 0u;
    struct stat st;

    while (elapsed < timeout) {
        if (stat(path, &st) == 0) {
            return 0;
        }
        sleep(interval);
        elapsed += interval;
    }
    return -1;
}
```

---

### TASK-C2 — Gate `find_previous_reboot_log()` on `.backup_logs_done`
**Repo**: `reboot-manager`  
**File**: `reboot-reason-fetcher/src/rebootreason_main.c`  
**Spec**: REQ-SYNC-002  
**Depends on**: TASK-C1, TASK-A2

In `main()` (or the top-level orchestration function), after `update_reboot_info()` returns
and before `find_previous_reboot_log()` is called, add:

```c
LOG_INFO("Waiting for backup_logs completion sentinel: %s (timeout %us)",
         PATH_FLAG_BACKUP_LOGS_DONE, BACKUP_LOGS_POLL_TIMEOUT_S);

if (poll_for_sentinel(PATH_FLAG_BACKUP_LOGS_DONE,
                      BACKUP_LOGS_POLL_INTERVAL_S,
                      BACKUP_LOGS_POLL_TIMEOUT_S) != 0) {
    LOG_ERROR("Timed out after %us waiting for %s. "
              "backup_logs may have failed. Aborting reboot reason update.",
              BACKUP_LOGS_POLL_TIMEOUT_S, PATH_FLAG_BACKUP_LOGS_DONE);
    return ERROR_GENERAL;
}

LOG_INFO("backup_logs sentinel detected. Proceeding with PreviousLogs/ analysis.");
/* find_previous_reboot_log() call follows here (unchanged) */
```

---

## Group D — uploadstblogs Sentinel Poll

### TASK-D1 — Add poll constants and sentinel paths for reboot readiness
**Repo**: `dcm-agent`  
**File**: `uploadstblogs/include/uploadstblogs_types.h`  
**Spec**: REQ-SYNC-003, REQ-SYNC-008

Add:

```c
/** NTP synchronization sentinel — written by time sync service.
 *  Explicitly checked to ensure archive timestamps are NTP-accurate. */
#define STT_FLAG                      "/tmp/stt_received"

/** Reboot reason completion sentinel — written by update-prev-reboot-info.
 *  Presence guarantees previousreboot.info is written and complete. */
#define PATH_FLAG_INVOCATION          "/tmp/Update_rebootInfo_invoked"

/** Telemetry PreviousLogs scan completion sentinel — written by telemetry2_0.
 *  Cross-repo interface: path also defined in the telemetry repository.
 *  Any change MUST be coordinated with and released simultaneously with telemetry. */
#define TELEMETRY_PREVLOGS_DONE_FLAG  "/tmp/.telemetry_prevlogs_done"

/** Poll parameters for waiting on upload prerequisites.
 *  See uploadstblogs/src/strategies.c reboot_setup(). */
#define REBOOT_POLL_INTERVAL_S        1u
#define REBOOT_POLL_TIMEOUT_S         120u
```

---

### TASK-D2 — Replace `sleep(330)` with triple-sentinel poll in `reboot_setup()`
**Repo**: `dcm-agent`  
**File**: `uploadstblogs/src/strategies.c`  
**Spec**: REQ-SYNC-003, REQ-SYNC-004  
**Depends on**: TASK-D1

Add `wait_for_upload_prerequisites()` static helper that checks **all three** sentinels
`/tmp/stt_received` (NTP sync), `/tmp/Update_rebootInfo_invoked` (reboot reason ready),
and `/tmp/.telemetry_prevlogs_done` (telemetry done with PreviousLogs/). Use
`clock_gettime(CLOCK_MONOTONIC)` for accurate timeout tracking (avoids `EINTR` drift):

```c
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int wait_for_upload_prerequisites(void)
{
    struct timespec start, now;
    struct stat st;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (;;) {
        int ntp_ready      = (stat(STT_FLAG, &st) == 0) ? 1 : 0;
        int reboot_ready   = (stat(PATH_FLAG_INVOCATION, &st) == 0) ? 1 : 0;
        int telemetry_done = (stat(TELEMETRY_PREVLOGS_DONE_FLAG, &st) == 0) ? 1 : 0;

        if (ntp_ready && reboot_ready && telemetry_done) {
            return 0;
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((now.tv_sec - start.tv_sec) >= (time_t)REBOOT_POLL_TIMEOUT_S) {
            return -1;
        }
        sleep(REBOOT_POLL_INTERVAL_S);
    }
}
```

In `reboot_setup()`, replace:
```c
/* OLD — remove this block entirely */
long uptime_secs = get_system_uptime();
if (uptime_secs != -1 && uptime_secs < 900) {
    long sleep_duration = 330 - (long)(uptime_secs / 3);
    if (sleep_duration > 0) {
        sleep((unsigned int)sleep_duration);
    }
}
```

With:
```c
/* NEW — poll for NTP sync, reboot reason, and telemetry completion */
LOG_INFO("Waiting for upload prerequisites: NTP (%s), reboot reason (%s), "
         "telemetry (%s), timeout %us",
         STT_FLAG, PATH_FLAG_INVOCATION, TELEMETRY_PREVLOGS_DONE_FLAG,
         REBOOT_POLL_TIMEOUT_S);

if (wait_for_upload_prerequisites() != 0) {
    LOG_ERROR("Timed out after %us waiting for upload prerequisites "
              "(NTP: %s, RebootReason: %s, Telemetry: %s). Skipping reboot log upload.",
              REBOOT_POLL_TIMEOUT_S, STT_FLAG, PATH_FLAG_INVOCATION,
              TELEMETRY_PREVLOGS_DONE_FLAG);
    return -1;
}

LOG_INFO("All upload prerequisites met. Proceeding with archive/upload.");
```

---

## Group E — Documentation

### TASK-E1 — Update `dcm-agent/docs/DEPENDENCIES.md`
**Repo**: `dcm-agent`  
**Spec**: REQ-SYNC-006, REQ-SYNC-010

Add a section documenting both cross-repo sentinel interfaces:

```markdown
## Cross-Repository Sentinel Interfaces

### `/tmp/.backup_logs_done`
- **Written by**: `backup_logs` binary (this repo), in `backup_logs_main()` after successful `backup_logs_execute()`.
- **Consumed by**:
  - `reboot-manager` repository, `update-prev-reboot-info` binary.
  - `telemetry` repository, `telemetry2_0` daemon (`profile.c`, PreviousLogs grep report).
- **Semantics**: File presence indicates all logs have been safely moved to `/opt/logs/PreviousLogs/`.
- **Lifecycle**: Volatile (`/tmp/`); cleared automatically on reboot.
- **Coordination**: Any change MUST be released simultaneously with `reboot-manager` and `telemetry`.

### `/tmp/.telemetry_prevlogs_done`
- **Written by**: `telemetry` repository, `telemetry2_0` daemon, after completing the PreviousLogs grep scan.
- **Consumed by**: `uploadstblogs` (this repo) in `reboot_setup()` before calling `add_timestamp_to_files()`.
- **Semantics**: File presence indicates `telemetry2_0` has finished reading `PreviousLogs/`.
- **Lifecycle**: Volatile (`/tmp/`); cleared automatically on reboot.
- **Coordination**: Any change MUST be released simultaneously with `telemetry`.
```

---

### TASK-E2 — Document cross-repo dependency in reboot-manager
**Repo**: `reboot-manager`  
**File**: `docs/DEPENDENCIES.md` (create if absent)  
**Spec**: REQ-SYNC-006

Add a section documenting the cross-repo sentinel interface (mirror of TASK-E1, from
consumer perspective).

---

## Group F — Tests

### TASK-F1 — Unit test: backup_logs writes sentinel on success, not on failure
**Repo**: `dcm-agent`  
**File**: `backup_logs/unittest/backup_logs_gtest.cpp`  
**Spec**: TEST-SYNC-001  
**Depends on**: TASK-B1

Test cases:
- `BackupLogsSentinel_WrittenOnSuccess`: Mock `backup_logs_execute()` → `BACKUP_SUCCESS`.
  Assert `/tmp/.backup_logs_done` exists after `backup_logs_main()` returns `EXIT_SUCCESS`.
- `BackupLogsSentinel_NotWrittenOnFailure`: Mock `backup_logs_execute()` → non-zero.
  Assert `/tmp/.backup_logs_done` does NOT exist after `backup_logs_main()` returns
  `EXIT_FAILURE`.

Use a temporary directory override via test fixture to avoid writing to real `/tmp/`.

---

### TASK-F2 — Unit test: update-prev-reboot-info poll behavior
**Repo**: `reboot-manager`  
**File**: `reboot-reason-fetcher/unittest/rebootreason_gtest.cpp` (create if absent)  
**Spec**: TEST-SYNC-002, TEST-SYNC-003  
**Depends on**: TASK-C2

Test cases:
- `PollSentinel_FoundImmediately`: Place sentinel before calling `poll_for_sentinel()`.
  Assert returns 0.
- `PollSentinel_FoundAfterDelay`: Place sentinel in a background thread after 2s.
  Assert `poll_for_sentinel()` returns 0 within timeout.
- `PollSentinel_Timeout`: Do NOT place sentinel. Assert `poll_for_sentinel()` returns -1
  after `BACKUP_LOGS_POLL_TIMEOUT_S` seconds (use a short override for fast test execution).

---

### TASK-F3 — Unit test: reboot_setup() triple-sentinel poll and timeout behavior
**Repo**: `dcm-agent`  
**File**: `uploadstblogs/unittest/strategies_gtest.cpp` (or existing equivalent)  
**Spec**: TEST-SYNC-004, TEST-SYNC-005, TEST-SYNC-009  
**Depends on**: TASK-D2

Test cases:
- `RebootSetup_ProceedsWhenAllSentinelsPresent`: Create all three sentinels (`stt_received`,
  `Update_rebootInfo_invoked`, `.telemetry_prevlogs_done`) before calling `reboot_setup()`.
  Assert returns 0 (success).
- `RebootSetup_SkipsUploadWhenNTPMissing`: Create `Update_rebootInfo_invoked` and
  `.telemetry_prevlogs_done` only (no `stt_received`). Assert `reboot_setup()` returns -1
  after timeout.
- `RebootSetup_SkipsUploadWhenRebootReasonMissing`: Create `stt_received` and
  `.telemetry_prevlogs_done` only (no `Update_rebootInfo_invoked`). Assert returns -1.
- `RebootSetup_SkipsUploadWhenTelemetryMissing`: Create `stt_received` and
  `Update_rebootInfo_invoked` only (no `.telemetry_prevlogs_done`). Assert returns -1
  after timeout. (TEST-SYNC-009)
- `RebootSetup_SkipsUploadOnFullTimeout`: Create none of the three sentinels. Assert
  `reboot_setup()` returns -1 after `REBOOT_POLL_TIMEOUT_S` (use a short override for
  fast test execution).
- `RebootSetup_ArchiveTimestampIsPostNTP`: Stub `time()` to return a known post-NTP epoch.
  Assert archive name contains expected timestamp.

---

### TASK-F4 — L2 integration test: full boot sequence sentinel chain
**Repo**: `dcm-agent`  
**File**: `test/functional-tests/` (new feature file)  
**Spec**: TEST-SYNC-007  
**Depends on**: All Group B, C, D, G tasks

Verify the complete sentinel chain end-to-end in the Docker CI environment:

1. Start with a clean `/tmp/` (no sentinels).
2. Run `backup_logs` binary → assert `/tmp/.backup_logs_done` created.
3. Simulate `telemetry2_0` scan completion → assert `/tmp/.telemetry_prevlogs_done` created.
4. Run `update-prev-reboot-info` → assert it reads PreviousLogs/ correctly and writes
   `/tmp/Update_rebootInfo_invoked`.
5. Run `uploadstblogs` with `TRIGGER_REBOOT` → assert it proceeds without the 330s sleep.
6. Inject failure: run `update-prev-reboot-info` without first creating `.backup_logs_done`
   → assert it exits non-zero after timeout.
7. Inject failure: run `reboot_setup()` without `Update_rebootInfo_invoked` → assert
   upload is skipped.
8. Inject failure: run `reboot_setup()` without `.telemetry_prevlogs_done` → assert
   upload is skipped (telemetry timeout path).

---

---

## Group G — Telemetry Repo Changes [telemetry]

### TASK-G1 — Poll `.backup_logs_done` before PreviousLogs grep scan
**Repo**: `telemetry`  
**File**: `telemetry2_0/src/profile.c` (or equivalent, in `CollectAndReport()`)
**Spec**: REQ-SYNC-001 (consumer side), REQ-SYNC-008  
**Depends on**: TASK-B1 (sentinel written by backup_logs)

In the `checkPreviousSeek` / `PERSIST_LOG_MON_REF` code path that sets
`customLogPath = PREVIOUS_LOGS_PATH`, add a poll for `/tmp/.backup_logs_done`
before reading any files from `PreviousLogs/`:

- Poll interval: 1 second  
- Timeout: 60 seconds  
- On timeout: skip the PreviousLogs report entirely; do NOT write the completion sentinel.

---

### TASK-G2 — Write `/tmp/.telemetry_prevlogs_done` after PreviousLogs scan completes
**Repo**: `telemetry`  
**File**: `telemetry2_0/src/profile.c` (or equivalent)  
**Spec**: REQ-SYNC-008  
**Depends on**: TASK-G1

After all grep patterns for the Previous Logs report have been evaluated (whether or not
any markers were found), write the completion sentinel:

```c
/* After PreviousLogs grep scan completes successfully: */
{
    int sentinel_fd = open("/tmp/.telemetry_prevlogs_done", O_CREAT | O_WRONLY, 0644);
    if (sentinel_fd < 0) {
        LOG_WARN("Failed to create telemetry sentinel: %s", strerror(errno));
        /* Non-fatal: uploadstblogs will timeout and skip reboot upload */
    } else {
        close(sentinel_fd);
    }
}
```

Do NOT write the sentinel if the grep scan was skipped due to `.backup_logs_done` timeout.

---

### TASK-G3 — Define sentinel path constants in telemetry header
**Repo**: `telemetry`  
**File**: Appropriate telemetry header  
**Spec**: REQ-SYNC-006, REQ-SYNC-008

Define:

```c
/** Sentinel written by backup_logs (dcm-agent). Gates PreviousLogs/ grep scan.
 *  Cross-repo interface: also defined in dcm-agent/backup_logs/include/backup_logs.h.
 *  Any change MUST be coordinated with dcm-agent and reboot-manager. */
#define PATH_FLAG_BACKUP_LOGS_DONE      "/tmp/.backup_logs_done"
#define TELEMETRY_PREVLOGS_POLL_INTERVAL_S  1u
#define TELEMETRY_PREVLOGS_POLL_TIMEOUT_S   60u

/** Sentinel written by telemetry2_0 after PreviousLogs/ scan.
 *  Cross-repo interface: consumed by dcm-agent/uploadstblogs/reboot_setup().
 *  Any change MUST be coordinated with dcm-agent. */
#define PATH_FLAG_TELEMETRY_PREVLOGS_DONE "/tmp/.telemetry_prevlogs_done"
```

---

### TASK-G4 — Unit test: telemetry2_0 sentinel write behavior
**Repo**: `telemetry`  
**Spec**: TEST-SYNC-008  
**Depends on**: TASK-G2

Test cases:
- `TelemetrySentinel_WrittenAfterScan`: Simulate successful PreviousLogs/ grep scan.
  Assert `/tmp/.telemetry_prevlogs_done` exists after `CollectAndReport()` returns.
- `TelemetrySentinel_NotWrittenOnTimeout`: Do NOT create `.backup_logs_done`. Assert
  `.telemetry_prevlogs_done` does NOT exist after `CollectAndReport()` times out.

---

## Implementation Order Summary

```
TASK-A1 ──┐
           ├─► TASK-B1 ──► TASK-F1
TASK-A2 ──┤
           ├─► TASK-C1 ──► TASK-C2 ──► TASK-F2
           └─► TASK-D1 ──► TASK-D2 ──► TASK-F3

TASK-G3 ──┬
           ├─► TASK-G1 ──► TASK-G2 ──► TASK-G4
           └─ (depends on TASK-B1 sentinel being written)

TASK-E1, TASK-E2 (parallel, no dependencies)

TASK-F4 ──► after all Groups B, C, D, G tasks complete
```
