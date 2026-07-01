---
theme: default
title: Logupload Synchronization
info: Boot-time log upload synchronization via sentinel files — dcm-agent, reboot-manager, and telemetry coordinated with inotify-based eventing.
highlighter: shiki
transition: slide-left
mdc: true
background: linear-gradient(135deg, #0f2027, #203a43, #2c5364)
---

<style>
h1 { color: #e0f7fa !important; }
h3 { color: #b2dfdb !important; }
p, code { color: #e0e0e0 !important; }
</style>

# Logupload Synchronization

### Boot-Time Coordination for `dcm-agent`
`backup_logs` · `reboot-info` · `telemetry2_0` · `uploadstblogs`

---
background: linear-gradient(135deg, #1a0000, #3d0c02, #5c1a0a)
---

<style>
h1 { color: #ffcdd2 !important; }
h3 { color: #ef9a9a !important; }
li, p, strong, code { color: #fce4ec !important; }
</style>

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
background: linear-gradient(135deg, #1b0a2e, #2d1b4e, #1a0a3e)
---

<style>
h1 { color: #ce93d8 !important; }
th { color: #e1bee7 !important; background: rgba(0,0,0,0.3) !important; }
td { color: #f3e5f5 !important; }
blockquote { border-left-color: #ab47bc !important; }
blockquote p { color: #e1bee7 !important; }
p { color: #e1bee7 !important; }
strong { color: #f3e5f5 !important; }
</style>

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
background: linear-gradient(135deg, #0d1b2a, #1b2838, #253747)
---

<style>
h1 { color: #90caf9 !important; }
</style>

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
background: linear-gradient(135deg, #002a1a, #0a3d2e, #1a4d3e)
---

<style>
h1 { color: #a5d6a7 !important; }
</style>

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
background: linear-gradient(135deg, #1a1a2e, #16213e, #0f3460)
---

<style>
h1 { color: #90caf9 !important; }
li { color: #e3f2fd !important; }
strong { color: #bbdefb !important; }
</style>

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
background: linear-gradient(135deg, #1b3a1b, #2e5a2e, #1a4a2a)
---

<style>
h1 { color: #c8e6c9 !important; }
p { color: #e8f5e9 !important; }
</style>

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
background: linear-gradient(135deg, #1a2a1a, #1e3a1e, #2a4a2a)
---

<style>
h1 { color: #a5d6a7 !important; }
th { color: #c8e6c9 !important; background: rgba(0,0,0,0.3) !important; }
td { color: #e8f5e9 !important; }
strong { color: #fff !important; }
</style>

# Sentinel Registry

| File | Written by | Consumed by | Type |
|------|-----------|------------|------|
| `/tmp/.backup_logs_done` | `backup_logs` | `update-prev-reboot-info`, `telemetry2_0`, `uploadstblogs` | **Hard gate** |
| `/tmp/stt_received` | time-sync service | `uploadstblogs` | Soft gate (NTP) |
| `/tmp/Update_rebootInfo_invoked` | `update-prev-reboot-info` | `uploadstblogs` | Soft gate |


---
background: linear-gradient(135deg, #0d2137, #162d50, #1e3a5f)
---

<style>
h1 { color: #90caf9 !important; }
li { color: #e3f2fd !important; }
strong { color: #bbdefb !important; }
code { color: #80cbc4 !important; }
</style>

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
background: linear-gradient(135deg, #2a1a0a, #3d2b1a, #4a3520)
---

<style>
h1 { color: #ffe0b2 !important; }
</style>

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
background: linear-gradient(135deg, #1a1a2e, #2d2d44, #16213e)
---

<style>
h1 { color: #b39ddb !important; }
th { color: #d1c4e9 !important; background: rgba(0,0,0,0.3) !important; }
td { color: #ede7f6 !important; }
strong { color: #fff !important; }
code { color: #80cbc4 !important; }
</style>

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
background: linear-gradient(135deg, #0f2027, #203a43, #2c5364)
---

<style>
h1 { color: #e0f7fa !important; }
h3 { color: #b2dfdb !important; }
li { color: #e0f2f1 !important; }
p { color: #e0f2f1 !important; }
strong { color: #fff !important; }
code { color: #80cbc4 !important; }
</style>

# Summary

Signal-file chain eliminates race conditions at boot

**backup_logs → { telemetry2_0, reboot-info } → uploadstblogs**

### Key changes

- **`sleep(330)` removed** — replaced with inotify-based sentinel polling (zero CPU, sub-ms response)
- **MaintenanceManager log upload trigger removed** — dcm-agent is now single owner of upload scheduling
- **3 signal files** coordinate boot sequence: `.backup_logs_done` · `stt_received` · `Update_rebootInfo_invoked`
- **Soft-gate semantics** — upload always proceeds if backup succeeded; missing metadata is annotated, never blocks

---
background: linear-gradient(135deg, #0f2027, #203a43, #2c5364)
---

<style>
h2 { color: #e0f7fa !important; }
th { color: #b2dfdb !important; background: rgba(0,0,0,0.3) !important; }
td { color: #e0f2f1 !important; }
</style>

## Summary Table

| Step           | Hard Dependency | Fallback Action                | Upload Allowed? |
|----------------|----------------|--------------------------------|-----------------|
| BackupLogs     | Yes            | None (abort if missing)        | No              |
| STT            | No             | Check internet, use last good time | Yes         |
| RebootInfo     | No             | Wait, annotate if missing, proceed  | Yes         |

---
background: linear-gradient(135deg, #1a237e, #283593, #1a2070)
---

<style>
h1 { color: #c5cae9 !important; }
h3 { color: #9fa8da !important; }
p, li { color: #e8eaf6 !important; }
blockquote { border-left-color: #5c6bc0 !important; }
blockquote p { color: #c5cae9 !important; }
code { color: #80cbc4 !important; }
</style>

# Open Question: Is bak1/bak2 Backup Rotation Still Needed?

Previously, log upload took **~7 minutes** due to the blind `sleep(330)`. If a reboot occurred during this window, logs still pending upload would be rotated into backup directories (`bak1`, `bak2`, etc.) to prevent data loss.

With the sentinel-based change, log upload completes in **~40 seconds**. The probability of a reboot occurring within this narrow window is significantly lower.

### Question

> Is the `bak1`/`bak2` rotation mechanism still necessary, or can it be simplified/removed?


# Thank You

[dcm-agent Repository](https://github.com/rdkcentral/dcm-agent)
---
