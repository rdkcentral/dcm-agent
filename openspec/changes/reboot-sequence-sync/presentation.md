---
marp: true
theme: default
paginate: true
header: "reboot-sequence-sync | dcm-agent"
footer: "Confidential — Internal Review"
---

<!-- _paginate: false -->
<!-- _header: "" -->
<!-- _footer: "" -->

# Reboot Sequence Synchronization

### Enforcing Strict Boot-Time Execution Order Across dcm-agent and reboot-manager

**Change**: `reboot-sequence-sync`
**Date**: 2026-05-06
**Status**: Proposal — Team Review

---

## Agenda

1. Problem Statement
2. Current Architecture — Why It Breaks
3. Root Cause Analysis
4. Proposed Solution — Sentinel Chain
5. Key Design Decisions
6. Component Changes (What Changes, Where)
7. NTP Timestamp Correctness
8. Error & Timeout Behavior
9. Testing Strategy
10. Task Breakdown & Timeline
11. Questions / Discussion

---

## 1. Problem Statement

**User Story**

> *As a device operator, I want log upload after reboot to use the correct reboot reason
> and NTP-accurate timestamps, so that support teams can reliably correlate uploaded logs
> with the actual reboot event.*

**The Gap Today**

Three independent boot-time subsystems run **without any ordering guarantee**:

| Subsystem | What it does |
|-----------|-------------|
| `backup_logs` | Moves `/opt/logs/*.log` → `/opt/logs/PreviousLogs/` |
| `update-prev-reboot-info` | Reads `PreviousLogs/` to derive reboot reason; writes `previousreboot.info` |
| `uploadstblogs` (REBOOT) | Archives `PreviousLogs/` and uploads to log server |

They **race** at boot — and the race has real consequences.

---

## 2. Current Architecture — The Race

```
[Boot event]
      │
      ├──────────────────────────────────────────────────────────────┐
      │                                                              │
      ▼                          ▼                                   ▼
 backup_logs             update-prev-reboot-info            uploadstblogs (REBOOT)
      │                          │                                   │
      │  moving logs...          │  find_previous_reboot_log()  ◄── sleep(330s)
      │                          │    ⚠ reads PreviousLogs/          │
      │                          │      while backup running!        │  reads
      │  (still running) ───────►│                                   │  previousreboot.info
      │                          │  writes previousreboot.info  ◄────┤  (may not exist!)
      ▼                          ▼                                   ▼
  PreviousLogs/ complete    wrong/incomplete              wrong upload decision,
                            reboot reason                 pre-NTP timestamp in
                                                          archive filename
```

**The 330-second sleep is the only "guard" — and it's not a guard at all.**

---

## 3. Root Cause — Three Synchronization Gaps

### Gap 1 — Backup vs. Reboot Reason Derivation (A → B)

`update-prev-reboot-info` calls `find_previous_reboot_log()` which searches
`PreviousLogs/` for the `last_reboot` marker.

**Risk**: If `backup_logs` is still mid-copy, `last_reboot` may not exist yet,
or the directory may be partially populated. The wrong reboot reason gets recorded.

---

### Gap 2 — Reboot Reason vs. Upload (B → C)

`uploadstblogs` reads `previousreboot.info` in `reboot_upload()` to decide
whether the reboot was scheduled (DCM-triggered) or unscheduled.

**Risk**: `previousreboot.info` may not exist yet, or may contain stale data
from the prior boot. Wrong data → wrong upload classification.

---

### Gap 3 — NTP Sync vs. Archive Timestamp

`generate_archive_name()` and `add_timestamp_to_files()` both call `time(NULL)`.

**Risk**: If `uploadstblogs` races ahead of NTP synchronization, archive filenames
like `AA-BB_Logs_11-25-25-02-30PM.tgz` carry pre-NTP (often epoch or BIOS) time.

**Current workaround**: `sleep(330s)` if uptime < 900s — crude, wrong direction.

```
⚠ 330s sleep is:
  • Too long on fast/modern devices  →  needlessly delays uploads
  • Too short on slow/loaded devices →  NTP may still not be done
  • Not event-driven                 →  fundamentally unreliable
```

---

## 4. Proposed Solution — Sentinel Chain

Use **sentinel files in `/tmp/`** (volatile, cleared on every reboot) to chain
the three subsystems into a strict, event-driven execution order.

```
  [Boot]
     │
     ▼
  backup_logs
     │  moves /opt/logs/* → /opt/logs/PreviousLogs/
     │  (NTP-FREE — runs before NTP sync, by design)
     │
     └──► touch /tmp/.backup_logs_done       ◄── NEW sentinel

                    │
                    │  (update-prev-reboot-info polls, 1s interval, 60s timeout)
                    ▼

  update-prev-reboot-info (reboot-manager)
     │  already gates on /tmp/stt_received   (NTP sync complete — existing)
     │  NEW: also gates on /tmp/.backup_logs_done
     │
     │  find_previous_reboot_log() → PreviousLogs/ is now stable ✓
     │  parse → write previousreboot.info
     │
     └──► touch /tmp/Update_rebootInfo_invoked   ◄── existing sentinel, now used!

                    │
                    │  (uploadstblogs polls, 1s interval, 120s timeout)
                    │  replaces sleep(330s)
                    ▼

  uploadstblogs (TRIGGER_REBOOT)
     │  previousreboot.info is present and correct ✓
     │  time(NULL) is NTP-accurate ✓  (guaranteed transitively)
     │
     ├── reboot_archive()  → generate_archive_name()  ← NTP-correct timestamp ✓
     ├── reboot_upload()   → previousreboot.info read ← correct reboot reason ✓
     └── reboot_cleanup()
```

---

## 5. Key Design Decisions

| # | Decision | Choice | Rationale |
|---|----------|--------|-----------|
| Q1 | Poll timeout for uploadstblogs | **120 seconds** | Shorter than 330s; event-driven so fast devices proceed immediately |
| Q2 | Poll interval | **1 second** | Matches existing codebase pattern; low CPU overhead |
| Q3 | Timeout behavior (uploadstblogs) | **Skip upload entirely** | Safe degradation; no partial or mis-classified upload |
| Q4 | Sentinel location | **`/tmp/`** | Volatile — auto-cleared on reboot; no stale-sentinel risk |
| Q5 | Timeout behavior (reboot-manager) | **Exit with error** | Prevents deriving wrong reboot reason from partial PreviousLogs/ |
| Q6 | Spec scope | **Single spec** | Both repos serve the same boot-time pipeline; one spec = one source of truth |
| Q7 | NTP requirement for backup_logs | **NTP-free** | backup_logs must run as early as possible; NTP is only needed for archive *naming* (satisfied transitively) |

---

## 6. What Changes, Where

### 6.1 `dcm-agent` — `backup_logs/src/backup_logs.c`

After `backup_logs_execute()` returns `BACKUP_SUCCESS`:

```c
int fd = open("/tmp/.backup_logs_done", O_CREAT | O_WRONLY, 0644);
if (fd >= 0) close(fd);
/* Non-fatal if creation fails — downstream will timeout gracefully */
```

**New constant** in `backup_logs/include/backup_logs.h`:
```c
#define BACKUP_LOGS_DONE_FLAG  "/tmp/.backup_logs_done"
```

**Sentinel written**: only on success. Not written on failure.

---

### 6.2 `reboot-manager` — `rebootreason_main.c`

New poll loop **before** `find_previous_reboot_log()`:

```
while elapsed < 60s:
    if stat("/tmp/.backup_logs_done") == 0 → proceed
    sleep(1s)

if timeout:
    LOG_ERROR("backup_logs did not complete. Aborting.")
    exit(ERROR_GENERAL)
```

**New constants** in `update-reboot-info.h`:
```c
#define PATH_FLAG_BACKUP_LOGS_DONE    "/tmp/.backup_logs_done"
#define BACKUP_LOGS_POLL_INTERVAL_S   1u
#define BACKUP_LOGS_POLL_TIMEOUT_S    60u
```

> `/tmp/.backup_logs_done` is a **cross-repo interface contract**. Path changes must be coordinated.

---

### 6.3 `dcm-agent` — `uploadstblogs/src/strategies.c` — `reboot_setup()`

**Remove** (330-second sleep block):
```c
// ❌ DELETE THIS
if (uptime_secs < 900) {
    sleep(330 - uptime_secs/3);
}
```

**Replace with** deterministic sentinel poll:
```
while elapsed < 120s:
    if stat("/tmp/Update_rebootInfo_invoked") == 0 → proceed
    sleep(1s)

if timeout:
    LOG_ERROR("Reboot reason not ready. Skipping reboot upload.")
    return -1   ← strategy handler skips archive/upload/cleanup
```

`PATH_FLAG_INVOCATION = "/tmp/Update_rebootInfo_invoked"` — already defined.

---

## 7. NTP Timestamp Correctness (No Extra Code Needed)

The sentinel chain already **transitively guarantees** NTP sync before any `time(NULL)` call
in the upload path:

```
NTP completes
    │
    └──► writes /tmp/stt_received

                │  [update-reboot-info.service:
                │   ConditionPathExists=/tmp/stt_received]
                ▼

        update-prev-reboot-info runs
            polls .backup_logs_done → reads PreviousLogs/ → writes previousreboot.info
            └──► touches /tmp/Update_rebootInfo_invoked

                        │  [uploadstblogs polls]
                        ▼

                reboot_setup() poll succeeds
                    │
                    ├── generate_archive_name()   → time(NULL) ✓ NTP-correct
                    └── add_timestamp_to_files()  → time(NULL) ✓ NTP-correct
```

**Note on TAR mtime**: File modification times inside the archive reflect the time
`backup_logs` *copied* the files (pre-NTP). This is expected and acceptable — only
the **archive filename** and **per-file prefix** need to be NTP-accurate.

---

## 8. Error & Timeout Behavior

### Failure Propagation

```
backup_logs fails
  → .backup_logs_done NOT written
  → update-prev-reboot-info polls 60s → TIMEOUT → exits ERROR_GENERAL
  → Update_rebootInfo_invoked NOT written
  → uploadstblogs polls 120s → TIMEOUT → skips upload, logs error
  → device continues boot normally
  → next reboot: fresh /tmp/, protocol resets cleanly
```

### Timeout Summary

| Subsystem | Polls for | Interval | Timeout | On Timeout |
|-----------|-----------|----------|---------|------------|
| `update-prev-reboot-info` | `/tmp/.backup_logs_done` | 1s | 60s | Exit `ERROR_GENERAL` |
| `uploadstblogs` `reboot_setup()` | `/tmp/Update_rebootInfo_invoked` | 1s | 120s | Skip reboot upload |

**Worst-case boot penalty**: 60s + 120s = 180s — only when backup or reboot-info derivation
fails. Happy path: poll resolves within seconds of sentinel creation.

---

## 9. Why `stat()` and Not `inotify`?

| Criterion | `inotify` | `stat()` polling |
|-----------|-----------|-----------------|
| POSIX portability | ❌ Linux-only | ✅ All target platforms |
| Embedded resource cost | Higher (kernel watch descriptors) | Minimal |
| Already in codebase | No | ✅ Pattern used in `dcm_utils.c` |
| Complexity | Higher (event loop, fd management) | Simple loop |
| Accuracy | Event-exact | ±1s (acceptable) |

**Decision**: `stat()` polling, consistent with existing codebase patterns.

---

## 10. Testing Strategy

### Unit Tests

| Test | What it verifies | Module |
|------|-----------------|--------|
| TEST-SYNC-001 | Sentinel written on success, absent on failure | `backup_logs` |
| TEST-SYNC-002 | `update-prev-reboot-info` proceeds when sentinel present | `reboot-manager` |
| TEST-SYNC-003 | `update-prev-reboot-info` exits error after 60s timeout | `reboot-manager` |
| TEST-SYNC-004 | `reboot_setup()` proceeds when sentinel present | `uploadstblogs` |
| TEST-SYNC-005 | `reboot_setup()` skips upload after 120s timeout | `uploadstblogs` |
| TEST-SYNC-006 | Archive name timestamp is post-NTP (mock `time()`) | `uploadstblogs` |

### L2 Integration Test (Docker CI)

| Step | Verify |
|------|--------|
| Run `backup_logs` | `.backup_logs_done` created |
| Run `update-prev-reboot-info` | Reads PreviousLogs/ correctly, writes `Update_rebootInfo_invoked` |
| Run `uploadstblogs` REBOOT | No 330s sleep; proceeds immediately after sentinel |
| Inject failure (no `.backup_logs_done`) | `update-prev-reboot-info` exits error |
| Inject timeout (no `Update_rebootInfo_invoked`) | Upload skipped cleanly |

---

## 11. Task Breakdown

### Groups (parallel where noted)

```
TASK-A1  Define BACKUP_LOGS_DONE_FLAG in backup_logs.h           [dcm-agent]
TASK-A2  Define PATH_FLAG_BACKUP_LOGS_DONE in update-reboot-info.h [reboot-manager]
    │
    ├── TASK-B1  Write sentinel in backup_logs_main()             [dcm-agent]
    │
    ├── TASK-C1  Add poll_for_sentinel() helper                   [reboot-manager]
    │   TASK-C2  Gate find_previous_reboot_log() on sentinel      [reboot-manager]
    │
    └── TASK-D1  Add REBOOT_POLL_TIMEOUT_S constant               [dcm-agent]
        TASK-D2  Replace sleep(330) with sentinel poll            [dcm-agent]

TASK-E1  Update dcm-agent/docs/DEPENDENCIES.md                   [dcm-agent]
TASK-E2  Add reboot-manager/docs/DEPENDENCIES.md entry           [reboot-manager]

TASK-F1  Unit tests — backup_logs sentinel                        [dcm-agent]
TASK-F2  Unit tests — reboot-manager poll behavior                [reboot-manager]
TASK-F3  Unit tests — uploadstblogs reboot_setup()               [dcm-agent]
TASK-F4  L2 integration test — full sentinel chain E2E            [dcm-agent]
```

**Total**: 14 tasks across 2 repositories, 6 groups

---

## 12. What We Are NOT Changing

To set clear scope expectations:

| Area | In scope? | Note |
|------|-----------|------|
| `TRIGGER_DCM` upload path | ❌ No | Only REBOOT trigger has the race |
| `TRIGGER_ONDEMAND` path | ❌ No | User-triggered; not a boot-sequence concern |
| Scheduled log upload | ❌ No | Different code path entirely |
| NTP subsystem | ❌ No | We consume its signal, don't change it |
| Systemd service files | ❌ No | Existing `.service` / `.path` units unchanged |
| `backup_logs` copy logic | ❌ No | Only sentinel write added at end |

---

## 13. Summary

**Before**:
- Three concurrent processes with no ordering
- 330-second sleep as the only "guard"
- Race conditions on `PreviousLogs/` directory and `previousreboot.info`
- Possible pre-NTP archive timestamps

**After**:
- Deterministic, event-driven execution chain
- `backup_logs` → signals done → `update-prev-reboot-info` → signals done → `uploadstblogs`
- `backup_logs` remains NTP-free
- `uploadstblogs` gets NTP-correct time transitively, automatically
- 330-second sleep removed; fast devices upload sooner
- Clean, auditable failure mode: timeout → skip, not corrupt upload

**Minimal change surface**: 3 C source files, 2 headers, 1 new sentinel path constant.
Pattern is identical to existing sentinels already in both codebases.

---

<!-- _paginate: false -->
<!-- _header: "" -->

## Questions & Discussion

```
sentinel chain recap:

  backup_logs
    └──► /tmp/.backup_logs_done          (NEW — dcm-agent)
              │
  update-prev-reboot-info
    ├── polls .backup_logs_done          (NEW — reboot-manager)
    └──► /tmp/Update_rebootInfo_invoked  (existing — reboot-manager)
              │
  uploadstblogs reboot_setup()
    └── polls Update_rebootInfo_invoked  (replaces sleep(330) — dcm-agent)
```

**Artifacts**: `openspec/changes/reboot-sequence-sync/`
- `proposal.md` — What and why
- `spec.md` — Formal requirements (REQ-SYNC-001 through REQ-SYNC-007)
- `design.md` — Code changes, diagrams, rationale
- `tasks.md` — 14 implementation tasks with dependencies
