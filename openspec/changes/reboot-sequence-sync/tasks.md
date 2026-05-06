# Tasks: Reboot Sequence Synchronization

> Execute tasks in the order listed. Tasks within the same group can be parallelized
> across contributors. Cross-repo tasks (marked `[reboot-manager]`) apply to the
> `reboot-manager` repository.

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

### TASK-D1 — Add poll constants for reboot readiness
**Repo**: `dcm-agent`  
**File**: `uploadstblogs/include/uploadstblogs_types.h`  
**Spec**: REQ-SYNC-003

Add:

```c
/** Poll parameters for waiting on reboot reason readiness sentinel.
 *  See uploadstblogs/src/strategies.c reboot_setup(). */
#define REBOOT_POLL_INTERVAL_S    1u
#define REBOOT_POLL_TIMEOUT_S     120u
```

---

### TASK-D2 — Replace `sleep(330)` with sentinel poll in `reboot_setup()`
**Repo**: `dcm-agent`  
**File**: `uploadstblogs/src/strategies.c`  
**Spec**: REQ-SYNC-003, REQ-SYNC-004  
**Depends on**: TASK-D1

Add `wait_for_reboot_info_ready()` static helper and replace the `sleep()` block:

```c
#include <sys/stat.h>
#include <unistd.h>

static int wait_for_reboot_info_ready(void)
{
    unsigned int elapsed = 0u;
    struct stat st;

    while (elapsed < REBOOT_POLL_TIMEOUT_S) {
        if (stat(PATH_FLAG_INVOCATION, &st) == 0) {
            return 0;
        }
        sleep(REBOOT_POLL_INTERVAL_S);
        elapsed += REBOOT_POLL_INTERVAL_S;
    }
    return -1;
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
/* NEW — poll for reboot reason readiness */
LOG_INFO("Waiting for reboot reason sentinel: %s (timeout %us)",
         PATH_FLAG_INVOCATION, REBOOT_POLL_TIMEOUT_S);

if (wait_for_reboot_info_ready() != 0) {
    LOG_ERROR("Timed out after %us waiting for %s. "
              "Skipping reboot log upload.",
              REBOOT_POLL_TIMEOUT_S, PATH_FLAG_INVOCATION);
    return -1;
}

LOG_INFO("Reboot reason sentinel detected. Proceeding with archive/upload.");
```

---

## Group E — Documentation

### TASK-E1 — Update `dcm-agent/docs/DEPENDENCIES.md`
**Repo**: `dcm-agent`  
**Spec**: REQ-SYNC-006

Add a section documenting the cross-repo sentinel interface:

```markdown
## Cross-Repository Sentinel Interface

### `/tmp/.backup_logs_done`
- **Written by**: `backup_logs` binary (this repo), in `backup_logs_main()` after successful `backup_logs_execute()`.
- **Consumed by**: `reboot-manager` repository, `update-prev-reboot-info` binary.
- **Semantics**: File presence indicates that all logs have been safely moved to `/opt/logs/PreviousLogs/` and the directory is stable for reading.
- **Lifecycle**: Volatile (`/tmp/`); cleared automatically on reboot.
- **Cross-repo coordination**: Any change to this path MUST be released simultaneously with a matching change in the `reboot-manager` repository.
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

### TASK-F3 — Unit test: reboot_setup() poll and timeout behavior
**Repo**: `dcm-agent`  
**File**: `uploadstblogs/unittest/strategies_gtest.cpp` (or existing equivalent)  
**Spec**: TEST-SYNC-004, TEST-SYNC-005  
**Depends on**: TASK-D2

Test cases:
- `RebootSetup_ProceedsWhenSentinelPresent`: Create `Update_rebootInfo_invoked` before
  calling `reboot_setup()`. Assert returns 0 (success).
- `RebootSetup_SkipsUploadOnTimeout`: Do NOT create sentinel. Assert `reboot_setup()`
  returns -1 after `REBOOT_POLL_TIMEOUT_S` (use a short override for fast test execution).
- `RebootSetup_ArchiveTimestampIsPostNTP`: Stub `time()` to return a known post-NTP epoch.
  Assert archive name contains expected timestamp.

---

### TASK-F4 — L2 integration test: full boot sequence sentinel chain
**Repo**: `dcm-agent`  
**File**: `test/functional-tests/` (new feature file)  
**Spec**: TEST-SYNC-007  
**Depends on**: All Group B, C, D tasks

Verify the complete sentinel chain end-to-end in the Docker CI environment:

1. Start with a clean `/tmp/` (no sentinels).
2. Run `backup_logs` binary → assert `/tmp/.backup_logs_done` created.
3. Run `update-prev-reboot-info` → assert it reads PreviousLogs/ correctly and writes
   `/tmp/Update_rebootInfo_invoked`.
4. Run `uploadstblogs` with `TRIGGER_REBOOT` → assert it proceeds without the 330s sleep.
5. Inject failure: run `update-prev-reboot-info` without first creating `.backup_logs_done`
   → assert it exits non-zero after timeout.
6. Inject failure: run `reboot_setup()` without `Update_rebootInfo_invoked` → assert
   upload is skipped.

---

## Implementation Order Summary

```
TASK-A1 ──┐
           ├─► TASK-B1 ──► TASK-F1
TASK-A2 ──┤
           ├─► TASK-C1 ──► TASK-C2 ──► TASK-F2
           └─► TASK-D1 ──► TASK-D2 ──► TASK-F3

TASK-E1, TASK-E2 (parallel, no dependencies)

TASK-F4 ──► after all implementation tasks complete
```
