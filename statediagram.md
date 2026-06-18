# Boot Synchronisation State Machine

Cross-repo sentinel flow for dcm-agent · reboot-manager · telemetry · systimemgr.

```mermaid
flowchart TD
    BOOT([System Boot]) --> SYS_START & BL_START & RM_START

    subgraph SYS["systimemgr"]
        SYS_START[Start] --> SYS1{NTP sync?}
        SYS1 -- yes --> SYS2[write\n/tmp/stt_received]
        SYS1 -- no  --> SYS3[update\n/opt/secure/clock.txt]
        SYS2 --> SYS3
    end

    subgraph BL["backup_logs  ·  dcm-agent"]
        BL_START[Assemble PreviousLogs] --> BL1[write\n/tmp/.backup_logs_done]
    end

    subgraph RM["reboot-manager  ·  update-prev-reboot-info"]
        RM_START[Start] --> RM1{"inotify wait\n.backup_logs_done\n⏱ 60 s"}
        RM1 -- present   --> RM2[read legacy sources\nkernel panic / pstore]
        RM1 -- timeout   --> RM2
        RM2 --> RM3{STT_FLAG\npresent?}
        RM3 -- yes --> RM4[update_reboot_info]
        RM3 -- no  --> RM5{TRIGGER_REBOOT\nINFO_UPDATE?}
        RM5 -- yes --> RM4
        RM5 -- no  --> RM_SKIP([skip])
        RM4 --> RM6[write\n/tmp/Update_rebootInfo_invoked]
        RM6 --> RM7[remove TRIGGER file]
    end

    subgraph TEL["telemetry  ·  dcautil.c  ·  getGrepResults"]
        TEL_START[called for PreviousLogs] --> TEL1{"inotify wait\n.backup_logs_done\n⏱ 60 s"}
        TEL1 -- present --> TEL2[grep PreviousLogs]
        TEL1 -- timeout --> TEL2
        TEL2 --> TEL3[write\n/tmp/.telemetry_prevlogs_done]
    end

    subgraph UL["uploadstblogs  ·  reboot_setup"]
        UL_START[reboot_setup] --> UL1{"stat\n.backup_logs_done"}
        UL1 -- absent  --> UL_ABORT1([ABORT — retry next trigger])
        UL1 -- present --> UL2{"dir_exists PreviousLogs\nhas_log_files"}
        UL2 -- no      --> UL_ABORT2([ABORT — no logs])
        UL2 -- yes     --> UL3{"stat\nSTT_FLAG"}
        UL3 -- absent  --> UL4{"check_internet\nIPv4 → IPv6\norg.rdk.NetworkManager\n:9998/jsonrpc"}
        UL4 -- connected    --> UL5["fopen /opt/secure/clock.txt\nsettimeofday(epoch)"]
        UL4 -- no internet  --> UL6[annotate\nANNOTATION_NTP_UNAVAILABLE]
        UL3 -- present --> UL7
        UL5 --> UL7
        UL6 --> UL7
        UL7{"inotify wait\nUpdate_rebootInfo_invoked\n⏱ 120 s"} -- found   --> UL_OK([proceed →\narchive / upload])
        UL7                                                       -- timeout --> UL8["touch /tmp/stt_received\n(trigger_reboot_info_update)"]
        UL8 --> UL9[annotate\nANNOTATION_REBOOT_REASON_UNAVAILABLE]
        UL9 --> UL_OK
    end

    BL1  -. "inotify" .-> RM1
    BL1  -. "inotify" .-> TEL1
    BL1  -. "stat"    .-> UL1
    SYS2 -. "stat"    .-> RM3
    SYS2 -. "stat"    .-> UL3
    UL8  -. "touch"   .-> RM3
    RM6  -. "inotify" .-> UL7
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
