# IPC Eventing Mechanism Analysis

## Context

The current design uses **stat() polling + trigger files** in `/tmp/` for inter-process
synchronization during the boot-time log upload sequence. This document evaluates whether
a better eventing mechanism exists, given the project's embedded constraints.

---

## Current Approach — `stat()` Polling + Sentinel Files

```c
/* Poll every 1 s for up to 120 s */
while (elapsed < REBOOT_POLL_TIMEOUT_S) {
    if (stat(path, &st) == 0) return 0;
    sleep(REBOOT_POLL_INTERVAL_S);
    elapsed += REBOOT_POLL_INTERVAL_S;
}
return -1;
```

**Pros**: POSIX portable, crash-safe, auto-cleared on reboot (`/tmp/`), idempotent  
**Cons**: Up to 1 s latency per poll cycle, wastes 1 Hz CPU wakeups during the wait window

---

## Option A — `inotify` + Sentinel Files (Recommended Upgrade)

Replace the `sleep(1)` + `stat()` loop with a blocking `inotify` directory watch. Keep the
sentinel files in `/tmp/` as the authoritative state.

```c
#include <sys/inotify.h>
#include <sys/select.h>
#include <libgen.h>
#include <limits.h>

/**
 * wait_for_sentinel_inotify - Block until a sentinel file appears or timeout elapses.
 * @path:      Absolute path of the sentinel file to watch.
 * @timeout_s: Maximum wait time in seconds (uses select() timeout).
 *
 * Always checks stat() before adding the watch to handle the file-already-exists race.
 * Returns 0 on success (file found), -1 on timeout or error.
 */
static int wait_for_sentinel_inotify(const char *path, unsigned int timeout_s)
{
    struct stat st;
    char dir_buf[PATH_MAX];
    char path_buf[PATH_MAX];
    const char *dir, *base;
    int ifd, wd, ret = -1;
    char event_buf[sizeof(struct inotify_event) + NAME_MAX + 1];
    struct timeval tv;
    fd_set rfds;
    ssize_t len;

    /* Fast path: sentinel already present */
    if (stat(path, &st) == 0) {
        return 0;
    }

    strncpy(dir_buf, path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';
    strncpy(path_buf, path, sizeof(path_buf) - 1);
    path_buf[sizeof(path_buf) - 1] = '\0';

    dir  = dirname(dir_buf);
    base = basename(path_buf);

    ifd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (ifd < 0) {
        return -1;  /* fall back to stat() polling at call site */
    }

    wd = inotify_add_watch(ifd, dir, IN_CREATE | IN_MOVED_TO);
    if (wd < 0) {
        close(ifd);
        return -1;
    }

    /* Re-check after watch is registered to close the TOCTOU window */
    if (stat(path, &st) == 0) {
        ret = 0;
        goto cleanup;
    }

    tv.tv_sec  = (time_t)timeout_s;
    tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(ifd, &rfds);

    if (select(ifd + 1, &rfds, NULL, NULL, &tv) > 0) {
        len = read(ifd, event_buf, sizeof(event_buf));
        (void)len;  /* event details not needed; re-check via stat() */
        ret = (stat(path, &st) == 0) ? 0 : -1;
    }

cleanup:
    inotify_rm_watch(ifd, wd);
    close(ifd);
    return ret;
}
```

**Pros**: Zero CPU during wait, sub-millisecond latency, no polling thread needed  
**Cons**: Linux-only — `inotify` is not in POSIX; not portable to non-Linux embedded targets  
**Integration**: Add `#ifdef HAVE_INOTIFY` guard; fall back to stat() polling if undefined

---

## Option B — POSIX Named Semaphores

```c
#include <semaphore.h>
#include <time.h>
#include <fcntl.h>

/* Writer (e.g., backup_logs): */
sem_t *sem = sem_open("/backup_logs_done", O_CREAT | O_RDWR, 0644, 0);
sem_post(sem);
sem_close(sem);

/* Waiter (e.g., uploadstblogs): */
sem_t *sem = sem_open("/backup_logs_done", O_CREAT, 0644, 0);
struct timespec abs_timeout;
clock_gettime(CLOCK_REALTIME, &abs_timeout);
abs_timeout.tv_sec += REBOOT_POLL_TIMEOUT_S;
sem_timedwait(sem, &abs_timeout);
sem_close(sem);
sem_unlink("/backup_logs_done");  /* Must be cleaned up explicitly */
```

**Pros**: POSIX portable, zero-CPU blocking wait, no filesystem polling  
**Cons**: Semaphores survive process crashes (`/dev/shm/` or equivalent); manual `sem_unlink()`
required; not inherently idempotent; if the writer crashes after `sem_post()` but before
signalling completion, the semaphore is stuck — no equivalent of "file exists or not"

---

## Option C — POSIX Message Queues

```c
#include <mqueue.h>

/* Writer: */
mqd_t mq = mq_open("/boot_events", O_CREAT | O_WRONLY, 0644, &attr);
mq_send(mq, "backup_logs_done", 16, 0);
mq_close(mq);

/* Reader: */
mqd_t mq = mq_open("/boot_events", O_RDONLY, 0644, NULL);
mq_timedreceive(mq, buf, sizeof(buf), NULL, &abs_timeout);
mq_close(mq);
mq_unlink("/boot_events");
```

**Pros**: Push-based, kernel-buffered, can carry typed payload, POSIX portable  
**Cons**: Requires explicit `mq_unlink()` cleanup; queue persists until unlinked; more
complex attribute configuration; not inherently idempotent across readers

---

## Option D — RBUS Events (already in dcm-agent)

`dcm-agent` already uses RBUS (`dcm_rbus.c`). The same infrastructure could publish
boot-time sentinel events:

```c
/* Publisher (backup_logs): */
rbus_set(handle, "Device.DeviceInfo.X_RDKCENTRAL-COM_BackupLogsDone", true);

/* Subscriber (uploadstblogs): */
rbus_subscribe(handle, "Device.DeviceInfo.X_RDKCENTRAL-COM_BackupLogsDone", cb, NULL, 0, NULL);
```

**Pros**: Already in the RDK stack, proper event-driven model, integrates with TR-181 data
model, no extra IPC infrastructure needed  
**Cons**: **RBUS daemon (`rbusd`) must be running** — risky during early boot before `rbusd`
is initialized; adds a hard runtime dependency on RBUS for transient boot-time signals;
cross-repo coordination needed for data model element names; not available on non-RDK targets

---

## Option E — IARM Bus Events (already used in uploadstblogs)

`uploadstblogs` already uses IARM Bus via `event_manager.c` to send `MAINT_LOGUPLOAD_COMPLETE`
and `MAINT_LOGUPLOAD_ERROR` to the MaintenanceManager. `dcmd.service` declares
`After=iarmbusd.service`, so the IARM daemon is guaranteed up before `dcmd` starts.

```c
/* Publisher (e.g., backup_logs — would need new IARM dependency): */
IARM_Bus_Init("backup_logs");
IARM_Bus_Connect();
IARM_Bus_BroadcastEvent("DCM", DCM_IARM_EVENT_BACKUP_LOGS_DONE, &data, sizeof(data));
IARM_Bus_Disconnect();
IARM_Bus_Term();

/* Subscriber (uploadstblogs — already has IARM, just add a new handler): */
IARM_Bus_RegisterEventHandler("DCM", DCM_IARM_EVENT_BACKUP_LOGS_DONE, on_backup_logs_done);
```

**Pros**: IARM is already linked in `uploadstblogs`; `iarmbusd` is guaranteed running before
`dcmd`; no new IPC library dependencies for the consumer side; event payload can carry status codes  
**Cons**:

1. **Fire-and-forget — events are lost if no subscriber is registered at broadcast time.**
   In the boot sequence, `backup_logs` is a short-lived process. If it fires the event before
   `uploadstblogs` has called `IARM_Bus_RegisterEventHandler()`, the event is permanently lost
   and `uploadstblogs` blocks forever. There is no "replay" or "query current state" in IARM.

2. **`backup_logs` has no IARM dependency today.** Adding `IARM_Bus_Init()` /
   `IARM_Bus_Connect()` to `backup_logs` couples a previously dependency-free binary to
   `iarmbusd`. If `iarmbusd` is slow to start, `backup_logs` would stall waiting for the
   bus — delaying the entire log backup for an IPC concern that belongs only to the upload phase.

3. **Not crash-safe in the sentinel sense.** If `backup_logs` crashes before broadcasting,
   no sentinel is ever set and `uploadstblogs` has no way to distinguish "event not yet fired"
   from "event missed". The stat()-on-file model makes this unambiguous.

4. **`update-prev-reboot-info` (reboot-manager) and `telemetry2_0` (telemetry repo) do not
   use IARM at all.** Adding IARM to both repos purely for this synchronization would be a
   significant cross-repo coupling with no other benefit.

**Verdict**: IARM Bus is the right mechanism for signalling **completion status to the
MaintenanceManager** (which is already done). It is the wrong mechanism for **boot-time
process synchronization** where producer lifetime and consumer registration order are
not coordinated.

---

## Comparison Matrix

| Mechanism | CPU during wait | Latency | POSIX portable | Linux only | Crash-safe | Boot-safe | Cleanup needed | Complexity |
|-----------|:--------------:|:-------:|:--------------:|:----------:|:----------:|:---------:|:--------------:|:----------:|
| `stat()` poll (current) | Low (1 Hz) | ~1 s | ✅ | ❌ | ✅ | ✅ | None | Low |
| `inotify` + sentinel files | Zero | <1 ms | ❌ | ✅ | ✅ | ✅ | None | Low |
| Named semaphore | Zero | <1 ms | ✅ | ❌ | ❌ | ✅ | `sem_unlink()` | Medium |
| POSIX message queue | Zero | <1 ms | ✅ | ❌ | ❌ | ✅ | `mq_unlink()` | Medium |
| RBUS events | Zero | <1 ms | ❌ | ❌ | ⚠️ | ❌ early boot | None | High |
| IARM Bus events | Zero | <1 ms | ❌ | ❌ | ❌ lost-event | ⚠️ order-dep | None | Medium |

---

## Decision

### Recommended: `inotify` with `stat()` polling fallback

Keep sentinel files in `/tmp/` as the authoritative state (idempotent, auto-cleared on reboot,
crash-safe). Replace the `sleep(1)` + `stat()` loop with `inotify` + `select()` for the wait.

**Rationale**:
1. All current and planned target platforms are embedded Linux → `inotify` is always available.
2. Sentinel files in `/tmp/` retain all safety properties of the current design.
3. Zero CPU during the wait window (up to 120 s) — significant on constrained platforms.
4. Sub-millisecond response latency vs. worst-case 1 s with polling.
5. No new dependencies, no cleanup burden, no daemon dependency.
6. `stat()` fallback via `#ifdef HAVE_INOTIFY` preserves portability if ever needed.

### What does NOT change

- Sentinel file paths and constants — unchanged, all cross-repo interfaces remain stable.
- Trigger file mechanism (`TRIGGER_REBOOT_INFO_UPDATE`, `TRIGGER_TELEMETRY_SCAN`) — unchanged.
- Timeout values (`REBOOT_POLL_TIMEOUT_S = 120 s`) — unchanged; now enforced by `select()` timeout.
- Bitmask return and annotate-and-proceed semantics — unchanged.
- Maintenance Manager timer compatibility — **changed**: the LogUpload task is being removed from the Maintenance Manager (see REQ-SYNC-011 in spec.md). The IARM `MAINT_LOGUPLOAD_COMPLETE` / `MAINT_LOGUPLOAD_ERROR` broadcasts in `uploadstblogs` will be removed.

### Implementation note

The `inotify` watch must be registered **before** the final `stat()` check to avoid the
TOCTOU race (file created between the initial `stat()` and `inotify_add_watch()`). The
reference implementation in Option A above handles this correctly with a post-registration
`stat()` re-check.
