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
- **Trigger-then-poll** — re-trigger lagging services
- **Soft gates** — NTP/reboot-reason/telemetry timeouts annotate but never block upload
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
  T2 -->|creates| S4[".telemetry_prevlogs_done"]
  S2 -->|soft gate| UL["uploadstblogs"]
  S3 -->|soft gate| UL
  S4 -->|soft gate| UL
  UL -->|triggers| TR1[".trigger_reboot_info_update"]
  UL -->|triggers| TR2[".trigger_telemetry_prevlogs_scan"]
  UL -->|uploads| SERVER["Log Server"]

  style BL fill:#4CAF50,color:#fff
  style RM fill:#2196F3,color:#fff
  style UL fill:#FF9800,color:#fff
  style NTP fill:#9C27B0,color:#fff
  style T2 fill:#E91E63,color:#fff
  style S1 fill:#E8F5E9,stroke:#4CAF50
  style S2 fill:#F3E5F5,stroke:#9C27B0
  style S3 fill:#E3F2FD,stroke:#2196F3
  style S4 fill:#FCE4EC,stroke:#E91E63
  style TR1 fill:#FFF3E0,stroke:#FF9800
  style TR2 fill:#FFF3E0,stroke:#FF9800
```

All sentinels reside in `/tmp/` — volatile, auto-cleared on reboot, no stale-sentinel risk.

---

# Sentinel Registry

| File | Written by | Consumed by | Type |
|------|-----------|------------|------|
| `/tmp/.backup_logs_done` | `backup_logs` | `update-prev-reboot-info`, `telemetry2_0`, `uploadstblogs` | **Hard gate** |
| `/tmp/stt_received` | time-sync service | `uploadstblogs` | Soft gate (NTP) |
| `/tmp/Update_rebootInfo_invoked` | `update-prev-reboot-info` | `uploadstblogs` | Soft gate |
| `/tmp/.telemetry_prevlogs_done` | `telemetry2_0` | `uploadstblogs` | Soft gate |
| `/tmp/.trigger_reboot_info_update` | `uploadstblogs` | `update-prev-reboot-info` (retry) | Trigger |
| `/tmp/.trigger_telemetry_prevlogs_scan` | `uploadstblogs` | `telemetry2_0` (retry) | Trigger |

---

# Execution Flow — Trigger-Then-Poll

- **Device Reboots**
  - `backup_logs` starts → moves logs to `/opt/logs/PreviousLogs/`
    - If fails → `.backup_logs_done` NOT written → **all downstream aborts** (hard gate)
  - On success → creates `/tmp/.backup_logs_done` (atomic `open()` + `close()`)
  - `update-prev-reboot-info` inotify-waits for `.backup_logs_done` + `stt_received`
    - Derives reboot reason → creates `/tmp/Update_rebootInfo_invoked`
  - `telemetry2_0` inotify-waits for `.backup_logs_done`
    - Grep-scans `PreviousLogs/` → creates `/tmp/.telemetry_prevlogs_done`
  - `uploadstblogs` **triggers then polls**:
    1. Writes `/tmp/.trigger_reboot_info_update` if `Update_rebootInfo_invoked` absent
    2. Writes `/tmp/.trigger_telemetry_prevlogs_scan` if `.telemetry_prevlogs_done` absent
    3. inotify-polls all three soft gates (120s timeout):
       - `/tmp/stt_received` — NTP fallback via `systemtimemgr` if missing
       - `/tmp/Update_rebootInfo_invoked` — annotate if missing
       - `/tmp/.telemetry_prevlogs_done` — annotate if missing
  - **Always proceeds to archive + upload** if backup succeeded, annotating any missing metadata

---

# Execution Sequence

```mermaid
sequenceDiagram
  participant Device
  participant backup_logs
  participant NTP
  participant Telemetry as telemetry2_0
  participant RebootMgr as update-prev-reboot-info
  participant Upload as uploadstblogs
  participant Server as Log Server

  Device->>backup_logs: Boot trigger
  backup_logs->>backup_logs: Move logs to PreviousLogs/
  backup_logs-->>Device: Create /tmp/.backup_logs_done

  NTP-->>Device: Create /tmp/stt_received

  par Parallel processing
    Telemetry->>Device: inotify wait .backup_logs_done
    Telemetry->>Telemetry: Grep scan PreviousLogs/
    Telemetry-->>Server: Send Previous Logs report
    Telemetry-->>Device: Create /tmp/.telemetry_prevlogs_done
  and
    RebootMgr->>Device: inotify wait .backup_logs_done
    RebootMgr->>Device: inotify wait stt_received
    RebootMgr->>RebootMgr: Derive reboot reason
    RebootMgr-->>Device: Create /tmp/Update_rebootInfo_invoked
  end

  Upload->>Upload: trigger_missing_services()
  Upload->>Device: inotify-poll stt_received, Update_rebootInfo_invoked, .telemetry_prevlogs_done
  alt All sentinels present
    Upload->>Upload: Proceed — no annotations
  else Timeout (soft gates)
    Upload->>Upload: Annotate missing prerequisites
    Upload->>Upload: apply_ntp_fallback_time() if NTP missing
  end
  Upload->>Upload: add_timestamp_to_files() + generate_archive_name()
  Upload->>Server: Upload logs (annotated if needed)
  Server-->>Upload: Acknowledge
```

---

# State Machine — Failure Scenarios

```mermaid
stateDiagram-v2
    [*] --> BackupLogs : Device boot

    BackupLogs --> Abort : S1 — backup_logs fails
    BackupLogs --> WriteBackupDone : backup_logs succeeds

    state Abort {
        [*] --> LogBackupError
        LogBackupError --> [*]
    }
    Abort --> [*] : EXIT_FAILURE — no upload

    WriteBackupDone --> WriteTriggers : create .backup_logs_done

    state WriteTriggers {
        [*] --> TriggerRI : write trigger if RI absent
        TriggerRI --> TriggerTel : write trigger if Tel absent
        TriggerTel --> [*]
    }

    WriteTriggers --> Poll120s : inotify-poll 120s

    state Poll120s {
        [*] --> PollLoop
        PollLoop --> AllReady : all 3 sentinels present
        PollLoop --> TimedOut : elapsed >= 120s
        AllReady --> [*] : bitmask = 0
        TimedOut --> [*] : bitmask of missing
    }

    Poll120s --> HandleMissing

    state HandleMissing {
        [*] --> NTPMissing : S3 — apply systemtimemgr fallback or annotate
        [*] --> RIMissing : S2 — annotate REBOOT_REASON_UNAVAILABLE
        [*] --> TelMissing : S4 — annotate TELEMETRY_UNAVAILABLE
        [*] --> AllPresent : no annotations needed
    }

    HandleMissing --> Upload : ALWAYS proceed (soft gates)

    state Upload {
        [*] --> Archive : generate_archive_name + add_timestamp_to_files
        Archive --> UploadToServer
        UploadToServer --> [*]
    }

    Upload --> [*]
```

---

# IPC Mechanism — inotify with stat() Fallback

| Mechanism | CPU during wait | Latency | POSIX portable | Crash-safe | Boot-safe |
|-----------|:-:|:-:|:-:|:-:|:-:|
| **`inotify` + sentinel files** | **Zero** | **<1 ms** | ❌ (Linux) | ✅ | ✅ |
| `stat()` poll (fallback) | Low (1 Hz) | ~1 s | ✅ | ✅ | ✅ |
| Named semaphore | Zero | <1 ms | ✅ | ❌ | ✅ |
| RBUS events | Zero | <1 ms | ❌ | ⚠️ | ❌ early boot |
| IARM Bus events | Zero | <1 ms | ❌ | ❌ lost-event | ⚠️ order-dep |

**Decision**: `inotify` + `select()` for the wait, sentinel files in `/tmp/` for state. `stat()` fallback via `#ifdef HAVE_INOTIFY`. All current targets are embedded Linux.

---

# NTP Timestamp Correctness & Fallback

```mermaid
flowchart TB
    POLL["reboot_setup() polls stt_received"]
    POLL -->|found| NTP_OK["time(NULL) — NTP-correct ✓"]
    POLL -->|timeout| FALLBACK["apply_ntp_fallback_time()"]
    FALLBACK --> INET{"internet reachable?"}
    INET -->|yes| SYSTIME["systemtimemgr last-known time<br/>/opt/secure/clock.txt"]
    INET -->|no| RAW["time(NULL) as-is (pre-NTP)"]
    SYSTIME --> ANNOT1["Annotate: NTP_FALLBACK_SYSTEMTIMEMGR"]
    RAW --> ANNOT2["Annotate: NTP_UNAVAILABLE"]

    style NTP_OK fill:#4CAF50,color:#fff
    style SYSTIME fill:#FFC107,color:#000
    style RAW fill:#F44336,color:#fff
```

`backup_logs` runs independently and pre-NTP — its `time()` calls are irrelevant. Only archive names and timestamp prefixes from `add_timestamp_to_files()` require NTP accuracy.

---

# Key Components

| Component | Language | Role |
|-----------|----------|------|
| **backup_logs** | C | Move logs to `/opt/logs/PreviousLogs/`; write hard-gate sentinel |
| **Reboot Manager** | C | Derive and persist reboot reason; write soft-gate sentinel |
| **uploadstblogs** | C | Trigger-then-poll, archive & upload logs to server |
| **telemetry2_0** | C | Grep-scan PreviousLogs/ for markers; write soft-gate sentinel |
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
- `uploadstblogs` detects all three soft-gate sentinels absent → treats as hard abort
- **Only case where upload is cancelled entirely**

### Soft gates: annotate and proceed

| Component | Polls for | Timeout | Action on timeout |
|-----------|-----------|:-------:|-------------------|
| `update-prev-reboot-info` | `.backup_logs_done` | 60s | Exit `ERROR_GENERAL` |
| `telemetry2_0` | `.backup_logs_done` | 60s | Skip PreviousLogs report; do NOT write completion sentinel |
| `uploadstblogs` | `stt_received` + `Update_rebootInfo_invoked` + `.telemetry_prevlogs_done` | 120s | **Annotate** missing prerequisites and **always proceed** |

### Upload Annotation Bitmask

| Bit | Constant | Meaning |
|:---:|----------|---------|
| 0 | `ANNOTATION_REBOOT_REASON_UNAVAILABLE` | Reboot reason absent after 120s |
| 1 | `ANNOTATION_NTP_UNAVAILABLE` | NTP absent and internet unreachable |

---

# Failure Scenario Summary

| # | Scenario | Trigger written? | Upload outcome |
|:-:|----------|:----------------:|----------------|
| S1 | `backup_logs` fails | No | **Aborted** — EXIT_FAILURE |
| S2 | Backup OK, reboot reason absent | `TRIGGER_REBOOT_INFO_UPDATE` | Proceeds + `REBOOT_REASON_UNAVAILABLE` |
| S3 | Backup OK, NTP + RI absent | `TRIGGER_REBOOT_INFO_UPDATE` | Proceeds + `NTP_FALLBACK` or `NTP_UNAVAILABLE` + `REBOOT_REASON_UNAVAILABLE` |
| S4 | Backup OK, telemetry absent | `TRIGGER_TELEMETRY_SCAN` | Proceeds + `TELEMETRY_UNAVAILABLE` |

> **Invariant**: Upload always occurs if `backup_logs` succeeded. Missing metadata is annotated, never silently discarded.

---

# Cross-Repo Release Constraint

Three repositories must be released **simultaneously** (REQ-SYNC-010):

| Repo | Change |
|------|--------|
| **dcm-agent** | `backup_logs` writes `.backup_logs_done`; `uploadstblogs` polls all three sentinels |
| **reboot-manager** | `update-prev-reboot-info` polls `.backup_logs_done` |
| **telemetry** | `telemetry2_0` polls `.backup_logs_done`; writes `.telemetry_prevlogs_done` |

### Partial deployment risks

- **telemetry not updated** → `uploadstblogs` times out on `.telemetry_prevlogs_done` → upload proceeds with annotation
- **reboot-manager not updated** → `Update_rebootInfo_invoked` not written in time → upload proceeds with annotation

No backward compatibility window — all three repos must deploy together.

---

# Implementation Task Groups

| Group | Scope | Key Tasks |
|:-----:|-------|-----------|
| **A** | Sentinel infrastructure | Define `BACKUP_LOGS_DONE_FLAG`, `PATH_FLAG_BACKUP_LOGS_DONE` constants |
| **B** | backup_logs sentinel write | Write `.backup_logs_done` on success (atomic `O_CREAT`) |
| **C** | reboot-manager poll | `poll_for_sentinel()` helper + gate `find_previous_reboot_log()` |
| **D** | uploadstblogs poll | Replace `sleep(330)` with triple-sentinel inotify-poll; trigger-then-poll; remove MM IARM events |
| **E** | Documentation | Cross-repo `DEPENDENCIES.md` in all three repos |
| **F** | Tests | Unit tests + L2 integration (sentinel chain end-to-end) |
| **G** | Telemetry repo | Poll `.backup_logs_done`; write `.telemetry_prevlogs_done` |

---

# Risks & Notes

- If both STT and internet are missing, upload proceeds but is annotated (`NTP_UNAVAILABLE`)
- Partial repo deployment degrades gracefully via annotations (soft gates) rather than blocking
- `inotify` is Linux-only — `#ifdef HAVE_INOTIFY` guard with `stat()` fallback preserves portability
- Cross-repo sentinel paths are **interface contracts** — any change requires coordinated simultaneous release
- The 120s `uploadstblogs` timeout exceeds telemetry's 60s internal timeout, guaranteeing the telemetry scan is complete or abandoned before `add_timestamp_to_files()` runs
- Distributed state machine: Each module owns its state, but uploadstblogs orchestrates the checks and triggers.
- Timeouts must be coordinated to avoid indefinite waits.
- All fallback uploads must clearly annotate what metadata was missing or defaulted.

---

# Log Upload: State Machine & Fallbacks

> **Fallback Method:** Log upload must always happen except when log backup fails. All missing/failed steps are annotated in the upload for diagnostics.

- DCM Agent synchronizes backup, STT, reboot reason, telemetry, and upload.
- Each module owns its state; DCM Agent triggers and coordinates as needed.
- State machine ensures robust fallback and retry logic.

👉 **[Logupload State Machine & Fallbacks](./logupload-state-machine.md)**

Refer to the linked file for the scenario breakdowns and state diagram.

---

# Design Principles

- **No dynamic memory allocation** — memory pools where needed
- **Platform-neutral** — portable across embedded targets
- **Thread-safe** — safe concurrent access
- **Fixed-point arithmetic** — no floating-point dependency
- **Secure coding** — input validation, no buffer overflows
- **Modular** — each subsystem independently testable

---

# Cross-Repo Interface Contract

The signal file `/tmp/.backup_logs_done` is a **3-repo interface**:

| Repo | Role | Action |
|------|------|--------|
| **dcm-agent** | Writer | `backup_logs` creates `.backup_logs_done` on success |
| **reboot-manager** | Consumer | Polls `.backup_logs_done` before reading PreviousLogs/ |
| **telemetry** | Consumer + Writer | Polls `.backup_logs_done`; writes `.telemetry_prevlogs_done` after grep scan |
| **dcm-agent** | Consumer | `uploadstblogs` polls `.telemetry_prevlogs_done` before archive |

- All path changes must be **coordinated across all three repos**
- Signal files reside in `/tmp/` — volatile, cleared on every reboot
- **Atomic 3-repo release** required — no backward compatibility window
- Telemetry changes tracked as **Group G** tasks in this change (TASK-G1 – TASK-G4)

---
layout: center
---

# The Telemetry Gap

**Discovery**: `telemetry2_0` reads PreviousLogs/ at boot — **outside the signal chain**

```mermaid
graph LR
  BL[backup_logs] -->|.backup_logs_done| RM[reboot-info]
  BL -->|.backup_logs_done| T2[telemetry2_0]
  RM -->|Update_rebootInfo_invoked| UL[uploadstblogs]
  T2 -->|.telemetry_prevlogs_done| UL

  style T2 fill:#E91E63,color:#fff
  style BL fill:#4CAF50,color:#fff
  style RM fill:#2196F3,color:#fff
  style UL fill:#FF9800,color:#fff
```

- `PERSIST_LOG_MON_REF` enabled on **all builds** — telemetry always scans PreviousLogs/
- PreviousLogs grep report is **fire-and-forget** — never retried
- If it reads while `add_timestamp_to_files()` is renaming, markers are **permanently lost**
- **Fix (now in scope — Group G)**:
  - Telemetry polls `.backup_logs_done` before grep scan (TASK-G1)
  - Telemetry writes `.telemetry_prevlogs_done` after scan (TASK-G2)
  - `uploadstblogs` polls `.telemetry_prevlogs_done` before calling `add_timestamp_to_files()` (TASK-D2)

---
layout: center
---

# Summary

Signal-file chain eliminates race conditions at boot

**backup_logs → { telemetry2_0, reboot-info } → uploadstblogs**

4 signal files: `.backup_logs_done` · `stt_received` · `Update_rebootInfo_invoked` · `.telemetry_prevlogs_done`

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
| RebootInfo     | No             | Trigger update, wait, proceed  | Yes             |
| TelemetryFlag  | No             | Trigger scan, wait, proceed    | Yes             |

---

## Risks & Notes

- If both STT and internet are missing, upload proceeds but is heavily annotated as incomplete.
- Distributed state machine: Each module owns its state, but uploadstblogs orchestrates the checks and triggers.
- Timeouts must be coordinated to avoid indefinite waits.
- All fallback uploads must clearly annotate what metadata was missing or defaulted.

---

# Thank You

[dcm-agent Repository](https://github.com/rdkcentral/dcm-agent)
---
