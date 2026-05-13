# Capability Specification: Reboot Sequence Synchronization

**Change**: `reboot-sequence-sync`  
**Status**: Draft  
**Date**: 2026-05-06  

---

## 1. Overview

This spec defines the behavioral requirements for synchronizing the three boot-time subsystems
that must execute in strict order after a device reboot: `backup_logs`, `update-prev-reboot-info`
(reboot-manager), and `uploadstblogs` (dcm-agent TRIGGER_REBOOT path).

---

## 2. Sentinel File Protocol

### 2.1 Sentinel Definitions

| Sentinel File | Written by | Consumed by | Meaning |
|---------------|-----------|-------------|---------|
| `/tmp/.backup_logs_done` | `backup_logs` binary | `update-prev-reboot-info`, `telemetry2_0` (PreviousLogs grep report) | Log backup to PreviousLogs/ completed successfully || `/tmp/stt_received` | NTP daemon / time sync service | `update-prev-reboot-info`, `uploadstblogs` (reboot_setup) | System clock is NTP-synchronized || `/tmp/Update_rebootInfo_invoked` | `update-prev-reboot-info` | `uploadstblogs` (reboot_setup) | Reboot reason fully derived and persisted to previousreboot.info |

Both sentinels reside in `/tmp/` and are therefore volatile — they are cleared automatically
on every reboot. No explicit removal at startup is required.

### 2.2 Sentinel Ordering Chain

```
[backup_logs]
    Moves /opt/logs/*.log → /opt/logs/PreviousLogs/
    ON SUCCESS → touch /tmp/.backup_logs_done

        ↓ (update-prev-reboot-info polls)

[update-prev-reboot-info]
    Gates on /tmp/.backup_logs_done  ← NEW gate
    Gates on /tmp/stt_received       ← existing gate (NTP sync signal)
    Reads PreviousLogs/ to derive previousreboot.info
    ON SUCCESS → touch /tmp/Update_rebootInfo_invoked  ← existing signal

        ↓ (uploadstblogs polls)

[uploadstblogs TRIGGER_REBOOT]
    reboot_setup() polls /tmp/stt_received AND /tmp/Update_rebootInfo_invoked
    BOTH present → proceed with archive/upload
    EITHER times out → skip reboot upload entirely
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
`sleep(330)` with a polling loop that waits for **both** of the following sentinels:

1. `/tmp/stt_received` — NTP clock synchronization is complete
2. `/tmp/Update_rebootInfo_invoked` — reboot reason has been fully derived and persisted

**Poll behavior**:
- Poll interval: 1 second
- Timeout: 120 seconds (applies to the combined wait for both sentinels)
- On both sentinels detected within timeout: proceed to archive/upload phase
- On timeout (either sentinel missing): log error and return a non-zero status code that
  causes the caller to skip the upload

The poll loop MUST check `stat()` for both files on each iteration and proceed only when
both exist. This provides an explicit NTP guarantee rather than relying on transitive
inference through the sentinel chain.

The uptime check (`get_system_uptime()`) MAY be retained as an early gate (skip poll if
uptime ≥ 900s), but the dual sentinel poll MUST be the primary readiness mechanism.

**Rationale**: Checking `/tmp/stt_received` explicitly makes the NTP dependency
self-documenting and resilient to future changes in the `update-prev-reboot-info` pipeline.
Checking `/tmp/Update_rebootInfo_invoked` ensures `previousreboot.info` is written and
complete before upload decisions are made.

---

### REQ-SYNC-004 — Upload Skip on Timeout

**Priority**: MUST  
**Module**: `uploadstblogs/src/strategies.c` (`dcm-agent`)

If the poll in REQ-SYNC-003 times out (120 seconds without `/tmp/Update_rebootInfo_invoked`
appearing), `reboot_setup()` MUST:

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
MUST execute only after the poll in REQ-SYNC-003 has confirmed **both** `/tmp/stt_received`
and `/tmp/Update_rebootInfo_invoked` are present.

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

The sentinel chain MUST be strictly unidirectional:

```
backup_logs  →  update-prev-reboot-info  →  uploadstblogs
```

- `backup_logs` MUST NOT poll for or depend on any signal from `update-prev-reboot-info`
  or `uploadstblogs`.
- `update-prev-reboot-info` MUST NOT depend on anything from `uploadstblogs`.
- `uploadstblogs` (TRIGGER_REBOOT path) MUST NOT write sentinels consumed by
  `backup_logs` or `update-prev-reboot-info`.

---

## 4. Timeouts Summary

| Component | Polls for | Interval | Timeout | Timeout action |
|-----------|-----------|----------|---------|----------------|
| `update-prev-reboot-info` | `/tmp/.backup_logs_done` | 1s | 60s | Exit `ERROR_GENERAL` |
| `uploadstblogs` `reboot_setup()` | `/tmp/stt_received` AND `/tmp/Update_rebootInfo_invoked` | 1s | 120s | Skip reboot upload |

---

## 5. Error Handling

| Condition | Behavior |
|-----------|---------|
| `backup_logs_execute()` fails | Sentinel NOT written; `update-prev-reboot-info` will timeout |
| `update-prev-reboot-info` times out on `.backup_logs_done` | Exits with error; `Update_rebootInfo_invoked` is NOT written; `uploadstblogs` will timeout |
| `uploadstblogs` times out on `stt_received` or `Update_rebootInfo_invoked` | Skip upload, return failure to strategy handler |
| `previousreboot.info` absent when sentinel is present | Existing `reboot_upload()` handles gracefully (warns and may skip) |

---

## 6. Cross-Repo Interface Contract

The path `/tmp/.backup_logs_done` is a cross-repository interface between `dcm-agent`
(writer) and two consumers:

| Repo | Consumer | Purpose |
|------|----------|---------|
| `reboot-manager` | `update-prev-reboot-info` | Gates PreviousLogs/ read for reboot reason derivation |
| `telemetry` | `telemetry2_0` (`profile.c`, `PERSIST_LOG_MON_REF`) | Gates PreviousLogs/ grep marker scan for "Previous Logs" report |

Changes to this sentinel path MUST be coordinated across all three repos and released
together.

All three repos MUST document this dependency in their respective `docs/DEPENDENCIES.md`.

> **Note**: The telemetry fix is tracked as a separate change in the `telemetry` repository.
> Telemetry's `CollectAndReport()` sets `customLogPath = PREVIOUS_LOGS_PATH` on first boot
> when `checkPreviousSeek` is true. This is a one-shot report — if it reads incomplete data,
> telemetry markers from the previous boot are permanently lost. The sentinel poll prevents this.

---

## 7. Testing Requirements

| Test ID | Description | Module |
|---------|-------------|--------|
| TEST-SYNC-001 | backup_logs writes sentinel on success, absent on failure | `backup_logs` unit test |
| TEST-SYNC-002 | update-prev-reboot-info proceeds when `.backup_logs_done` present | `reboot-manager` unit test |
| TEST-SYNC-003 | update-prev-reboot-info exits with error after 60s timeout | `reboot-manager` unit test |
| TEST-SYNC-004 | reboot_setup() proceeds when both `stt_received` and `Update_rebootInfo_invoked` present | `uploadstblogs` unit test |
| TEST-SYNC-005 | reboot_setup() skips upload after 120s timeout (missing either sentinel) | `uploadstblogs` unit test |
| TEST-SYNC-006 | Archive name timestamp is post-NTP (requires mock time) | `uploadstblogs` unit test |
| TEST-SYNC-007 | Full boot sequence integration (sentinel chain E2E) | L2 test |
