# Capability Specification: Reboot Sequence Synchronization

**Change**: `Logupload-Synchronization`  
**Status**: Draft  
**Date**: 2026-05-06  

---

## 1. Overview

This spec defines the behavioral requirements for synchronizing the four boot-time subsystems
that must execute in coordinated order after a device reboot: `backup_logs`, `telemetry2_0`
(telemetry), `update-prev-reboot-info` (reboot-manager), and `uploadstblogs` (dcm-agent
TRIGGER_REBOOT path).

---

## 2. Sentinel File Protocol

### 2.1 Sentinel Definitions

| Sentinel File | Written by | Consumed by | Meaning |
|---------------|-----------|-------------|------|
| `/tmp/.backup_logs_done` | `backup_logs` binary | `update-prev-reboot-info`, `telemetry2_0`, `uploadstblogs` | Log backup to PreviousLogs/ completed successfully |
| `/tmp/stt_received` | NTP daemon / time sync service | `update-prev-reboot-info`, `uploadstblogs` (reboot_setup) | System clock is NTP-synchronized — **stable interface, must not be removed** |
| `/tmp/Update_rebootInfo_invoked` | `update-prev-reboot-info` | `uploadstblogs` (reboot_setup) | Reboot reason fully derived and persisted to previousreboot.info |
| `/tmp/.telemetry_prevlogs_done` | `telemetry2_0` | `uploadstblogs` (reboot_setup) | PreviousLogs/ grep scan complete; safe for uploadstblogs to rename and archive files |

All sentinels reside in `/tmp/` and are therefore volatile — they are cleared automatically

### 2.2 Sentinel Ordering Chain

```
[backup_logs]
    Moves /opt/logs/*.log → /opt/logs/PreviousLogs/
    ON SUCCESS → touch /tmp/.backup_logs_done

        ↓ (parallel: telemetry2_0 AND update-prev-reboot-info both poll)

[telemetry2_0]  ─────────────────────────────────────────────────────────────
    Gates on /tmp/.backup_logs_done      ← NEW gate
    Grep-scans PreviousLogs/ for telemetry markers (one-shot)
    ON COMPLETION → touch /tmp/.telemetry_prevlogs_done  ← NEW sentinel

[update-prev-reboot-info]  ──────────────────────────────────────────────────
    Gates on /tmp/.backup_logs_done      ← NEW gate
    Gates on /tmp/stt_received           ← existing gate (NTP sync signal)
    Reads PreviousLogs/ to derive previousreboot.info
    ON SUCCESS → touch /tmp/Update_rebootInfo_invoked  ← existing signal

        ↓ (uploadstblogs polls — waits for ALL THREE)

[uploadstblogs TRIGGER_REBOOT]
    reboot_setup() polls:
      /tmp/stt_received                ← NTP sync
      /tmp/Update_rebootInfo_invoked   ← reboot reason ready
      /tmp/.telemetry_prevlogs_done    ← telemetry done with PreviousLogs/  ← NEW
    ALL THREE present → proceed with add_timestamp_to_files(), archive, upload
    ANY times out (120s) → skip reboot upload entirely
```

---

## 3. Requirements

### REQ-SYNC-001 — backup_logs Completion Sentinel

**Priority**: MUST  
**Module**: `backup_logs` (`dcm-agent`)

The `backup_logs` binary MUST write the file `/tmp/.backup_logs_done` after
`backup_logs_execute()` returns `BACKUP_SUCCESS`. It MUST NOT write the sentinel if
`backup_logs_execute()` returns any error code.

The sentinel MUST be created using an atomic filesystem operation (e.g., `open()` + `close()`
with `O_CREAT`) to prevent partial writes being observed.

`backup_logs` MUST NOT have any dependency on NTP synchronization. It runs NTP-free by design.

**Rationale**: Provides a reliable completion signal that downstream subsystems can gate on,
replacing the timing-based assumptions currently implicit in the 330s sleep.

---

### REQ-SYNC-002 — update-prev-reboot-info Gates on backup_logs Sentinel

**Priority**: MUST  
**Module**: `reboot-reason-fetcher` (`reboot-manager`)

The `update-prev-reboot-info` binary MUST poll for `/tmp/.backup_logs_done` before calling
`find_previous_reboot_log()` or any other operation that reads from `PreviousLogs/`.

**Poll behavior**:
- Poll interval: 1 second
- Timeout: 60 seconds
- On sentinel detected within timeout: proceed normally
- On timeout: log error and exit with `ERROR_GENERAL` (non-zero exit code)

The polling MUST NOT block the lock acquisition; the sentinel check MUST occur after the
process lock is acquired and after `update_reboot_info()` flag-gating passes.

**Rationale**: `PreviousLogs/` may be incomplete (files still being moved) if
`update-prev-reboot-info` runs before `backup_logs` finishes. Polling the sentinel ensures
the directory is stable before it is read.

---

### REQ-SYNC-003 — uploadstblogs reboot_setup() Polls for All Prerequisites

**Priority**: MUST  
**Module**: `uploadstblogs/src/strategies.c` (`dcm-agent`)

The `reboot_setup()` function in the REBOOT/NON_DCM strategy MUST replace the existing
`sleep(330)` with a polling loop that waits for **all three** of the following sentinels:

1. `/tmp/stt_received` — NTP clock synchronization is complete
2. `/tmp/Update_rebootInfo_invoked` — reboot reason has been fully derived and persisted
3. `/tmp/.telemetry_prevlogs_done` — `telemetry2_0` has finished grep-scanning `PreviousLogs/`

**Poll behavior**:
- Poll interval: 1 second
- Timeout: 120 seconds (applies to the combined wait for all three sentinels)
- On all three sentinels detected within timeout: proceed to archive/upload phase
- On timeout (any sentinel missing): log error and return a non-zero status code that
  causes the caller to skip the upload

The poll loop MUST check `stat()` for all three files on each iteration and proceed only
when all three exist. The telemetry sentinel MUST be confirmed before
`add_timestamp_to_files()` is called in `reboot_setup()`, as that function renames files
in `PreviousLogs/` and would corrupt an in-progress telemetry grep scan.

The uptime check (`get_system_uptime()`) MAY be retained as an early gate (skip poll if
uptime ≥ 900s), but the triple sentinel poll MUST be the primary readiness mechanism.

**Rationale**: `telemetry2_0` grep-scans `PreviousLogs/` for markers in a one-shot,
never-retried operation. `reboot_setup()` calls `add_timestamp_to_files()` which renames
every file in `PreviousLogs/` — if this runs while telemetry is still scanning, markers
are permanently lost. Checking `/tmp/stt_received` explicitly makes the NTP dependency
self-documenting. Checking `/tmp/Update_rebootInfo_invoked` ensures `previousreboot.info`
is written and complete before upload decisions are made.

---

### REQ-SYNC-004 — Upload Skip on Timeout

**Priority**: MUST  
**Module**: `uploadstblogs/src/strategies.c` (`dcm-agent`)

If the poll in REQ-SYNC-003 times out (120 seconds without all three sentinels present),
`reboot_setup()` MUST:

1. Emit an error log message indicating the timeout and that the reboot upload is being skipped.
2. Return `-1` so that `strategy_handler` treats the setup phase as failed and skips
   archive, upload, and cleanup phases.

The device MUST NOT attempt a partial upload with potentially incorrect timestamps or
missing reboot reason data.

---

### REQ-SYNC-005 — NTP-Correct Archive Timestamps

**Priority**: MUST  
**Module**: `uploadstblogs/src/archive_manager.c`, `uploadstblogs/src/file_operations.c` (`dcm-agent`)

All calls to `time(NULL)` within `generate_archive_name()` and `add_timestamp_to_files()`
MUST execute only after the poll in REQ-SYNC-003 has confirmed **all three** sentinels
(`/tmp/stt_received`, `/tmp/Update_rebootInfo_invoked`, and `/tmp/.telemetry_prevlogs_done`)
are present.

This is guaranteed structurally: both functions are called in `reboot_archive()` and
`reboot_setup()` respectively, which execute after `reboot_setup()` returns successfully.
The explicit `/tmp/stt_received` check in REQ-SYNC-003 provides a direct NTP guarantee.

**Note on file mtime**: The `st_mtime` values embedded in TAR headers by `write_tar_header()`
reflect the on-disk modification time of files copied by `backup_logs` (which runs
pre-NTP). This is acceptable — only the archive name and the timestamp prefix added to
filenames in `add_timestamp_to_files()` are required to be NTP-accurate.

---

### REQ-SYNC-006 — Sentinel Constants Must Be Defined in Headers

**Priority**: SHOULD  
**Module**: `backup_logs/include/backup_logs.h` and `reboot-reason-fetcher/include/update-reboot-info.h`

The sentinel path `/tmp/.backup_logs_done` MUST be defined as a named constant in the
`backup_logs` header and in the `update-reboot-info.h` header:

```c
/* backup_logs/include/backup_logs.h */
#define BACKUP_LOGS_DONE_FLAG "/tmp/.backup_logs_done"

/* reboot-reason-fetcher/include/update-reboot-info.h */
#define PATH_FLAG_BACKUP_LOGS_DONE "/tmp/.backup_logs_done"
```

Both must resolve to the same path. Documentation MUST note that this is a cross-repo
interface contract.

---

### REQ-SYNC-007 — No Circular Dependencies

**Priority**: MUST  
**Scope**: Architecture constraint

The sentinel dependency graph MUST be strictly acyclic (no circular waits):

```
backup_logs  →  { telemetry2_0, update-prev-reboot-info }  →  uploadstblogs
```

- `backup_logs` MUST NOT poll for or depend on any signal from `telemetry2_0`,
  `update-prev-reboot-info`, or `uploadstblogs`.
- `telemetry2_0` and `update-prev-reboot-info` MAY run in parallel; neither MUST depend
  on the other.
- `update-prev-reboot-info` MUST NOT depend on anything from `uploadstblogs`.
- `uploadstblogs` (TRIGGER_REBOOT path) MUST NOT write sentinels consumed by
  `backup_logs`, `telemetry2_0`, or `update-prev-reboot-info`.

---

### REQ-SYNC-008 — telemetry2_0 Writes PreviousLogs Completion Sentinel

**Priority**: MUST  
**Module**: `telemetry2_0` (`profile.c`, `telemetry` repo)

`telemetry2_0` MUST write the file `/tmp/.telemetry_prevlogs_done` after completing its
`PreviousLogs/` grep scan (the one-shot `PERSIST_LOG_MON_REF` / `checkPreviousSeek` path
in `CollectAndReport()`).

The sentinel MUST be written:
- After all grep patterns for the Previous Logs telemetry report have been evaluated.
- Regardless of whether any markers were found (success or empty scan).
- NOT if the telemetry process exits before completing the scan.

The sentinel MUST be created using an atomic filesystem operation (`open()` + `close()` with
`O_CREAT`) to prevent partial writes being observed.

**Rationale**: `uploadstblogs` calls `add_timestamp_to_files()` in `reboot_setup()`, which
renames every file in `PreviousLogs/`. If this rename happens while `telemetry2_0` is still
scanning, grep patterns referencing original filenames will silently fail to match, and
telemetry markers from the previous boot are permanently lost (one-shot, never retried).

---

### REQ-SYNC-009 — `/tmp/stt_received` Is a Stable Cross-Repo Interface

**Priority**: MUST  
**Scope**: Architecture constraint

The sentinel `/tmp/stt_received` (written by the NTP sync service) is a stable cross-repo
interface consumed by both `update-prev-reboot-info` (reboot-manager) and `uploadstblogs`
(dcm-agent). It MUST NOT be removed, renamed, or relocated without coordinated changes
across all consumers.

Any proposal to change `/tmp/stt_received` MUST identify and update all repos that read this
path, and MUST be released atomically across those repos.

**Rationale**: `uploadstblogs reboot_setup()` explicitly checks `stt_received` for defense
in depth — even if the `update-prev-reboot-info` pipeline is changed to not gate on NTP,
the upload side retains its direct NTP correctness guarantee. Removing `stt_received`
without updating `uploadstblogs` would silently break archive timestamp correctness.

---

### REQ-SYNC-010 — Atomic Three-Repo Release

**Priority**: MUST  
**Scope**: Release constraint

The sentinel path `/tmp/.telemetry_prevlogs_done` is a cross-repository interface between
`telemetry` (writer) and `dcm-agent/uploadstblogs` (consumer). The following changes MUST
be released simultaneously across all three repositories:

| Repo | Change |
|------|--------|
| `dcm-agent` | `backup_logs` writes `/tmp/.backup_logs_done`; `uploadstblogs` polls all three sentinels |
| `reboot-manager` | `update-prev-reboot-info` polls `/tmp/.backup_logs_done` |
| `telemetry` | `telemetry2_0` polls `/tmp/.backup_logs_done`; writes `/tmp/.telemetry_prevlogs_done` |

A partial deployment (only some repos updated) MUST be treated as a known risk:
- If `telemetry` is not updated: `uploadstblogs` will time out waiting for
  `/tmp/.telemetry_prevlogs_done` and skip the reboot upload entirely.
- If `reboot-manager` is not updated: `Update_rebootInfo_invoked` will not be written
  within the sentinel window; `uploadstblogs` will time out and skip upload.

**There is no backward compatibility window.** All three repos must be deployed together.

---

## 4. Timeouts Summary

| Component | Polls for | Interval | Timeout | Timeout action |
|-----------|-----------|----------|---------|----------------|
| `telemetry2_0` | `/tmp/.backup_logs_done` | 1s | 60s | Skip PreviousLogs report; do NOT write `.telemetry_prevlogs_done` |
| `update-prev-reboot-info` | `/tmp/.backup_logs_done` | 1s | 60s | Exit `ERROR_GENERAL`; do NOT write `Update_rebootInfo_invoked` |
| `uploadstblogs` `reboot_setup()` | `/tmp/stt_received` AND `/tmp/Update_rebootInfo_invoked` AND `/tmp/.telemetry_prevlogs_done` | 1s | 120s | Skip reboot upload entirely |

---

## 5. Error Handling

| Condition | Behavior |
|-----------|---------|
| `backup_logs_execute()` fails | `.backup_logs_done` NOT written; `telemetry2_0` and `update-prev-reboot-info` both timeout |
| `telemetry2_0` times out on `.backup_logs_done` | Skips PreviousLogs report; `.telemetry_prevlogs_done` NOT written; `uploadstblogs` will timeout |
| `update-prev-reboot-info` times out on `.backup_logs_done` | Exits with error; `Update_rebootInfo_invoked` NOT written; `uploadstblogs` will timeout |
| `uploadstblogs` times out on any sentinel | Skip upload entirely, return failure to strategy handler |
| `previousreboot.info` absent when sentinel is present | Existing `reboot_upload()` handles gracefully (warns and may skip) |

---

## 6. Cross-Repo Interface Contract

Two sentinel paths cross repository boundaries. All changes to these paths require a
coordinated, simultaneous release across all affected repositories (see REQ-SYNC-010).

### `/tmp/.backup_logs_done`

Written by `dcm-agent` (`backup_logs`). Consumed by:

| Repo | Consumer | Purpose |
|------|----------|---------|
| `reboot-manager` | `update-prev-reboot-info` | Gates PreviousLogs/ read for reboot reason derivation |
| `telemetry` | `telemetry2_0` (`profile.c`, `PERSIST_LOG_MON_REF`) | Gates PreviousLogs/ grep marker scan before one-shot report |

### `/tmp/.telemetry_prevlogs_done`

Written by `telemetry` (`telemetry2_0`). Consumed by:

| Repo | Consumer | Purpose |
|------|----------|---------|
| `dcm-agent` | `uploadstblogs` (`reboot_setup`) | Gates `add_timestamp_to_files()` and all subsequent archive/upload phases |

All three repositories MUST document these dependencies in their respective `docs/DEPENDENCIES.md`.

> **Telemetry context**: `telemetry2_0`'s `CollectAndReport()` sets
> `customLogPath = PREVIOUS_LOGS_PATH` on first boot when `checkPreviousSeek` is true.
> This is a one-shot report — if it reads incomplete data or data whose filenames have been
> mutated by `add_timestamp_to_files()`, telemetry markers from the previous boot are
> permanently lost. The dual-sentinel protocol (reading `.backup_logs_done` before scan,
> writing `.telemetry_prevlogs_done` after) prevents both hazards.

---

## 7. Testing Requirements

| Test ID | Description | Module |
|---------|-------------|--------|
| TEST-SYNC-001 | backup_logs writes `.backup_logs_done` on success, absent on failure | `backup_logs` unit test |
| TEST-SYNC-002 | update-prev-reboot-info proceeds when `.backup_logs_done` present | `reboot-manager` unit test |
| TEST-SYNC-003 | update-prev-reboot-info exits with error after 60s timeout on `.backup_logs_done` | `reboot-manager` unit test |
| TEST-SYNC-004 | reboot_setup() proceeds only when all three sentinels present | `uploadstblogs` unit test |
| TEST-SYNC-005 | reboot_setup() skips upload after 120s timeout (any sentinel missing) | `uploadstblogs` unit test |
| TEST-SYNC-006 | Archive name timestamp is post-NTP (requires mock time) | `uploadstblogs` unit test |
| TEST-SYNC-007 | Full boot sequence integration (4-sentinel chain E2E including telemetry) | L2 test |
| TEST-SYNC-008 | telemetry2_0 writes `.telemetry_prevlogs_done` after scan, not on timeout | `telemetry` unit test |
| TEST-SYNC-009 | reboot_setup() skips upload when `.telemetry_prevlogs_done` absent | `uploadstblogs` unit test |
