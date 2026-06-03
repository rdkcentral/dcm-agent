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
| AC-1 | Log backup completes successfully before reboot reason update starts |
| AC-2 | Reboot reason is fully updated and persisted before any log cleanup or log upload is triggered |
| AC-3 | Log Upload Tarfile must be created with NTP-synchronized time to maintain accurate timestamps |
| AC-4 | Log upload only occurs after reboot reason update is confirmed |
| AC-5 | The 330-second `sleep()` is replaced with explicit checks for `/tmp/stt_received` (NTP), `/tmp/Update_rebootInfo_invoked` (reboot reason), and `/tmp/.telemetry_prevlogs_done` (telemetry scan complete) |
| AC-6 | **Log backup failure is the only hard gate** — if `backup_logs` fails, all subsequent steps are aborted and upload is skipped with no annotation |
| AC-11 | If `stt_received` times out, `uploadstblogs` checks internet connectivity; if internet is available, it uses last known good time from `systemtimemgr`; if internet is also unavailable, upload proceeds with error annotation — upload is never skipped due to NTP timeout alone |
| AC-12 | If `Update_rebootInfo_invoked` or `.telemetry_prevlogs_done` times out, upload proceeds with annotation of the missing metadata — upload is never skipped due to these soft prerequisites timing out |
| AC-7 | `backup_logs` continues to run without any NTP dependency |
| AC-8 | All new sentinel-poll paths are covered by unit tests |
| AC-9 | The full sentinel chain is covered by an L2 integration test |
| AC-10 | `telemetry2_0` completes its one-shot `PreviousLogs/` grep scan and writes `/tmp/.telemetry_prevlogs_done` before `uploadstblogs` calls `add_timestamp_to_files()` or any other operation that mutates filenames in `PreviousLogs/` |
| AC-13 | If `Update_rebootInfo_invoked` is absent at the start of the poll window, `uploadstblogs` writes a trigger file (`/tmp/.trigger_reboot_info_update`) to signal `update-prev-reboot-info` to retry before the timeout annotation path is taken |
| AC-14 | If `.telemetry_prevlogs_done` is absent at the start of the poll window, `uploadstblogs` writes a trigger file (`/tmp/.trigger_telemetry_prevlogs_scan`) to signal `telemetry2_0` to run its `PreviousLogs/` scan before the timeout annotation path is taken |
| AC-15 | The MaintenanceManager per-task watchdog (`TASK_TIMEOUT = 3600 s`) encompasses the maximum sentinel poll window (`REBOOT_POLL_TIMEOUT_S = 120 s`) plus upload time; no `TASK_TIMEOUT` adjustment is required, and the sentinel chain does not risk a spurious `MAINT_LOGUPLOAD_ERROR` from the watchdog |

---

## Summary

Enforce a strict, sentinel-based execution order across four independent subsystems
(`backup_logs`, `telemetry2_0`, `reboot-manager`, `uploadstblogs`) during device boot-up so that:

1. Log backup completes successfully before reboot-reason derivation or telemetry scanning starts.
2. `telemetry2_0` completes its one-shot `PreviousLogs/` grep scan before `uploadstblogs` renames or archives any files.
3. Reboot reason is fully updated and persisted before log upload begins when possible; if unavailable after timeout, upload proceeds with annotation.
4. Log upload tarfile is created with NTP-synchronized time when available; if NTP is unavailable, `systemtimemgr` last-known time is used; all fallbacks are annotated.
5. **Log upload always proceeds if log backup succeeded** — missing metadata is annotated, never silently discarded.

## Problem

On device reboot the following three processes run concurrently without any ordering
guarantees:

- **`backup_logs`** – moves live logs from `/opt/logs/` into `/opt/logs/PreviousLogs/`.
- **`update-prev-reboot-info`** (reboot-manager) – reads `PreviousLogs/` to derive the
  previous reboot reason and writes `/opt/secure/reboot/previousreboot.info`.
- **`uploadstblogs`** (dcm-agent, `TRIGGER_REBOOT`) – reads `PreviousLogs/` for archiving
  and reads `previousreboot.info` to decide whether to upload.
- **`telemetry2_0`** (telemetry repo, `PERSIST_LOG_MON_REF`) – grep-scans `PreviousLogs/`
  for telemetry markers on first boot. This is a one-shot report; if it reads incomplete
  data **or data whose filenames have been mutated by `add_timestamp_to_files()`**, markers
  are permanently lost for that boot cycle. Fix coordinated in the telemetry repo —
  it will poll `/tmp/.backup_logs_done` before reading `PreviousLogs/`, and write
  `/tmp/.telemetry_prevlogs_done` when done.

The absence of coordination produces four failure modes:

| Failure | Symptom |
|---------|---------|
| `update-prev-reboot-info` reads `PreviousLogs/` before `backup_logs` finishes | Incomplete or missing reboot reason in `previousreboot.info` |
| `uploadstblogs` starts before `previousreboot.info` is written | Wrong upload decision (scheduled vs unscheduled reboot gate uses stale/empty data) |
| `uploadstblogs` calls `time(NULL)` before NTP sync | Archive filenames and file timestamp prefixes carry incorrect pre-NTP wall-clock time |
| `telemetry2_0` grep-scans `PreviousLogs/` before `backup_logs` finishes | Missing telemetry markers in "Previous Logs" report — one-shot, never retried |
| `uploadstblogs` calls `add_timestamp_to_files()` while `telemetry2_0` is scanning | Telemetry grep patterns fail to match renamed files; markers permanently lost |

The current workaround — a 330-second `sleep()` inside `reboot_setup()` — is both
unreliable (fast devices may still race) and wasteful (slow devices wait needlessly).

## Proposed Solution

Introduce a sentinel-file protocol that chains the subsystems, with active triggering for services that may not have started:

```
backup_logs
  → writes /tmp/.backup_logs_done  (on success — HARD GATE)

telemetry2_0  [parallel with update-prev-reboot-info]
  → polls /tmp/.backup_logs_done  (before grep-scanning PreviousLogs/)
  → polls /tmp/.trigger_telemetry_prevlogs_scan  (written by uploadstblogs if missing — NEW)
  → writes /tmp/.telemetry_prevlogs_done  (after scan completes)  ← NEW

update-prev-reboot-info
  → polls /tmp/.backup_logs_done  (before reading PreviousLogs)
  → polls /tmp/.trigger_reboot_info_update  (written by uploadstblogs if missing — NEW)
  → writes /tmp/Update_rebootInfo_invoked  (already exists, already the completion signal)

uploadstblogs reboot_setup()  ── TRIGGER-THEN-POLL pattern ──
  STEP 1: Write trigger files for any missing prerequisites
      /tmp/Update_rebootInfo_invoked absent? → write /tmp/.trigger_reboot_info_update
      /tmp/.telemetry_prevlogs_done absent?  → write /tmp/.trigger_telemetry_prevlogs_scan
  STEP 2: Poll all three soft-gate sentinels (120 s total)
      poll /tmp/stt_received              (NTP sync — soft gate)
          missing after 120s → check internet
                               → internet up:   use systemtimemgr last-known time + annotate NTP_FALLBACK
                               → internet down: use time(NULL) as-is + annotate NTP_UNAVAILABLE
      poll /tmp/Update_rebootInfo_invoked  (reboot reason ready — replaces sleep 330s)
          missing after 120s → proceed with annotation REBOOT_REASON_UNAVAILABLE
      poll /tmp/.telemetry_prevlogs_done   (telemetry PreviousLogs done — NEW)
          missing after 120s → proceed with annotation TELEMETRY_UNAVAILABLE
  STEP 3: Always proceed with archive + upload, annotating any unmet prerequisites

NOTE: backup_logs failure is the ONLY hard gate. All other timeouts produce annotated uploads.
      MaintenanceManager watchdog (3600 s) safely contains the 120 s poll window.
```

Using `/tmp/` ensures sentinels are volatile (cleared on every reboot), so there is no
stale-sentinel problem across boots.

## Goals

- Guarantee log backup completes before reboot-reason derivation or telemetry scanning.
- Guarantee `telemetry2_0` finishes its one-shot `PreviousLogs/` grep scan before `uploadstblogs` renames or archives any files.
- Actively trigger `update-prev-reboot-info` and `telemetry2_0` if their sentinels are absent at upload time, giving them a second chance to complete within the poll window.
- Ensure reboot reason is persisted before log upload when possible; annotate upload if unavailable after timeout.
- Use NTP-correct time for archive timestamps when available; fall back to `systemtimemgr` last-known time; annotate if neither is available.
- **Always produce an upload if log backup succeeded** — no silent data loss.
- Remove the unreliable 330-second sleep from `uploadstblogs`.
- Keep changes minimal and platform-neutral (trigger mechanism uses sentinel files only; no new IPC beyond `open()`, no systemd-specific logic in C code).

## Non-Goals

- Changing the NTP synchronization mechanism itself.
- Adding NTP awareness to `backup_logs` (it must remain NTP-free by design).
- Supporting concurrent reboot-triggered uploads (single-instance lock already exists).
- Altering scheduled (`DCM_LOG_UPLOAD`) or on-demand (`TRIGGER_ONDEMAND`) upload paths.
- Providing a backward compatibility window for partial deployments — all three repositories (`dcm-agent`, `reboot-manager`, `telemetry`) MUST be deployed simultaneously.

## Affected Repositories / Modules

| Repository | Module | Change type |
|------------|--------|-------------|
| `dcm-agent` | `backup_logs/src/backup_logs.c` | Add sentinel write |
| `dcm-agent` | `uploadstblogs/src/strategies.c` (`reboot_setup`) | Replace sleep with trigger-then-poll (triple-sentinel) |
| `dcm-agent` | `uploadstblogs/include/uploadstblogs_types.h` | Add trigger + sentinel constants |
| `reboot-manager` | `reboot-reason-fetcher/src/rebootreason_main.c` | Add sentinel poll + trigger file watch |
| `reboot-manager` | `reboot-reason-fetcher/include/update-reboot-info.h` | Add sentinel + trigger constants |
| `telemetry` | `telemetry2_0/src/profile.c` (`CollectAndReport`) | Poll `.backup_logs_done`; watch trigger file; write `.telemetry_prevlogs_done` |
| `entservices-maintenancemanager` | `MaintenanceManager/MaintenanceManager.h` | No change required — existing `TASK_TIMEOUT = 3600 s` encompasses 120 s poll window |

## Success Criteria

- `update-prev-reboot-info` never reads an incomplete `PreviousLogs/` directory.
- `telemetry2_0` never grep-scans `PreviousLogs/` while files are being renamed by `uploadstblogs`.
- `uploadstblogs` waits for `previousreboot.info` up to timeout; proceeds with annotation if unavailable.
- Reboot-triggered archive names and file timestamp prefixes reflect post-NTP time when NTP is available; `systemtimemgr` last-known time is used as fallback.
- Upload always occurs if `backup_logs` succeeded, regardless of NTP/reboot-reason/telemetry timeout status.
- All timeout and fallback events are annotated in the upload for diagnostics.
- 330-second sleep removed; typical boot latency before upload reduced on fast devices.
- Existing unit and L2 tests continue to pass; new tests cover timeout, fallback, and sentinel paths.
