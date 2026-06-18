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

    note over SYS,UL: ── telemetry triggers dcm-agent ──────────────────

    TEL  -->> DCM : RBUS event: logupload trigger
    DCM  ->>  UL  : start reboot_setup

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
    BOOT([Boot])

    BOOT --> SYS & BL

    subgraph SYS [systimemgr]
        SYS_NTP([NTP sync]) -->|touch| STT[/stt_received/]
        SYS_NTP -->|write| CLK[/clock.txt/]
    end

    subgraph BL [backup_logs]
        BL_ASSEMBLE([assemble PreviousLogs]) -->|write| BLD[/.backup_logs_done/]
    end

    BLD -->|inotify 60 s| RM_RUN
    BLD -->|inotify 60 s| TEL_RUN
    STT -->|gate| RM_RUN

    subgraph RM [reboot-manager]
        RM_RUN([update reboot info]) -->|write| RBI[/Update_rebootInfo_invoked/]
    end

    subgraph TEL [telemetry]
        TEL_RUN([grep PreviousLogs]) -->|RBUS event| DCM([dcm-agent])
    end

    DCM -->|trigger| UL_START
    BLD -->|stat gate| UL_GATE

    subgraph UL [uploadstblogs · reboot_setup]
        UL_START([reboot_setup]) --> UL_GATE{.backup_logs_done\npresent?}
        UL_GATE -- no  --> ABORT([ABORT])
        UL_GATE -- yes --> NTP_CHK{stt_received\npresent?}
        NTP_CHK -- no  --> INET([check internet\napply clock.txt\nor annotate])
        NTP_CHK -- yes --> RBI_WAIT
        INET --> RBI_WAIT
        RBI_WAIT{wait Update_rebootInfo_invoked\n120 s} -- ok      --> DONE([archive / upload])
        RBI_WAIT                                        -- timeout --> TRIG([touch stt_received\nannotate])
        TRIG --> DONE
    end

    RBI -->|inotify| RBI_WAIT
    CLK -->|fallback| INET
    TRIG -.->|re-trigger| RM_RUN
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
