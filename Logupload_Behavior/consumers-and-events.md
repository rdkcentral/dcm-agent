# Consumers and Events — Who Reads Log Upload Outputs

> Part of `Logupload_Behavior/` — see [README.md](README.md) for the full ecosystem map.

---

## Overview

`uploadstblogs` produces outputs on three channels: IARM bus events, T2 telemetry markers,
and filesystem artifacts (uploaded files, status files). Multiple other RDKE components
subscribe to these outputs.

---

## Channel 1 — IARM Bus Events

### Event bus: `IARM_BUS_MAINTENANCE_MGR_NAME`

These events drive the MaintenanceManager state machine in `rdkcentral/rdkservices`.

| IARM Event Constant | Integer | Meaning |
|---------------------|---------|---------|
| `MAINT_LOGUPLOAD_INPROGRESS` | 16 | Lock acquired; another instance already running |
| `MAINT_LOGUPLOAD_COMPLETE` | 4 | Upload finished successfully or privacy-aborted |
| `MAINT_LOGUPLOAD_ERROR` | 5 | Upload failed after all retries |

**Emitted from:** `uploadstblogs/src/event_manager.c`

```c
// event_manager.c — send_iarm_event_maintenance()
send_iarm_event_maintenance(MAINT_LOGUPLOAD_COMPLETE);   // success
send_iarm_event_maintenance(MAINT_LOGUPLOAD_ERROR);       // failure
send_iarm_event_maintenance(MAINT_LOGUPLOAD_INPROGRESS); // lock blocked
```

**Consumed by:** `MaintenanceManager.cpp` → `iarmEventHandler()`

```cpp
// MaintenanceManager.cpp — iarmEventHandler()
case MAINT_LOGUPLOAD_COMPLETE:
    SET_STATUS(g_task_status, LOGUPLOAD_SUCCESS);
    SET_STATUS(g_task_status, LOGUPLOAD_COMPLETE);
    task_thread.notify_one();   // advance to next maintenance task / completion
    break;
case MAINT_LOGUPLOAD_ERROR:
    SET_STATUS(g_task_status, LOGUPLOAD_COMPLETE);
    task_thread.notify_one();   // treat error as done; don't block maintenance
    break;
case MAINT_LOGUPLOAD_INPROGRESS:
    m_task_map[task_names_foreground[2].c_str()] = true;
    break;
```

**MaintenanceManager state machine outcome:**

```
All 3 tasks complete → MAINTENANCE_COMPLETE (or MAINTENANCE_INCOMPLETE/ERROR)
     ↓
onMaintenanceStatusChange() fired
     ↓
Thunder event "onMaintenanceStatusChange" emitted to JSON-RPC subscribers
     ↓
App layer / RDK UI receives completion notification
```

### Event bus: `IARM_BUS_SYSMGR_NAME`

| Event | Value | Meaning |
|-------|-------|---------|
| `LogUploadEvent` with data 0 | success | Emitted on successful upload via normal (non-uploadlogsnow) path |
| `LogUploadEvent` with data 1 | failed | Upload failed |
| `LogUploadEvent` with data 2 | aborted | Privacy mode aborted the upload |

**Note:** `LogUploadEvent` is skipped when `ctx->uploadlogsnow_mode == true`.

---

## Channel 2 — T2 Telemetry Markers

**Repository:** `rdkcentral/telemetry`  
**Header:** `<telemetry_busmessage_sender.h>` (linked when `-DT2_EVENT_ENABLED`)

`uploadstblogs` is a telemetry event *producer*. It notifies `telemetry2_0` of upload
outcomes via the T2 bus. These markers are collected by the telemetry daemon, batched into
reports, and uploaded to the cloud analytics backend.

```c
// uploadstblogs.c — t2_count_notify() / t2_val_notify()
#ifdef T2_EVENT_ENABLED
void t2_count_notify(char *marker) { t2_event_d(marker, 1); }
void t2_val_notify(char *marker, char *val) { t2_event_s(marker, val); }
#endif
```

| Marker | Type | When emitted |
|--------|------|--------------|
| `SYST_INFO_lu_success` | count | Upload succeeded (any path) |
| (other markers TBD) | varies | From strategy_handler / upload_engine per failure mode |

`telemetry2_0` is initialized and torn down around the upload:
```c
t2_init("uploadstblogs");    // before execution
// ... upload ...
t2_uninit();                  // after execution
```

### Telemetry's own upload mechanism

`telemetry2_0` has **its own internal log upload** mechanism for telemetry reports —
separate from the general log upload. It can be triggered by:
- SIGUSR1 / signal 10 → `LOG_UPLOAD` with seekmap reset
- Signal 29 (SIGIO) → `LOG_UPLOAD_ONDEMAND` without seekmap reset
- SIGUSR2 / signal 12 → XConf configuration reload

These are **not** managed by `uploadstblogs`. They go to a different endpoint
(telemetry analytics backend, not the general log server).

---

## Channel 3 — Filesystem Artifacts

### Uploaded archive

The primary output: a `.tar.gz` file uploaded to the cloud endpoint. After successful
upload the archive is deleted from local storage. The file naming convention uses:
- MAC address of the device
- Timestamp (NTP-synchronized — source of XIONE-18607 defect if NTP is not yet synced)
- Device type / platform identifiers

### Lock file

`/tmp/.log-upload.lock` — created with `open()` + `flock(LOCK_EX | LOCK_NB)`. Released
on process exit. Multiple concurrent callers (e.g., MaintenanceManager and DCM daemon)
respect this lock.

### Status file (UploadLogsNow mode only)

`/tmp/logUploadStatus.txt` — written by `uploadlogsnow.c`:

```c
// uploadlogsnow.c — write_upload_status()
FILE* fp = fopen(STATUS_FILE, "w");
fprintf(fp, "%s %s\n", message, timebuf);
```

Contains a human-readable status string with timestamp. Polled by the app layer / UI
through SystemServices to display upload progress.

---

## Channel 4 — RBUS (TR-181 parameter reads)

`uploadstblogs` is a *consumer* of RBUS parameters, not a producer. It reads runtime
configuration via `rbus_interface.c`:

```c
rbus_open(&g_rbusHandle, "UploadSTBLogs");
rbus_get(g_rbusHandle, param_name, &paramValue);
```

TR-181 parameters read at runtime:

| Parameter | Source component | Used for |
|-----------|-----------------|---------|
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.MTLS.mTlsLogUpload.Enable` | RFC manager | Select mTLS upload path |
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadBeforeDeepSleep.Enable` | RFC manager | Deep sleep trigger gate |
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AutoReboot.Enable` | RFC manager | Maintenance flow |
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StopMaintenance.Enable` | RFC manager | Allow `stopMaintenance` API |
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RDKFirmwareUpgrader.Enable` | RFC manager | Maintenance mode change during active maintenance |

---

## Consumer Dependency Map

```
uploadstblogs
    │
    ├── IARM: MAINT_LOGUPLOAD_COMPLETE/ERROR/INPROGRESS
    │       └──► MaintenanceManager (rdkservices)
    │               └──► Thunder event: onMaintenanceStatusChange
    │                       └──► App layer / UI (JSON-RPC subscribers)
    │
    ├── IARM: LogUploadEvent (0/1/2)
    │       └──► SysMgr / system status consumers
    │
    ├── T2 telemetry markers
    │       └──► telemetry2_0 daemon (rdkcentral/telemetry)
    │               └──► XConf analytics backend (periodic reports)
    │
    ├── Uploaded .tar.gz file
    │       └──► Cloud log server (Comcast/RDK backend)
    │               └──► Ops team / log analysis tooling
    │
    └── /tmp/logUploadStatus.txt (UploadLogsNow only)
            └──► SystemServices → app/UI layer
```

---

## What Happens When Upload Fails

### Lock contention (`MAINT_LOGUPLOAD_INPROGRESS`)

Another upload instance holds the lock. `uploadstblogs` returns immediately:
- Sends `MAINT_LOGUPLOAD_INPROGRESS` (value 16) to IARM bus
- MaintenanceManager marks LOGUPLOAD task map as in-progress (does not advance)

### Upload failure after retries (`MAINT_LOGUPLOAD_ERROR`)

All direct and CodeBig fallback attempts exhausted:
- Sends `MAINT_LOGUPLOAD_ERROR` to IARM bus
- MaintenanceManager's `g_task_status` marks LOGUPLOAD_COMPLETE (error path)
- Maintenance cycle ends with `MAINTENANCE_ERROR` or `MAINTENANCE_INCOMPLETE`
- `SYST_INFO_lu_success` marker NOT emitted

### Privacy mode (`MAINT_LOGUPLOAD_COMPLETE`)

Privacy opt-out active (log directory cleared):
- Sends `MAINT_LOGUPLOAD_COMPLETE` (same as success)
- No archive created, no upload attempted
- Maintenance state machine advances normally
