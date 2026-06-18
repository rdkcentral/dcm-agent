# Boot Synchronisation State Machine

Cross-repo sentinel flow for dcm-agent · reboot-manager · telemetry · systimemgr.

## High-Level Flow

```mermaid
sequenceDiagram
    participant SYS  as systimemgr
    participant BL   as backup_logs
    participant RM   as reboot-manager
    participant TEL  as telemetry
    participant UL   as uploadstblogs

    note over SYS,UL: ── Boot ──────────────────────────────────────────

    SYS  ->> SYS  : NTP sync attempt
    SYS  -->> SYS : write /opt/secure/clock.txt (last-known-good)
    SYS  -->> RM  : [NTP ok] touch /tmp/stt_received

    BL   ->> BL   : assemble PreviousLogs
    BL   -->> RM  : write /tmp/.backup_logs_done
    BL   -->> TEL : (same file — inotify)
    BL   -->> UL  : (same file — stat gate)

    note over SYS,UL: ── Parallel consumers ────────────────────────────

    RM   ->> RM   : inotify wait .backup_logs_done (60 s)
    RM   ->> RM   : wait STT_FLAG / trigger
    RM   -->> UL  : write /tmp/Update_rebootInfo_invoked

    TEL  ->> TEL  : inotify wait .backup_logs_done (60 s)
    TEL  ->> TEL  : grep PreviousLogs
    TEL  -->> UL  : write /tmp/.telemetry_prevlogs_done

    note over SYS,UL: ── uploadstblogs reboot_setup ────────────────────

    UL   ->> UL   : stat .backup_logs_done → absent? ABORT
    UL   ->> UL   : stat stt_received → absent?
    UL   ->> UL   :   check internet (org.rdk.NetworkManager)
    UL   ->> SYS  :   read /opt/secure/clock.txt → settimeofday
    UL   ->> UL   : inotify wait Update_rebootInfo_invoked (120 s)
    UL   -->> RM  : [timeout] touch /tmp/stt_received → re-trigger RM
    UL   ->> UL   : archive → upload
```

---

## State Machine

```mermaid
flowchart TD
    BOOT([Boot]) --> SYS & BL

    SYS["systimemgr\n─────────────────\nNTP sync → stt_received\nupdate clock.txt"]
    BL["backup_logs\n─────────────────\nassemble PreviousLogs\n→ .backup_logs_done"]

    BL -->|inotify| RM["reboot-manager\n─────────────────\nwait .backup_logs_done\nwait stt_received\n→ Update_rebootInfo_invoked"]

    BL -->|inotify| TEL["telemetry\n─────────────────\nwait .backup_logs_done\ngrep PreviousLogs\n→ .telemetry_prevlogs_done"]

    BL -->|stat gate| UL_GATE{"uploadstblogs\nreboot_setup\n─────────────────\n.backup_logs_done\npresent?"}

    UL_GATE -- no  --> ABORT([ABORT])
    UL_GATE -- yes --> UL_NTP{"stt_received\npresent?"}

    UL_NTP -- no  --> UL_TIME["check internet\n→ apply clock.txt\nor annotate NTP_UNAVAILABLE"]
    UL_NTP -- yes --> UL_WAIT

    UL_TIME --> UL_WAIT{"wait Update_rebootInfo_invoked\n⏱ 120 s"}

    UL_WAIT -- ok      --> UPLOAD([archive → upload])
    UL_WAIT -- timeout --> UL_RETRIG["touch stt_received\nannotate REBOOT_REASON_UNAVAILABLE"]
    UL_RETRIG --> UPLOAD

    SYS  -. stt_received .-> UL_NTP
    RM   -. inotify      .-> UL_WAIT
    UL_RETRIG -. re-trigger .-> RM
```

## Sentinel Reference

| File | Written by | Consumed by | Mechanism |
|---|---|---|---|
| `/tmp/.backup_logs_done` | `backup_logs` (dcm-agent) | reboot-manager, telemetry, uploadstblogs | inotify wait / stat gate |
| `/tmp/stt_received` | `systimemgr` (on NTP sync) | reboot-manager (STT gate), uploadstblogs (NTP check); **touched by uploadstblogs** on reboot-reason timeout | stat / touch |
| `/tmp/Update_rebootInfo_invoked` | `reboot-manager` | uploadstblogs | inotify wait |
| `/opt/secure/clock.txt` | `systimemgr` (RdkDefaultTimeSync) | uploadstblogs (last-known-good time fallback) | fopen / fgets |
| `/tmp/.telemetry_prevlogs_done` | `telemetry` (dcautil.c) | uploadstblogs (stat — TASK-G2 pending) | stat |

## Annotation Codes (`SessionState.upload_annotations` bitmask)

| Bit | Constant | Meaning |
|---|---|---|
| 0 | `ANNOTATION_REBOOT_REASON_UNAVAILABLE` | `Update_rebootInfo_invoked` absent after 120 s; upload proceeds without confirmed reboot reason |
| 1 | `ANNOTATION_NTP_UNAVAILABLE` | NTP absent and internet unreachable; archive timestamps use raw system clock |
