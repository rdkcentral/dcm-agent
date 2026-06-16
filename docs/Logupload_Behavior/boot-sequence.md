# Boot-Time Log Upload Sequence

> Part of `Logupload_Behavior/` — see [README.md](README.md) for the full ecosystem map.

---

## Context: Why Boot Order Matters for Log Upload

After a device reboot, `uploadstblogs` must upload the **previous boot's logs** — not the
current boot's logs. This requires four preparatory steps to complete before upload can
begin:

1. **Backup** — `backup_logs` must move current→previous logs before anything reads them
2. **Reboot metadata** — `reboot-manager/reboot-reason-fetcher` must derive and write reboot
   reason from PreviousLogs before the archive is uploaded (or the wrong reason is embedded)
3. **Telemetry scan** — `telemetry2_0` must scan PreviousLogs for telemetry markers before
   the directory is archived and cleared
4. **Network/STT** — device identity (STT received) must be established so the upload
   endpoint and filename are correct

Without enforced ordering, these four steps race, producing the defects documented in
XIONE-18338, DELIA-70285, and XIONE-18607.

---

## Signal File Protocol

All four components coordinate via presence-check files in `/tmp/`. A component writes its
marker file only after its preparatory work is fully complete. Downstream components poll
for the file's existence rather than using IPC.

```
Component             Signal File Written         Condition to Write
─────────────────     ──────────────────────────  ─────────────────────────────
backup_logs           /tmp/.backup_logs_done      All previous-log rotation complete
reboot-manager/       /tmp/Update_rebootInfo_     .backup_logs_done exists AND
 reboot-reason-       invoked                     stt_received
 fetcher
telemetry2_0          /tmp/.telemetry_prevlogs_   .backup_logs_done exists AND
                      done                        PreviousLogs/ scan complete
uploadstblogs         (none — begins upload)      ALL THREE above exist (120s timeout)
```

---

## Sequence Diagram

```
Boot
  │
  │   systemd / init
  │     ├─ starts backup_logs (early boot unit)
  │     ├─ starts telemetry2_0 daemon
  │     ├─ starts reboot-manager services
  │     └─ starts dcm-agent / uploadstblogs

  │
  ▼
backup_logs
  │  reads: /opt/logs/  (current boot logs)
  │  creates: /opt/logs/PreviousLogs/   (rotated previous boot logs)
  │  writes: /tmp/.backup_logs_done     ← SIGNAL A
  ▼

  ┌──────────────────────────────────────────────────────┐
  │  (parallel after .backup_logs_done exists)           │
  │                                                      │
  │  reboot-manager/reboot-reason-fetcher                │
  │    polls: .backup_logs_done  (present)               │
  │    polls: stt_received       (waits up to N seconds) │
  │    reads: PreviousLogs/ → derives reboot reason      │
  │    writes reboot metadata to:                        │
  │       /opt/logs/rebootInfo.log                       │
  │       /opt/secure/reboot/reboot.info (JSON)          │
  │    writes: /tmp/Update_rebootInfo_invoked  ← SIGNAL B│
  │                                                      │
  │  telemetry2_0                                        │
  │    polls: .backup_logs_done  (present)               │
  │    scans: PreviousLogs/ for telemetry markers        │
  │    processes markers into telemetry reports          │
  │    writes: /tmp/.telemetry_prevlogs_done  ← SIGNAL C │
  └──────────────────────────────────────────────────────┘

  ▼
uploadstblogs reboot_setup()
  │  polls (up to 120 seconds, CLOCK_MONOTONIC):
  │    /tmp/Update_rebootInfo_invoked   (SIGNAL B)
  │    /tmp/.telemetry_prevlogs_done    (SIGNAL C)
  │    stt_received                     (network identity)
  │
  │  if all present within timeout:
  │    → executes upload [TRIGGER_REBOOT]
  │    → archives PreviousLogs/ into .tar.gz
  │    → uploads to cloud endpoint
  │    → emits IARM: MAINT_LOGUPLOAD_COMPLETE
  │    → emits T2: SYST_INFO_lu_success
  │
  │  if timeout (120s) expires with missing signals:
  │    → proceeds with upload anyway (best effort)
  │    → logs which signals were absent
  ▼
```

---

## Component Details

### backup_logs (rdkcentral/dcm-agent)

**Role:** The gate-keeper for the entire boot-time sequence.

**What it does:**
- Reads `LOG_PATH` from device properties
- Rotates current logs into `$LOG_PATH/PreviousLogs/` using a 4-level rotation strategy
  (for HDD-disabled devices) or a timestamped strategy (for HDD-enabled devices)
- Creates a `last_reboot` marker file
- Optionally runs `disk_threshold_check.sh`
- Writes `/tmp/.backup_logs_done` when rotation is complete

**Key source files:**
- `backup_logs/src/backup_logs.c` — orchestration
- `backup_logs/src/backup_engine.c` — rotation strategy
- `backup_logs/src/sys_integration.c` — systemd integration

**Timing:** Must complete before any component reads PreviousLogs. If it runs slowly on
low-memory devices, the entire boot-time upload sequence is delayed proportionally.

---

### reboot-manager / reboot-reason-fetcher (rdkcentral/reboot-manager)

**Role:** Derives and persists the reboot reason from post-reboot log evidence.

**What it does:**
- Waits for `/tmp/.backup_logs_done` (PreviousLogs is ready)
- Waits for `stt_received` (device identity confirmed)
- Reads PreviousLogs to find reboot reason evidence
- Classifies reason as: `APP_TRIGGERED`, `OPS_TRIGGERED`, `MAINTENANCE_REBOOT`, `FIRMWARE_FAILURE`
- Writes reboot metadata to:
  - `/opt/logs/rebootInfo.log`
  - `/opt/secure/reboot/reboot.info` (JSON)
  - `/opt/secure/reboot/previousreboot.info` (for next-cycle cyclic loop detection)
- Writes `/tmp/Update_rebootInfo_invoked`

**Key defect:** DELIA-70285 — if this runs before `backup_logs` completes, PreviousLogs
may contain the wrong boot's data, causing incorrect reboot reason classification.

---

### telemetry2_0 (rdkcentral/telemetry)

**Role:** Extracts telemetry markers from the previous boot's logs.

**What it does:**
- Waits for `/tmp/.backup_logs_done`
- Scans `$LOG_PATH/PreviousLogs/` for log marker patterns defined in telemetry profiles
- Creates a seekmap of found markers
- Writes `/tmp/.telemetry_prevlogs_done` when scan is complete

**Relationship with uploadstblogs:**
- `uploadstblogs` produces T2 telemetry events (upload outcome markers)
- `telemetry2_0` produces its own separate upload (report JSON) to the analytics backend
- These are two separate upload mechanisms going to two different endpoints

**Signal handling for upload trigger (telemetry's own mechanism):**

| Signal | Effect |
|--------|--------|
| SIGUSR1 / kill -10 | Trigger log upload with seekmap reset |
| Signal 29 (SIGIO) | On-demand log upload without seekmap reset |
| SIGUSR2 / kill -12 | Reload XConf profile configuration |

---

### uploadstblogs reboot_setup() (rdkcentral/dcm-agent)

**Role:** The actual log packager and uploader for the post-reboot case.

**What it does:**
1. Polls for all three signal files with 120-second timeout using `clock_gettime(CLOCK_MONOTONIC)`
2. Archives `PreviousLogs/` into a timestamped `.tar.gz`
3. Selects upload strategy (Direct or CodeBig fallback)
4. Uploads via HTTPS or HTTP
5. Verifies upload success via HTTP response code
6. Cleans up archive
7. Emits IARM events and T2 markers

**Timeout behaviour:**
- Uses `CLOCK_MONOTONIC` (immune to NTP clock jumps) for the 120-second window
- If timeout expires: proceeds with upload using whatever metadata is available
- Logs which signals were absent at timeout

---

## The Race Conditions (Current State Without Fix)

Without the signal-file protocol being fully implemented, three defects exist:

### DELIA-70285: Wrong reboot reason

```
boot
  ├─ reboot-manager/reboot-helper reads PreviousLogs/
  │     ← RACE: PreviousLogs not yet populated by backup_logs
  │     → reads empty or wrong-boot logs
  │     → derives wrong reboot reason (e.g., SOFTWARE_MASTER_RESET)
  │
  └─ backup_logs starts populating PreviousLogs/  (too late)
```

**Affected platforms:** LLAMA Gen1/Gen2, CELLO

### XIONE-18607: Wrong archive filename timestamp

```
boot
  └─ uploadstblogs starts immediately after backup_logs
        ← RACE: NTP sync not yet complete
        → clock returns 1970 or pre-NTP epoch
        → archive filename: stblogs_..._00000000_000000.tar.gz
        (wrong for days until device reboots again)
```

**Affected platforms:** XIONE ALPACA IT

### XIONE-18338: UploadOnReboot=false despite XConf offering true

```
boot
  └─ uploadstblogs reads DCM settings file for UploadOnReboot value
        ← RACE: DCM daemon has not yet written settings file
        → reads default false
        → skips reboot upload entirely
```

**Affected platforms:** Alpaca IT / RDK 8.4

---

## Design Proposal (Logupload-Synchronization Change)

The `Logupload-Synchronization` OpenSpec change proposes fixing all three races by
implementing the full signal-file protocol:

```
backup_logs      → /tmp/.backup_logs_done
reboot-manager   → /tmp/Update_rebootInfo_invoked  (polls .backup_logs_done + stt_received)
telemetry2_0     → /tmp/.telemetry_prevlogs_done   (polls .backup_logs_done)
uploadstblogs    → polls ALL THREE (120s CLOCK_MONOTONIC timeout)
```

This requires a **coordinated 3-repo release** of:
- `rdkcentral/dcm-agent` (backup_logs + uploadstblogs changes)
- `rdkcentral/reboot-manager` (reboot-reason-fetcher signal file writing)
- `rdkcentral/telemetry` (telemetry2_0 signal file writing)

See `openspec/Logupload-Synchronization/` for full design, requirements, and tasks.
