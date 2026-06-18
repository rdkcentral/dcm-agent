# Boot Synchronisation State Machine

Cross-repo sentinel flow for dcm-agent · reboot-manager · telemetry · systimemgr.

## High-Level Flow

```mermaid
sequenceDiagram
    participant SYS as systimemgr
    participant BL  as backup_logs
    participant RM  as reboot-manager
    participant TEL as telemetry
    participant DCM as dcm-agent
    participant UL  as uploadstblogs

    note over SYS,UL: Boot

    SYS ->> SYS : NTP sync → write clock.txt
    SYS -->> RM : touch stt_received

    BL  ->> BL  : assemble PreviousLogs
    BL  -->> RM : write .backup_logs_done
    BL  -->> TEL: inotify .backup_logs_done

    note over SYS,UL: Processing

    RM  ->> RM  : wait .backup_logs_done + stt_received
    RM  -->> UL : write Update_rebootInfo_invoked

    TEL ->> TEL : wait .backup_logs_done → grep PreviousLogs
    TEL -->> DCM: RBUS logupload event
    DCM -->> UL : trigger reboot_setup

    note over SYS,UL: uploadstblogs reboot_setup

    UL  ->> UL  : stat .backup_logs_done → absent? ABORT
    UL  ->> UL  : stat stt_received → absent?
    UL  ->> UL  :   check internet → read clock.txt → settimeofday
    UL  ->> UL  : wait Update_rebootInfo_invoked (120 s)
    UL  -->> RM : [timeout] touch stt_received
    UL  ->> UL  : archive → upload
```

---

## State Machine

```mermaid
flowchart TB
    BOOT([Boot])

    BOOT --> SYS[systimemgr]
    BOOT --> BL[backup_logs]

    SYS -->|touch| STT[stt_received]
    SYS -->|write| CLK[clock.txt]
    BL  -->|write| BLD[backup_logs_done]

    STT -->|gate| RM[reboot-manager]
    BLD -->|inotify| RM
    BLD -->|inotify| TEL[telemetry]

    RM  -->|write| RBI[Update_rebootInfo_invoked]
    TEL -->|RBUS event| DCM[dcm-agent]

    subgraph UL [uploadstblogs reboot_setup]
        direction TB
        S([start]) --> G1{"backup_logs_done present?"}
        G1 -- absent  --> ABORT([ABORT])
        G1 -- present --> G2{"stt_received present?"}
        G2 -- absent  --> NTP[check internet and apply clock.txt]
        G2 -- present --> W{"wait Update_rebootInfo_invoked 120s"}
        NTP --> W
        W -- ok      --> DONE([archive and upload])
        W -- timeout --> TRIG[touch stt_received and annotate]
        TRIG --> DONE
    end

    DCM -->|trigger| S
    RBI -->|inotify| W
    CLK -->|fallback| NTP
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
