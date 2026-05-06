# Proposal: Reboot Sequence Synchronization

## User Story

> **As a** device operator and support engineer,
> **I want** log uploads after device reboot to contain the correct reboot reason and
> NTP-accurate archive timestamps,
> **so that** I can reliably correlate uploaded logs with the actual reboot event,
> diagnose the root cause without ambiguity, and trust that no upload data was silently
> discarded or misclassified due to a boot-time race condition.

### Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC-1 | `update-prev-reboot-info` NEVER reads `PreviousLogs/` before `backup_logs` has fully completed moving all log files |
| AC-2 | `uploadstblogs` NEVER starts archiving before `previousreboot.info` is written and complete |
| AC-3 | All reboot-triggered archive filenames and per-file timestamp prefixes reflect post-NTP wall-clock time |
| AC-4 | The 330-second `sleep()` is removed from `reboot_setup()`; upload begins as soon as prerequisites are met |
| AC-5 | If any prerequisite times out, the upload is skipped cleanly — no partial or mis-classified upload |
| AC-6 | `backup_logs` continues to run without any NTP dependency |
| AC-7 | All new sentinel-poll paths are covered by unit tests; the full chain is covered by an L2 integration test |

---

## Summary

Enforce a strict, sentinel-based execution order across three independent subsystems
(`backup_logs`, `reboot-manager`, `uploadstblogs`) during device boot-up so that:

1. Log backup completes before reboot-reason derivation starts.
2. Reboot reason is fully persisted before log upload is triggered.
3. Log upload archive names and file timestamps are created with NTP-synchronized time.

## Problem

On device reboot the following three processes run concurrently without any ordering
guarantees:

- **`backup_logs`** – moves live logs from `/opt/logs/` into `/opt/logs/PreviousLogs/`.
- **`update-prev-reboot-info`** (reboot-manager) – reads `PreviousLogs/` to derive the
  previous reboot reason and writes `/opt/secure/reboot/previousreboot.info`.
- **`uploadstblogs`** (dcm-agent, `TRIGGER_REBOOT`) – reads `PreviousLogs/` for archiving
  and reads `previousreboot.info` to decide whether to upload.

The absence of coordination produces three failure modes:

| Failure | Symptom |
|---------|---------|
| `update-prev-reboot-info` reads `PreviousLogs/` before `backup_logs` finishes | Incomplete or missing reboot reason in `previousreboot.info` |
| `uploadstblogs` starts before `previousreboot.info` is written | Wrong upload decision (scheduled vs unscheduled reboot gate uses stale/empty data) |
| `uploadstblogs` calls `time(NULL)` before NTP sync | Archive filenames and file timestamp prefixes carry incorrect pre-NTP wall-clock time |

The current workaround — a 330-second `sleep()` inside `reboot_setup()` — is both
unreliable (fast devices may still race) and wasteful (slow devices wait needlessly).

## Proposed Solution

Introduce a sentinel-file protocol that chains the three subsystems:

```
backup_logs
  → writes /tmp/.backup_logs_done  (on success)

update-prev-reboot-info
  → polls /tmp/.backup_logs_done  (before reading PreviousLogs)
  → writes /tmp/Update_rebootInfo_invoked  (already exists, already the completion signal)

uploadstblogs reboot_setup()
  → polls /tmp/Update_rebootInfo_invoked  (replaces sleep 330s)
  → proceeds only when sentinel appears → NTP sync is transitively guaranteed
```

Using `/tmp/` ensures sentinels are volatile (cleared on every reboot), so there is no
stale-sentinel problem across boots.

## Goals

- Guarantee log backup completes before reboot-reason derivation.
- Guarantee reboot reason is persisted before log upload starts.
- Guarantee archive timestamps use NTP-correct time, with no NTP dependency added to `backup_logs` itself.
- Remove the unreliable 330-second sleep from `uploadstblogs`.
- Keep changes minimal and platform-neutral (no new IPC, no systemd-specific logic added to C code).

## Non-Goals

- Changing the NTP synchronization mechanism itself.
- Adding NTP awareness to `backup_logs` (it must remain NTP-free by design).
- Supporting concurrent reboot-triggered uploads (single-instance lock already exists).
- Altering scheduled (`DCM_LOG_UPLOAD`) or on-demand (`TRIGGER_ONDEMAND`) upload paths.

## Affected Repositories / Modules

| Repository | Module | Change type |
|------------|--------|-------------|
| `dcm-agent` | `backup_logs/src/backup_logs.c` | Add sentinel write |
| `dcm-agent` | `uploadstblogs/src/strategies.c` (`reboot_setup`) | Replace sleep with poll |
| `reboot-manager` | `reboot-reason-fetcher/src/rebootreason_main.c` | Add sentinel poll |
| `reboot-manager` | `reboot-reason-fetcher/include/update-reboot-info.h` | Add sentinel constant |

## Success Criteria

- `update-prev-reboot-info` never reads an incomplete `PreviousLogs/` directory.
- `uploadstblogs` never starts archiving before `previousreboot.info` is written.
- All reboot-triggered archive names and file timestamp prefixes reflect post-NTP time.
- 330-second sleep removed; typical boot latency before upload reduced on fast devices.
- Existing unit and L2 tests continue to pass; new tests cover timeout and sentinel paths.
