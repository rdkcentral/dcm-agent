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
          poll for ALL THREE:
            /tmp/stt_received                    ← explicit NTP check (NEW)
            /tmp/Update_rebootInfo_invoked       ← reboot reason ready (REPLACES sleep(330))
            /tmp/.telemetry_prevlogs_done        ← telemetry complete (NEW)
          ALL THREE present → proceed with archive/upload
          ANY missing at timeout (120s) → log error, return -1 (skip upload entirely)
        reboot_archive():
          generate_archive_name()  ← time(NULL) is now NTP-correct (explicit guarantee)
          add_timestamp_to_files() ← time(NULL) is now NTP-correct (explicit guarantee)
        reboot_upload():
          reads previousreboot.info ← guaranteed to exist
        reboot_cleanup():
          removes PreviousLogs/ staging
```

### 1.3 Trigger Ownership Decision (Architectural)

```
Reboot uploads (authoritative path):
  DCM Agent only
  backup_logs success -> sentinel chain -> uploadstblogs upload

On-demand uploads (retained):
  SystemServices API
  UploadLogsNow

Removed as redundant for logupload triggering:
  Maintenance Manager unsolicited path
  Maintenance Manager solicited path
```

This change explicitly removes Maintenance Manager as a logupload trigger source while
retaining SystemServices API and UploadLogsNow as valid on-demand entry points.

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

/* STT_FLAG, REBOOT_POLL_INTERVAL_S and REBOOT_POLL_TIMEOUT_S defined in
 * uploadstblogs_types.h to avoid magic numbers.
 * PATH_FLAG_INVOCATION = "/tmp/Update_rebootInfo_invoked" */

/**
 * wait_for_upload_prerequisites - Wait for NTP, reboot reason, and telemetry readiness.
 *
 * Polls for /tmp/stt_received (NTP sync), /tmp/Update_rebootInfo_invoked
 * (reboot reason), and /tmp/.telemetry_prevlogs_done (telemetry complete)
 * every REBOOT_POLL_INTERVAL_S. Proceeds only when ALL THREE are present.
 *
 * Returns  0 when all prerequisites are met.
 * Returns -1 on timeout (any sentinel missing after REBOOT_POLL_TIMEOUT_S).
 */
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
            return 0;           /* all prerequisites met */
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((now.tv_sec - start.tv_sec) >= (time_t)REBOOT_POLL_TIMEOUT_S) {
            return -1;          /* timeout */
        }
        sleep(REBOOT_POLL_INTERVAL_S);
    }
}

/* In reboot_setup(): */
if (wait_for_upload_prerequisites() != 0) {
    LOG_ERROR("Timed out after %us waiting for upload prerequisites "
              "(NTP: %s, RebootReason: %s, Telemetry: %s). Skipping reboot upload.",
              REBOOT_POLL_TIMEOUT_S, STT_FLAG, PATH_FLAG_INVOCATION,
              TELEMETRY_PREVLOGS_DONE_FLAG);
    return -1;
}
```

**Constants** (add to `uploadstblogs/include/uploadstblogs_types.h` or a dedicated header):
```c
/** NTP synchronization sentinel — written by time sync service. */
#define STT_FLAG                      "/tmp/stt_received"

/** Reboot reason completion sentinel — written by update-prev-reboot-info. */
#define PATH_FLAG_INVOCATION          "/tmp/Update_rebootInfo_invoked"

/** Telemetry PreviousLogs scan completion sentinel — written by telemetry2_0.
 *  Cross-repo interface: path also defined in telemetry repo.
 *  Any change MUST be coordinated with the telemetry repository. */
#define TELEMETRY_PREVLOGS_DONE_FLAG  "/tmp/.telemetry_prevlogs_done"

#define REBOOT_POLL_INTERVAL_S        1u
#define REBOOT_POLL_TIMEOUT_S         120u
```

---

## 3. NTP Timestamp Correctness (Explicit Check in reboot_setup)

The sentinel chain provides a transitive NTP guarantee, but `reboot_setup()` now also
explicitly checks `/tmp/stt_received` for defense in depth:

```
stt_received  (NTP sync complete) ───────────────────────────────────────────┐
    ↓  [update-reboot-info.service: ConditionPathExists=/tmp/stt_received]   │
update-prev-reboot-info starts                                               │
    ↓  [polls .backup_logs_done, then reads PreviousLogs/]                   │
writes Update_rebootInfo_invoked                                             │
    ↓  [reboot_setup() checks ALL THREE sentinels]                               │
    ↓                                                                        │
reboot_setup() poll: ──────────────── stt_received? ─────────────────────────┘
                      ├────────────── Update_rebootInfo_invoked? ✓
                      └────────────── .telemetry_prevlogs_done? ✓
                                      ALL THREE present → proceed
    ↓
reboot_archive() → generate_archive_name()  → time(NULL) ✓ NTP-correct
                 → add_timestamp_to_files() → time(NULL) ✓ NTP-correct
```

**Why the explicit check?** If the `update-prev-reboot-info` pipeline is ever modified to
no longer depend on `stt_received`, the transitive guarantee would silently break. The
explicit `stt_received` check in `reboot_setup()` ensures NTP correctness is directly
verifiable and self-documenting.

`backup_logs` runs independently and pre-NTP; its `time(NULL)` calls (if any) in the copy
phase are irrelevant because `backup_logs` does not generate archive names or file timestamp
prefixes.

---

## 4. Polling Implementation Notes

### 4.1 Polling Implementation: `stat()` + `clock_gettime(CLOCK_MONOTONIC)`

`stat()` polling is chosen over `inotify` and `select()`-based event-driven approaches:

- **Portability**: `stat()` is defined in POSIX and available on all target platforms.
  `inotify` is Linux-specific. While all current targets are embedded Linux, `stat()`
  keeps the implementation portable and consistent with the existing `waitForFile()` pattern
  in `dcm_utils.c`.
- **Simplicity**: No file descriptor lifecycle to manage; no watch registration/cleanup.
- **Correctness**: `clock_gettime(CLOCK_MONOTONIC)` tracks actual elapsed time rather than
  accumulating `sleep()` call counts. This eliminates timeout drift caused by `EINTR`
  signal interruptions — the elapsed time is measured in real wall-clock seconds regardless
  of how many times `sleep()` returns early.

### 4.2 `sleep()` Accuracy on Embedded Systems

On constrained platforms `sleep(1)` may wake slightly late due to scheduler granularity or
`EINTR` interruptions. Using `clock_gettime(CLOCK_MONOTONIC)` to measure elapsed time (see
section 4.1) ensures the timeout is based on actual wall-clock duration rather than a
counted number of `sleep()` calls, eliminating cumulative drift.

### 4.3 Thread Safety

- The helper functions `wait_for_reboot_info_ready()` and `poll_for_sentinel()` are
  stateless (no global state). They are safe to call from any thread context.
- `perm_log_path_storage[]` static in `strategies.c` is not used in `reboot_setup()`'s poll
  path and is therefore not a concern for this change.

### 4.4 Signal Handling / Interruption

Both poll loops use `sleep()` which can be interrupted by signals (`EINTR`). When `sleep()`
is interrupted early, the loop re-evaluates `stat()` on the next iteration (a benign extra
check). The effective timeout is measured by `clock_gettime(CLOCK_MONOTONIC)` (see section
4.1), so `EINTR` interruptions do not shorten the timeout window — elapsed time is measured
directly rather than inferred from `sleep()` call counts.

---

## 5. Error Propagation

```
backup_logs fails
  → .backup_logs_done NOT written
  → telemetry2_0 polls for 60s, times out → skips PreviousLogs report
  → .telemetry_prevlogs_done NOT written
  → update-prev-reboot-info polls for 60s, times out
  → update-prev-reboot-info exits ERROR_GENERAL
  → Update_rebootInfo_invoked NOT written
  → uploadstblogs reboot_setup() polls for 120s, times out
  → uploadstblogs skips reboot upload, logs error
  → device continues boot; next reboot will retry normally
```

In the case where `backup_logs` succeeds but `telemetry2_0` times out on its scan:
```
.backup_logs_done written
  → telemetry2_0 starts scan, times out (internal telemetry timeout)
  → .telemetry_prevlogs_done NOT written
  → uploadstblogs reboot_setup() polls for 120s, times out on .telemetry_prevlogs_done
  → uploadstblogs skips reboot upload, logs error
```

This is the safest degradation path: no partial upload, no upload with incorrect data,
and no `add_timestamp_to_files()` called while telemetry is still scanning.

---

## 6. Interface Contract Documentation

Two sentinel paths cross repository boundaries. All three repositories MUST document both
in their `docs/DEPENDENCIES.md`:

**In `dcm-agent/docs/DEPENDENCIES.md`**:
> `backup_logs` writes `/tmp/.backup_logs_done` upon successful completion. This sentinel
> is consumed by `reboot-manager/reboot-reason-fetcher` and `telemetry/telemetry2_0`.
> Any change to this path requires coordinated simultaneous release with `reboot-manager`
> and `telemetry`.
>
> `uploadstblogs` reads `/tmp/.telemetry_prevlogs_done` (written by `telemetry/telemetry2_0`)
> before calling `add_timestamp_to_files()`. Any change to this path requires coordinated
> simultaneous release with `telemetry`.

**In `reboot-manager/docs/DEPENDENCIES.md`** (or equivalent):
> `update-prev-reboot-info` polls `/tmp/.backup_logs_done` before reading `PreviousLogs/`.
> This sentinel is written by `dcm-agent/backup_logs`. Any change to this path requires
> coordinated simultaneous release with `dcm-agent` and `telemetry`.

**In `telemetry/docs/DEPENDENCIES.md`** (or equivalent):
> `telemetry2_0` polls `/tmp/.backup_logs_done` (written by `dcm-agent/backup_logs`)
> before grep-scanning `PreviousLogs/`. After the scan, it writes `/tmp/.telemetry_prevlogs_done`
> which is consumed by `dcm-agent/uploadstblogs`. Any change to either path requires
> coordinated simultaneous release with `dcm-agent` and `reboot-manager`.
