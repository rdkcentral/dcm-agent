# Boot Synchronisation State Machine

Cross-repo sentinel flow for dcm-agent · reboot-manager · telemetry.

## High-Level Flow

```mermaid
sequenceDiagram
    participant BL  as backup_logs
    participant RM  as reboot-manager
    participant TEL as telemetry
    participant DCM as dcm-agent
    participant UL  as uploadstblogs

    note over BL,UL: Boot

    BL  ->> BL  : assemble previous logs
    BL  -->> RM : inotify(.backup_logs_done)
    BL  -->> TEL: inotify(.backup_logs_done)

    note over BL,UL: Processing

    RM  ->> RM  : wait for backup complete and NTP ready
    RM  -->> UL : signal reboot info ready

    TEL ->> TEL : inotify wait .backup_logs_done
    TEL ->> TEL : inotify wait NTP sync indicator
    TEL ->> TEL : grep previous logs
    TEL -->> DCM: RBUS logupload event
    DCM -->> UL : trigger log upload

    note over BL,UL: uploadstblogs log upload

    UL  ->> UL  : check backup complete
    UL  ->> UL  : check NTP ready
    UL  ->> UL  : if NTP not synced and internet available - apply fallback time
    UL  ->> UL  : inotify(Update_rebootInfo_invoked) 120 s
    UL  -->> RM : re-trigger on timeout
    UL  ->> UL  : archive and upload
```

---

## State Machine

```mermaid
flowchart TB
    BOOT([Boot])

    BOOT --> BL[backup_logs]

    BL -->|inotify .backup_logs_done| RM[reboot-manager]
    BL -->|inotify .backup_logs_done| TEL[telemetry]

    RM  -->|write reboot info sentinel| W
    TEL -->|inotify NTP sync then RBUS logupload event| DCM[dcm-agent]

    subgraph UL [uploadstblogs log upload]
        direction TB
        S([start]) --> G1{"backup complete?"}
        G1 -- no  --> ABORT([ABORT])
        G1 -- yes --> G2{"NTP ready?"}
        G2 -- yes --> W{"inotify Update_rebootInfo_invoked 120s"}
        G2 -- no  --> G3{"internet available?"}
        G3 -- yes --> FALLBACK[apply fallback time]
        G3 -- no  --> ANNOT[annotate time unavailable]
        FALLBACK --> W
        ANNOT --> W
        W -- ok      --> DONE([archive and upload])
        W -- timeout --> TRIG[re-trigger reboot-manager and annotate]
        TRIG --> DONE
    end

    DCM -->|trigger| S
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
