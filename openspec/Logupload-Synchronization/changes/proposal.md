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
| AC-5 | The 330-second `sleep()` is replaced with explicit checks for `/tmp/stt_received` (NTP), `/tmp/Update_rebootInfo_invoked` (reboot reason), and `/tmp/.telemetry_prevlogs_done` (telemetry scan complete); upload proceeds only when all three prerequisites are met |
| AC-6 | Log upload will be skipped cleanly if any prerequisite times out — no partial or mis-classified upload |
| AC-7 | `backup_logs` continues to run without any NTP dependency |
| AC-8 | All new sentinel-poll paths are covered by unit tests |
| AC-9 | The full sentinel chain is covered by an L2 integration test |
| AC-10 | `telemetry2_0` completes its one-shot `PreviousLogs/` grep scan and writes `/tmp/.telemetry_prevlogs_done` before `uploadstblogs` calls `add_timestamp_to_files()` or any other operation that mutates filenames in `PreviousLogs/` |
| AC-11 | On reboot, log upload is owned by DCM Agent and MUST proceed when `backup_logs` succeeds |
| AC-12 | Maintenance Manager logupload triggers (solicited and unsolicited) are removed as redundant |

---

## Summary

Enforce a strict, sentinel-based execution order across four independent subsystems
(`backup_logs`, `telemetry2_0`, `reboot-manager`, `uploadstblogs`) during device boot-up so that:

1. Log backup completes successfully before reboot-reason derivation or telemetry scanning starts.
2. `telemetry2_0` completes its one-shot `PreviousLogs/` grep scan before `uploadstblogs` renames or archives any files.
3. Reboot reason is fully updated and persisted before any log cleanup or log upload is triggered.
4. Log upload tarfile is created with NTP-synchronized time to maintain accurate timestamps.
5. Log upload only occurs after reboot reason update is confirmed.
6. Reboot-triggered upload ownership is explicit: DCM Agent is the single authoritative trigger path.

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

Introduce a sentinel-file protocol that chains the subsystems:

```
backup_logs
  → writes /tmp/.backup_logs_done  (on success)

telemetry2_0  [parallel with update-prev-reboot-info]
  → polls /tmp/.backup_logs_done  (before grep-scanning PreviousLogs/)
  → writes /tmp/.telemetry_prevlogs_done  (after scan completes)  ← NEW

update-prev-reboot-info
  → polls /tmp/.backup_logs_done  (before reading PreviousLogs)
  → writes /tmp/Update_rebootInfo_invoked  (already exists, already the completion signal)

uploadstblogs reboot_setup()
  → polls /tmp/stt_received              (NTP sync — explicit check)
  → polls /tmp/Update_rebootInfo_invoked  (reboot reason ready — replaces sleep 330s)
  → polls /tmp/.telemetry_prevlogs_done   (telemetry done with PreviousLogs/ — NEW)
  → proceeds only when all three present → safe to rename files and archive
```

Using `/tmp/` ensures sentinels are volatile (cleared on every reboot), so there is no
stale-sentinel problem across boots.

## Goals

- Guarantee log backup completes before reboot-reason derivation or telemetry scanning.
- Guarantee `telemetry2_0` finishes its one-shot `PreviousLogs/` grep scan before `uploadstblogs` renames or archives any files.
- Guarantee reboot reason is persisted before log upload starts.
- Guarantee archive timestamps use NTP-correct time, with no NTP dependency added to `backup_logs` itself.
- Remove the unreliable 330-second sleep from `uploadstblogs`.
- Guarantee reboot uploads are always triggered by DCM Agent when `backup_logs` succeeds.
- Remove Maintenance Manager logupload triggers in both solicited and unsolicited cases as redundant.
- Keep changes minimal and platform-neutral (no new IPC, no systemd-specific logic added to C code).

## Non-Goals

- Changing the NTP synchronization mechanism itself.
- Adding NTP awareness to `backup_logs` (it must remain NTP-free by design).
- Supporting concurrent reboot-triggered uploads (single-instance lock already exists).
- Removing or altering valid on-demand API triggers (`SystemServices API`, `UploadLogsNow`).
- Providing a backward compatibility window for partial deployments — all three repositories (`dcm-agent`, `reboot-manager`, `telemetry`) MUST be deployed simultaneously.

## Affected Repositories / Modules

| Repository | Module | Change type |
|------------|--------|-------------|
| `dcm-agent` | `backup_logs/src/backup_logs.c` | Add sentinel write |
| `dcm-agent` | `uploadstblogs/src/strategies.c` (`reboot_setup`) | Replace sleep with triple-sentinel poll |
| `reboot-manager` | `reboot-reason-fetcher/src/rebootreason_main.c` | Add sentinel poll |
| `reboot-manager` | `reboot-reason-fetcher/include/update-reboot-info.h` | Add sentinel constant |
| `telemetry` | `telemetry2_0/src/profile.c` (`CollectAndReport`) | Poll `.backup_logs_done`; write `.telemetry_prevlogs_done` |

## Success Criteria

- `update-prev-reboot-info` never reads an incomplete `PreviousLogs/` directory.
- `telemetry2_0` never grep-scans `PreviousLogs/` while files are being renamed by `uploadstblogs`.
- `uploadstblogs` never starts archiving before `previousreboot.info` is written.
- All reboot-triggered archive names and file timestamp prefixes reflect post-NTP time.
- 330-second sleep removed; typical boot latency before upload reduced on fast devices.
- Existing unit and L2 tests continue to pass; new tests cover timeout and sentinel paths.
