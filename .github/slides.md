---
theme: default
title: Logupload Synchronization
info: Boot-time log upload synchronization via sentinel files — dcm-agent, reboot-manager, and telemetry coordinated with inotify-based eventing.
highlighter: shiki
transition: slide-left
mdc: true
---
 
# Logupload Synchronization

### Boot-Time Coordination for `dcm-agent`
`backup_logs` · `reboot-info` · `telemetry2_0` · `uploadstblogs`

---

# The Problem

**Four** independent subsystems read `/opt/logs/PreviousLogs/` after reboot — with **no coordination**:

- **backup_logs** — moves logs to PreviousLogs/ directory
- **update-prev-reboot-info** — derives and persists reboot reason
- **uploadstblogs** — archives and uploads logs to a server
- **telemetry2_0** — grep-scans PreviousLogs/ for telemetry markers (one-shot)

### What goes wrong?

- Logs read before backup completion
- Reboot reasons derived from incomplete logs
- Uploads with incorrect timestamps or missing data
- **Telemetry markers permanently lost** — one-shot report, never retried
- **`sleep(330)` is a blind wait** — wastes up to 330 seconds on fast devices, too short on slow ones

---

# Known Issues — Evidence from RDK Stack

These defects trace directly to the boot-time race conditions described above:

| Defect | Platform | Symptom | Root Cause |
|--------|----------|---------|------------|
| **XIONE-18338** | Alpaca IT / RDK 8.4 | `UploadOnReboot` flag set to `false` even when XConf offers `true` | Upload suppressed due to race between DCM config delivery and reboot upload trigger |
| **DELIA-70285** | LLAMA Gen1/Gen2, CELLO | After CDL-initiated or maintenance reboot, device records wrong reboot reason — `SOFTWARE_MASTER_RESET` | `update-prev-reboot-info` reads `PreviousLogs/` before `backup_logs` completes; derives stale reboot reason |
| **XIONE-18607** | XIONE ALPACA IT | Log upload file name wrong for days | `uploadstblogs` archives before NTP sync — timestamp in filename reflects pre-NTP epoch |

&nbsp;

> All three defects share a single root cause: **no ordering guarantee** between boot-time subsystems.
> The sentinel-file chain proposed here closes all three gaps simultaneously.

---

# Current Architecture

```mermaid
graph LR
  subgraph C["Caller / Trigger Paths"]
    direction LR
    TEL["telemetry2_0"] -->|NTP sync check & grep previous logs| DCM["DCM Scheduler"]
    UMM["Unsolicited Maintenance"]
    SS["SystemServices API"]
    ULN["UploadLogsNow"]
    AS["Solicited Maintenance from AS"]
  end

  DCM --> BUP["Bootup upload path"]
  UMM --> BUP
  AS --> ASG{"PreviousLogs has files?"}
  ASG -->|No| SKIP["Skip upload"]
  ASG -->|Yes| BUP

  SS --> ODP["On-demand upload path"]
  ULN --> ODP

  BUP --> PREV["Source: /opt/logs/PreviousLogs/"]
  ODP --> CURR["Source: /opt/logs/ current logs"]

  PREV --> CORE["uploadstblogs core"]
  CURR --> CORE

  CORE --> LOCK["/tmp/.log-upload.lock"]
  LOCK --> SERVER["Log Server"]

  style TEL fill:#1b5e20,color:#fff
  style DCM fill:#2e7d32,color:#fff
  style UMM fill:#2e7d32,color:#fff
  style SS fill:#ef6c00,color:#fff
  style ULN fill:#ef6c00,color:#fff
  style AS fill:#8e24aa,color:#fff
  style BUP fill:#43a047,color:#fff
  style ODP fill:#fb8c00,color:#fff
  style SKIP fill:#c62828,color:#fff
  style CORE fill:#1565c0,color:#fff
  style LOCK fill:#455a64,color:#fff
  style SERVER fill:#00897b,color:#fff
```

---

# Proposed Architecture

```mermaid
graph LR
  subgraph C["Caller / Trigger Paths"]
    direction LR
    TEL["telemetry2_0"] -->|NTP sync check & grep previous logs| DCM["DCM Scheduler"]
    SS["SystemServices API"]
    ULN["UploadLogsNow"]
  end

  subgraph S["Support Signals (Bootup only)"]
    direction TB
    S1[".backup_logs_done"]
    S2["stt_received"]
    S3["Update_rebootInfo_invoked"]
  end

  DCM --> BUP["Bootup upload path"]

  SS --> ODP["On-demand upload path"]
  ULN --> ODP

  S1 -.gates.-> BUP
  S2 -.gates.-> BUP
  S3 -.gates.-> BUP

  BUP --> PREV["Source: /opt/logs/PreviousLogs/"]
  ODP --> CURR["Source: /opt/logs/ current logs"]

  PREV --> CORE["uploadstblogs core"]
  CURR --> CORE

  CORE --> LOCK["/tmp/.log-upload.lock"]
  LOCK --> SERVER["Log Server"]

  style TEL fill:#1b5e20,color:#fff
  style DCM fill:#2e7d32,color:#fff
  style SS fill:#ef6c00,color:#fff
  style ULN fill:#ef6c00,color:#fff
  style BUP fill:#43a047,color:#fff
  style ODP fill:#fb8c00,color:#fff

  style CORE fill:#1565c0,color:#fff
  style LOCK fill:#455a64,color:#fff
  style SERVER fill:#00897b,color:#fff
```

---
layout: two-cols
---

# Current State

- Four subsystems launch independently at boot
- No ordering guarantees — relies on `sleep(330)` blind wait
- Race conditions between log backup, reboot-info, upload, and telemetry
- Timestamps may be inaccurate (no NTP sync)
- Telemetry scans PreviousLogs/ before files are moved
- MaintenanceManager redundantly schedules log upload

::right::

# Target State

- **Sentinel-file chain** enforces strict ordering
- **inotify-based** wait (zero CPU, sub-ms latency)
- **Poll-based** — inotify wait for prerequisite sentinels
- **Soft gates** — NTP/reboot-reason timeouts annotate but never block upload
- **Hard gate** — only `backup_logs` failure aborts
- MaintenanceManager log upload task **removed**

---

# Sentinel File Protocol

```mermaid
graph LR
  BL["backup_logs"] -->|creates| S1[".backup_logs_done"]
  NTP["NTP Sync"] -->|creates| S2["stt_received"]
  S1 -->|hard gate| RM["reboot-info"]
  S1 -->|hard gate| T2["telemetry2_0"]
  S2 -->|soft gate| RM
  RM -->|creates| S3["Update_rebootInfo_invoked"]
  S2 -->|soft gate| UL["uploadstblogs"]
  S3 -->|soft gate| UL
  UL -->|uploads| SERVER["Log Server"]

  style BL fill:#4CAF50,color:#fff
  style RM fill:#2196F3,color:#fff
  style UL fill:#FF9800,color:#fff
  style NTP fill:#9C27B0,color:#fff
  style T2 fill:#E91E63,color:#fff
  style S1 fill:#E8F5E9,stroke:#4CAF50
  style S2 fill:#F3E5F5,stroke:#9C27B0
  style S3 fill:#E3F2FD,stroke:#2196F3
```

All sentinels reside in `/tmp/` — volatile, auto-cleared on reboot, no stale-sentinel risk.

---

# Sentinel Registry

| File | Written by | Consumed by | Type |
|------|-----------|------------|------|
| `/tmp/.backup_logs_done` | `backup_logs` | `update-prev-reboot-info`, `telemetry2_0`, `uploadstblogs` | **Hard gate** |
| `/tmp/stt_received` | time-sync service | `uploadstblogs` | Soft gate (NTP) |
| `/tmp/Update_rebootInfo_invoked` | `update-prev-reboot-info` | `uploadstblogs` | Soft gate |


---

# Execution Flow — Poll-Based Synchronization

- **Device Reboots**
  - `backup_logs` starts → moves logs to `/opt/logs/PreviousLogs/`
    - If fails → `.backup_logs_done` NOT written → **all downstream aborts** (hard gate)
  - On success → creates `/tmp/.backup_logs_done` (atomic `open()` + `close()`)
  - `update-prev-reboot-info` inotify-waits for `.backup_logs_done` + `stt_received`
    - Derives reboot reason → creates `/tmp/Update_rebootInfo_invoked`
  - `telemetry2_0` inotify-waits for `.backup_logs_done`
    - Grep-scans `PreviousLogs/` for telemetry markers
  - `uploadstblogs` **polls soft gates**:
    - inotify-polls both soft gates (120s timeout):
       - `/tmp/stt_received` — NTP fallback via `systemtimemgr` if missing
       - `/tmp/Update_rebootInfo_invoked` — annotate if missing
  - **Always proceeds to archive + upload** if backup succeeded, annotating any missing metadata

---

# State Machine — Failure Scenarios

```mermaid
stateDiagram-v2
    [*] --> BackupLogs
    BackupLogs --> Error: fail
    BackupLogs --> CheckSTT: success
    CheckSTT --> CheckRebootInfo: stt_ok
    CheckSTT --> CheckInternet: stt_missing
    CheckInternet --> UseLastGoodTime: internet_ok
    CheckInternet --> Upload: internet_fail
    UseLastGoodTime --> CheckRebootInfo
    CheckRebootInfo --> Upload: rebootinfo_ok
    CheckRebootInfo --> Upload: rebootinfo_missing (annotate)
    Upload --> Success: upload_ok
    Upload --> Error: upload_fail
    Error --> [*]
    Success --> [*]
```
---

# Key Components

| Component | Language | Role |
|-----------|----------|------|
| **backup_logs** | C | Move logs to `/opt/logs/PreviousLogs/`; write hard-gate sentinel |
| **Reboot Manager** | C | Derive and persist reboot reason; write soft-gate sentinel |
| **uploadstblogs** | C | Poll prerequisites, archive & upload logs to server |
| **telemetry2_0** | C | Grep-scan PreviousLogs/ for markers (gates on `.backup_logs_done`) |
| **Sentinel Files** | `/tmp/` | Inter-process synchronization (volatile, auto-cleared) |
| **systemtimemgr** | C | Provides last-known-good time (`/opt/secure/clock.txt`) if NTP unavailable |

---

# MaintenanceManager — LogUpload Task Removed

MaintenanceManager triggered log upload via two paths — **both redundant**:

| Path | Behaviour | Why redundant |
|------|-----------|---------------|
| **Unsolicited maintenance** | Triggers bootup log upload during maintenance window | DCM already triggers via `telemetry2_0` → dcm-agent RBUS event path |
| **Solicited maintenance** | Attempts bootup log upload on explicit request | Always skips — `/opt/logs/PreviousLogs` already empty |

### What changes

- `MAINT_DCM_LOGUPLOAD` task removed from MaintenanceManager task table
- `MAINT_LOGUPLOAD_COMPLETE` / `MAINT_LOGUPLOAD_ERROR` IARM events removed from `uploadstblogs`
- IARM dependency in `uploadstblogs` removed if no other IARM usages remain
- `dcm-agent` becomes **single owner** of log upload scheduling

---

# Error Handling — Hard vs Soft Gates

### Hard gate: `backup_logs` failure = **abort upload**

- `.backup_logs_done` NOT written → all downstream services time out
- `uploadstblogs` detects both soft-gate sentinels absent → treats as hard abort
- **Only case where upload is cancelled entirely**

### Soft gates: annotate and proceed

| Component | Polls for | Timeout | Action on timeout |
|-----------|-----------|:-------:|-------------------|
| `update-prev-reboot-info` | `.backup_logs_done` | 60s | Exit `ERROR_GENERAL` |
| `telemetry2_0` | `.backup_logs_done` | 60s | Skip PreviousLogs report |
| `uploadstblogs` | `stt_received` + `Update_rebootInfo_invoked` | 120s | **Annotate** missing prerequisites and **always proceed** |

---

# Failure Scenario Summary

| # | Scenario | Upload outcome |
|:-:|----------|----------------|
| S1 | `backup_logs` fails | **Aborted** — EXIT_FAILURE |
| S2 | Backup OK, reboot reason absent | Proceeds + `REBOOT_REASON_UNAVAILABLE` |
| S3 | Backup OK, NTP + RI absent | Proceeds + `NTP_FALLBACK` or `NTP_UNAVAILABLE` + `REBOOT_REASON_UNAVAILABLE` |

> **Invariant**: Upload always occurs if `backup_logs` succeeded. Missing metadata is annotated, never silently discarded.

---

# Log Upload: State Machine & Fallbacks

> **Fallback Method:** Log upload must always happen except when log backup fails. All missing/failed steps are annotated in the upload for diagnostics.

- DCM Agent synchronizes backup, STT, reboot reason, telemetry, and upload.
- Each module owns its state; DCM Agent coordinates as needed.
- State machine ensures robust fallback and retry logic.

👉 **[Logupload State Machine & Fallbacks](./logupload-state-machine.md)**

Refer to the linked file for the scenario breakdowns and state diagram.

---

# Summary

Signal-file chain eliminates race conditions at boot

**backup_logs → { telemetry2_0, reboot-info } → uploadstblogs**

3 signal files: `.backup_logs_done` · `stt_received` · `Update_rebootInfo_invoked`

3-repo atomic release: dcm-agent · reboot-manager · telemetry

18 tasks across 7 groups — clean skip on any timeout

---
layout: center
---


## Summary Table

| Step           | Hard Dependency | Fallback Action                | Upload Allowed? |
|----------------|----------------|--------------------------------|-----------------|
| BackupLogs     | Yes            | None (abort if missing)        | No              |
| STT            | No             | Check internet, use last good time | Yes         |
| RebootInfo     | No             | Wait, annotate if missing, proceed  | Yes         |

---

## Risks & Notes

- If both STT and internet are missing, upload proceeds but is heavily annotated as incomplete.
- Distributed state machine: Each module owns its state, but uploadstblogs orchestrates the checks.
- Timeouts must be coordinated to avoid indefinite waits.
- All fallback uploads must clearly annotate what metadata was missing or defaulted.

---

# Thank You

[dcm-agent Repository](https://github.com/rdkcentral/dcm-agent)
---
