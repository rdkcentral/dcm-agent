# Callers and Triggers of Log Upload

> Part of `Logupload_Behavior/` — see [README.md](README.md) for the full ecosystem map.

---

## Overview

Log upload in RDKE is triggered from multiple independent paths across at least three
repositories. Each path carries different parameters and semantics.

---

## Trigger 1 — DCM Agent Scheduler (Periodic / XConf-scheduled)

**Repository:** `rdkcentral/dcm-agent`  
**File:** `dcm.c` → `dcmRunJobs()`  
**Interface:** C library API `uploadstblogs_run()`

### How it works

The DCM daemon (`dcmd`) receives a configuration bundle from XConf at boot. This bundle
includes a cron-style schedule for log uploads. The built-in scheduler fires the
`DCM_LOGUPLOAD_SCHED` job name when the time arrives, which calls directly into the
`uploadstblogs` shared library.

```c
// dcm.c — dcmRunJobs()
if (strcmp(profileName, DCM_LOGUPLOAD_SCHED) == 0) {
    UploadSTBLogsParams params = {
        .flag           = 0,
        .dcm_flag       = 1,
        .upload_on_reboot = false,
        .upload_protocol  = pPrctl,   // from XConf
        .upload_http_link = pURL,     // from XConf
        .trigger_type     = TRIGGER_SCHEDULED,
        .rrd_flag         = false,
        .rrd_file         = NULL
    };
    int result = uploadstblogs_run(&params);
}
```

### Parameters sourced from XConf (via DCM settings file)

| XConf field | Maps to | Notes |
|------------|---------|-------|
| `LogUploadSettings:UploadRepository:uploadProtocol` | `upload_protocol` | `HTTP` or `HTTPS` |
| `LogUploadSettings:UploadRepository:URL` | `upload_http_link` | Cloud endpoint URL |
| `LogUploadSettings:UploadOnReboot` | `upload_on_reboot` | Boolean |
| Cron expression | Schedule | Parsed by `dcm_cronparse` |

### Blocking condition

If `dcmSettingsGetMMFlag()` returns true (Maintenance Manager enabled), this path is
suppressed — the Maintenance Manager owns the schedule instead.

---

## Trigger 2 — Maintenance Manager (Scheduled Maintenance Window)

**Repository:** `rdkcentral/rdkservices`  
**File:** `MaintenanceManager/MaintenanceManager.cpp` → `task_execution_thread()`  
**Interface:** Shell script invocation via `system()`

### How it works

The MaintenanceManager (a Thunder/WPEFramework plugin) orchestrates a fixed sequence of
three maintenance tasks:

```
1.  /usr/bin/rfcMgr >> /opt/logs/rfcscript.log
2.  /lib/rdk/swupdate_utility.sh
3.  /lib/rdk/Start_uploadSTBLogs.sh        ← log upload
```

Each task is run sequentially. `Start_uploadSTBLogs.sh` wraps the `logupload` binary or the
legacy `uploadSTBLogs.sh` and passes the DCM-sourced configuration as arguments.

```cpp
// MaintenanceManager.cpp
string task_names_foreground[] = {
    "/usr/bin/rfcMgr >> /opt/logs/rfcscript.log",
    "/lib/rdk/swupdate_utility.sh",
    "/lib/rdk/Start_uploadSTBLogs.sh"    // ← uploadstblogs
};
// Each task is launched with: system(task.c_str())
```

### Completion signaling

`Start_uploadSTBLogs.sh` emits IARM events over `IARM_BUS_MAINTENANCE_MGR_NAME` when it
finishes. The MaintenanceManager waits for `MAINT_LOGUPLOAD_COMPLETE` or
`MAINT_LOGUPLOAD_ERROR` before advancing its state machine.

### When it fires

- **Unsolicited (on-boot):** triggered automatically when the device comes online.
- **Solicited (on-demand):** triggered via `startMaintenance` Thunder API call.
- Guard condition: only fires when `internetConnectStatus == true`.

### RFC control

```
Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StopMaintenance.Enable
```
When this RFC is `true`, `stopMaintenance` API can abort the running upload task.

---

## Trigger 3 — SystemServices UploadLogs API (On-demand from App)

**Repository:** `rdkcentral/rdkservices`  
**File:** `SystemServices/uploadlogs.cpp`  
**Interface:** Thunder JSON-RPC `org.rdk.SystemServices.uploadLogs`

### How it works

Apps and the RDK UI layer call the SystemServices plugin via JSON-RPC. SystemServices
reads DCM configuration and calls the log upload binary:

```cpp
// uploadlogs.cpp — logUploadAsync()
const char *argArray[] = {
    "/bin/sh",
    "/lib/rdk/uploadSTBLogs.sh",
    tftp_server.c_str(),
    "0",                     // FLAG
    "1",                     // DCM_FLAG
    "0",                     // UploadOnReboot
    upload_protocol.c_str(),
    upload_httplink.c_str(),
    "1",                     // calledFromPlugin
    0
};
pid_t pid = fork();
if (pid == 0) execve(argArray[0], (char**)argArray, environ);
```

Upload parameters are read from:
- `/etc/dcm.properties` or `/opt/dcm.properties` → `LOG_SERVER`
- DCM settings temp file `/tmp/DCMSettings.conf` → protocol and URL
- RFC `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.MTLS.mTlsLogUpload.Enable`
  → controls mTLS / xPKI path selection

### mTLS handling

If `FORCE_MTLS=true` in `/etc/device.properties` or the mTLS RFC is enabled, the HTTP
URL is modified:

```cpp
// append /secure/cgi-bin in place of /cgi-bin
upload_httplink = regex_replace(httplink, regex("cgi-bin"), "secure/cgi-bin");
```

---

## Trigger 4 — SystemServices Deep Sleep Pre-Upload

**Repository:** `rdkcentral/rdkservices`  
**File:** `SystemServices/uploadlogs.cpp` → `LogUploadBeforeDeepSleep()`  
**Interface:** Called internally when device enters deep sleep

### How it works

```cpp
// Checks RFC flag first
if (!checkLogUploadBeforeDeepSleepFlag()) return E_NOK;

string cmd = "nice -n 19 /bin/busybox sh /lib/rdk/uploadSTBLogs.sh "
           + tftp_server + " 0 1 0 "
           + upload_protocol + " " + upload_httplink
           + " 1 & \0";
system(cmd.c_str());
```

### RFC control

```
Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadBeforeDeepSleep.Enable
```

When `false` (default), this path is skipped entirely. When `true`, the device uploads
logs at reduced priority (`nice -n 19`) immediately before entering deep sleep.

---

## Trigger 5 — UploadLogsNow (Immediate On-Demand Mode)

**Repository:** `rdkcentral/dcm-agent`  
**File:** `uploadstblogs/src/uploadlogsnow.c` → `execute_uploadlogsnow_workflow()`  
**Interface:** `uploadstblogs uploadlogsnow` (special argv[1] value)

### How it works

When the binary is invoked with `argv[1] == "uploadlogsnow"`, a separate workflow runs
that:
1. Copies current logs to a staging area (excluding `dcm/`, `PreviousLogs/`, `PreviousLogs_backup/`)
2. Creates a tar.gz archive
3. Uploads to the configured cloud endpoint
4. Writes human-readable status to `/tmp/logUploadStatus.txt`

```c
// uploadstblogs.c — parse_args()
if (argc >= 2 && strcmp(argv[1], "uploadlogsnow") == 0) {
    ctx->trigger_type        = TRIGGER_ONDEMAND;
    ctx->uploadlogsnow_mode  = true;
    ctx->dcm_flag            = 1;
    ctx->upload_on_reboot    = 1;
}
```

### Who calls this

- `SystemServices` Thunder plugin → on `org.rdk.SystemServices.1.uploadLogs` JSON-RPC
- Manual operator invocation
- App-layer via Thunder → SystemServices → uploadlogsnow mode

### Behavioural difference from other triggers

- Skips IARM event emission (no MaintenanceManager state changes)
- Writes `logUploadStatus.txt` for UI polling
- Does not skip upload even if `UploadOnReboot=false`

---

## Trigger 6 — Post-Reboot Upload (Boot-Time Sequence)

**Repositories:** `rdkcentral/dcm-agent` (backup_logs + uploadstblogs), `rdkcentral/reboot-manager`  
**Interface:** Signal files in `/tmp/`

### Boot-time ordering constraint

This is the most complex trigger. Four independent processes must complete in order before
`uploadstblogs` can upload the previous boot's logs:

```
boot
 │
 ├─► backup_logs (C binary, runs early in boot)
 │       writes: /tmp/.backup_logs_done
 │
 ├─► reboot-manager/reboot-reason-fetcher (a.k.a. update-prev-reboot-info)
 │       waits for: /tmp/.backup_logs_done + stt_received
 │       writes: /tmp/Update_rebootInfo_invoked
 │
 ├─► telemetry2_0 daemon
 │       waits for: /tmp/.backup_logs_done
 │       scans: $LOG_PATH/PreviousLogs/ for telemetry markers
 │       writes: /tmp/.telemetry_prevlogs_done
 │
 └─► uploadstblogs reboot_setup()
         polls ALL THREE:
           /tmp/Update_rebootInfo_invoked
           /tmp/.telemetry_prevlogs_done
           stt_received
         timeout: 120 seconds (clock_gettime CLOCK_MONOTONIC)
         then: executes upload with TRIGGER_REBOOT
```

### Known race conditions (defect history)

| Defect | Symptom | Root Cause |
|--------|---------|------------|
| XIONE-18338 | `UploadOnReboot=false` even when XConf offers `true` | DCM config delivery races boot upload trigger |
| DELIA-70285 | Wrong reboot reason recorded | `update-prev-reboot-info` reads PreviousLogs before `backup_logs` completes |
| XIONE-18607 | Wrong filename timestamp for days | `uploadstblogs` archives before NTP clock sync |

---

## Trigger Comparison Table

| Trigger | Who | Interface | Trigger Type | IARM Events | Lock |
|---------|-----|-----------|--------------|-------------|------|
| DCM scheduler | dcm-agent `dcmd` | C library `uploadstblogs_run()` | `TRIGGER_SCHEDULED` | yes | yes |
| MaintenanceManager | rdkservices Thunder plugin | Shell script | `TRIGGER_SCHEDULED` | yes | yes |
| SystemServices API | rdkservices Thunder plugin | `fork+execve` shell | `TRIGGER_ONDEMAND` | no | yes |
| Deep sleep | rdkservices Thunder plugin | `system()` shell | `TRIGGER_ONDEMAND` | no | yes |
| UploadLogsNow | app → SystemServices | `uploadstblogs uploadlogsnow` | `TRIGGER_ONDEMAND` | no | yes |
| Post-reboot | boot sequence | binary after signal file poll | `TRIGGER_REBOOT` | yes | yes |

All paths use the `/tmp/.log-upload.lock` flock guard (matching the legacy shell `flock -n`)
to prevent concurrent uploads.
