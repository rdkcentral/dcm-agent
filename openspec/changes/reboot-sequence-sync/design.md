# Design: Reboot Sequence Synchronization

## 1. Architecture

### 1.1 Current State (Problematic)

```
╔══════════════════════════╗      ╔══════════════════════════════╗      ╔════════════════════════════╗
║     backup_logs          ║      ║   update-prev-reboot-info    ║      ║  uploadstblogs REBOOT      ║
║ (moves logs to           ║      ║ (derives previousreboot.info ║      ║ (archives & uploads        ║
║  PreviousLogs/)          ║      ║  from PreviousLogs/)         ║      ║  PreviousLogs to server)   ║
╚══════════════════════════╝      ╚══════════════════════════════╝      ╚════════════════════════════╝
         │                                    │                                      │
         │  ← no ordering guarantee →         │  ← reads unfinished PreviousLogs/   │
         │                                    │                                      │
         │                                    │        ← sleep(330s) proxy →         │
         │                                    │                                      │
         │                                    │    reads previousreboot.info         │
         │                                    │    (may not exist yet)               │
```

### 1.2 Target State (Sentinel Chain)

```
[Boot]
  │
  ├─► backup_logs ──────────────────────────────────────────────────────────────────
  │     backup_logs_init()            [clears nothing — /tmp/ is already volatile]
  │     backup_logs_execute() → OK
  │     open("/tmp/.backup_logs_done", O_CREAT|O_WRONLY, 0644)   ← NEW
  │     close(fd)
  │     return EXIT_SUCCESS
  │
  ├─► update-prev-reboot-info (reboot-manager) ────────────────────────────────────
  │     acquire process lock
  │     update_reboot_info()               [polls stt_received — existing]
  │     poll_sentinel("/tmp/.backup_logs_done", 1s, 60s)   ← NEW
  │       found → continue
  │       timeout → log error, exit(ERROR_GENERAL)
  │     find_previous_reboot_log()         [reads PreviousLogs/ safely]
  │     parse reboot info → write previousreboot.info
  │     touch /tmp/Update_rebootInfo_invoked   [existing]
  │
  └─► uploadstblogs TRIGGER_REBOOT ────────────────────────────────────────────────
        reboot_setup():
          [optional] if uptime >= 900s → skip poll (already done)
          poll_sentinel("/tmp/Update_rebootInfo_invoked", 1s, 120s)   ← REPLACES sleep(330)
            found → proceed with archive/upload
            timeout → log error, return -1 (skip upload entirely)
        reboot_archive():
          generate_archive_name()  ← time(NULL) is now NTP-correct
          add_timestamp_to_files() ← time(NULL) is now NTP-correct
        reboot_upload():
          reads previousreboot.info ← guaranteed to exist
        reboot_cleanup():
          removes PreviousLogs/ staging
```

---

## 2. Detailed Component Changes

### 2.1 `backup_logs` — `backup_logs/src/backup_logs.c`

**Location**: `backup_logs_main()`, after the `backup_logs_execute()` call.

**Current code** (simplified):
```c
ret = backup_logs_execute(&ctx);
if (ret != BACKUP_SUCCESS) {
    LOG_ERROR("backup_logs_execute failed: %d", ret);
    backup_logs_cleanup(&ctx);
    return EXIT_FAILURE;
}
backup_logs_cleanup(&ctx);
return EXIT_SUCCESS;
```

**New code**:
```c
#include "backup_logs.h"   /* defines BACKUP_LOGS_DONE_FLAG */
#include <fcntl.h>
#include <sys/stat.h>

ret = backup_logs_execute(&ctx);
if (ret != BACKUP_SUCCESS) {
    LOG_ERROR("backup_logs_execute failed: %d", ret);
    backup_logs_cleanup(&ctx);
    return EXIT_FAILURE;
}

/* Write completion sentinel for downstream consumers (e.g., update-prev-reboot-info).
 * /tmp/ is volatile so no stale-sentinel risk across reboots. */
{
    int sentinel_fd = open(BACKUP_LOGS_DONE_FLAG, O_CREAT | O_WRONLY, 0644);
    if (sentinel_fd < 0) {
        LOG_WARN("Failed to create sentinel %s: %s", BACKUP_LOGS_DONE_FLAG, strerror(errno));
        /* Non-fatal: downstream will timeout and skip gracefully */
    } else {
        close(sentinel_fd);
    }
}

backup_logs_cleanup(&ctx);
return EXIT_SUCCESS;
```

**Header change** — `backup_logs/include/backup_logs.h`:
```c
/** Sentinel written by backup_logs after successful completion.
 *  Cross-repo interface: also referenced by reboot-manager's update-prev-reboot-info.
 *  Any path change MUST be coordinated with the reboot-manager repository. */
#define BACKUP_LOGS_DONE_FLAG  "/tmp/.backup_logs_done"
```

---

### 2.2 `reboot-manager` — `reboot-reason-fetcher/src/rebootreason_main.c`

**Add a static poll helper** (can go in a shared utility, or inline in the translation unit):

```c
/**
 * poll_for_sentinel - Wait for a sentinel file to appear.
 * @path:     File path to poll for.
 * @interval: Poll interval in seconds.
 * @timeout:  Maximum time to wait in seconds.
 *
 * Returns  0 when the sentinel is found.
 * Returns -1 on timeout.
 */
static int poll_for_sentinel(const char *path, unsigned int interval,
                              unsigned int timeout)
{
    unsigned int elapsed = 0;
    struct stat st;

    while (elapsed < timeout) {
        if (stat(path, &st) == 0) {
            return 0;           /* sentinel found */
        }
        sleep(interval);
        elapsed += interval;
    }
    return -1;                  /* timeout */
}
```

**Call site** — before `find_previous_reboot_log()`:

```c
/* Gate: wait for backup_logs to complete before reading PreviousLogs/ */
if (poll_for_sentinel(PATH_FLAG_BACKUP_LOGS_DONE,
                      BACKUP_LOGS_POLL_INTERVAL_S,
                      BACKUP_LOGS_POLL_TIMEOUT_S) != 0) {
    LOG_ERROR("Timed out waiting for %s after %us. "
              "backup_logs may have failed. Exiting.",
              PATH_FLAG_BACKUP_LOGS_DONE, BACKUP_LOGS_POLL_TIMEOUT_S);
    return ERROR_GENERAL;
}
```

**Header change** — `reboot-reason-fetcher/include/update-reboot-info.h`:

```c
/** Sentinel written by backup_logs (dcm-agent) on successful completion.
 *  Cross-repo interface: path is also defined in backup_logs/include/backup_logs.h.
 *  Any change here MUST be coordinated with dcm-agent. */
#define PATH_FLAG_BACKUP_LOGS_DONE     "/tmp/.backup_logs_done"
#define BACKUP_LOGS_POLL_INTERVAL_S    1u
#define BACKUP_LOGS_POLL_TIMEOUT_S     60u
```

---

### 2.3 `uploadstblogs` — `uploadstblogs/src/strategies.c` — `reboot_setup()`

**Current code** (relevant portion):
```c
long uptime_secs = get_system_uptime();
if (uptime_secs != -1 && uptime_secs < 900) {
    long sleep_duration = 330 - (long)(uptime_secs / 3);
    if (sleep_duration > 0) {
        LOG_INFO("System uptime low (%lds). Sleeping %lds before upload.", ...);
        sleep((unsigned int)sleep_duration);
    }
}
```

**New code**:
```c
#include <sys/stat.h>
#include <unistd.h>

/* REBOOT_POLL_INTERVAL_S and REBOOT_POLL_TIMEOUT_S defined in uploadstblogs_types.h
 * or strategies.h to avoid magic numbers. */

static int wait_for_reboot_info_ready(void)
{
    unsigned int elapsed = 0;
    struct stat st;

    while (elapsed < REBOOT_POLL_TIMEOUT_S) {
        if (stat(PATH_FLAG_INVOCATION, &st) == 0) {
            return 0;           /* /tmp/Update_rebootInfo_invoked found */
        }
        sleep(REBOOT_POLL_INTERVAL_S);
        elapsed += REBOOT_POLL_INTERVAL_S;
    }
    return -1;                  /* timeout */
}

/* In reboot_setup(): */
if (wait_for_reboot_info_ready() != 0) {
    LOG_ERROR("Timed out after %us waiting for reboot reason readiness (%s). "
              "Skipping reboot upload.",
              REBOOT_POLL_TIMEOUT_S, PATH_FLAG_INVOCATION);
    return -1;
}
```

**Constants** (add to `uploadstblogs/include/uploadstblogs_types.h` or a dedicated header):
```c
#define REBOOT_POLL_INTERVAL_S    1u
#define REBOOT_POLL_TIMEOUT_S     120u
```

`PATH_FLAG_INVOCATION` is already defined in `update-reboot-info.h` as
`"/tmp/Update_rebootInfo_invoked"`. Include that header or duplicate the constant locally
with a comment referencing the source.

---

## 3. NTP Timestamp Correctness (No Code Change Required)

The sentinel chain already guarantees NTP correctness for archive timestamps:

```
stt_received  (NTP sync complete)
    ↓  [update-reboot-info.service: ConditionPathExists=/tmp/stt_received]
update-prev-reboot-info starts
    ↓  [polls .backup_logs_done, then reads PreviousLogs/]
writes Update_rebootInfo_invoked
    ↓  [reboot_setup() poll succeeds]
reboot_archive() → generate_archive_name()  → time(NULL) is NTP-correct
                 → add_timestamp_to_files() → time(NULL) is NTP-correct
```

`backup_logs` runs independently and pre-NTP; its `time(NULL)` calls (if any) in the copy
phase are irrelevant because `backup_logs` does not generate archive names or file timestamp
prefixes.

---

## 4. Polling Implementation Notes

### 4.1 Why `stat()` Instead of `inotify`

- `inotify` is Linux-specific and not available on all embedded targets.
- `stat()` with a 1-second sleep is idiomatic in this codebase (mirrors the existing
  `waitForFile()` pattern used in `dcm_utils.c`).
- `stat()` is defined in POSIX and portable to all target platforms.

### 4.2 `sleep()` Accuracy on Embedded Systems

On constrained platforms `sleep(1)` may wake slightly late. This is acceptable; the cumulative
drift over 60–120 iterations is bounded and does not affect correctness.

### 4.3 Thread Safety

- The helper functions `wait_for_reboot_info_ready()` and `poll_for_sentinel()` are
  stateless (no global state). They are safe to call from any thread context.
- `perm_log_path_storage[]` static in `strategies.c` is not used in `reboot_setup()`'s poll
  path and is therefore not a concern for this change.

### 4.4 Signal Handling / Interruption

Both poll loops use `sleep()` which can be interrupted by signals (`EINTR`). In the context
of a short-lived daemon that does not register custom signal handlers during setup, this is
acceptable. If `sleep()` is interrupted, the loop will re-evaluate `stat()` on the next
iteration, which may slightly reduce the effective timeout — this is tolerable.

---

## 5. Error Propagation

```
backup_logs fails
  → .backup_logs_done NOT written
  → update-prev-reboot-info polls for 60s, times out
  → update-prev-reboot-info exits ERROR_GENERAL
  → Update_rebootInfo_invoked NOT written
  → uploadstblogs reboot_setup() polls for 120s, times out
  → uploadstblogs skips reboot upload, logs error
  → device continues boot; next reboot will retry normally
```

This is the safest degradation path: no partial upload, no upload with incorrect data.

---

## 6. Interface Contract Documentation

The path `/tmp/.backup_logs_done` crosses a repository boundary. Both repositories MUST
document this in their `docs/DEPENDENCIES.md`:

**In `dcm-agent/docs/DEPENDENCIES.md`**:
> `backup_logs` writes `/tmp/.backup_logs_done` upon successful completion. This sentinel
> is consumed by `reboot-manager/reboot-reason-fetcher`. Any change to this path requires
> coordinated release with `reboot-manager`.

**In `reboot-manager/docs/DEPENDENCIES.md`** (or equivalent):
> `update-prev-reboot-info` polls `/tmp/.backup_logs_done` before reading `PreviousLogs/`.
> This sentinel is written by `dcm-agent/backup_logs`. Any change to this path requires
> coordinated release with `dcm-agent`.
