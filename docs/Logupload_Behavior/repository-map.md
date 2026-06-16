# Repository Map — Log Upload Ecosystem

> Part of `Logupload_Behavior/` — see [README.md](README.md) for the full ecosystem map.

---

## Repositories Involved

| Repository | GitHub URL | Role in Log Upload |
|------------|-----------|-------------------|
| `dcm-agent` | https://github.com/rdkcentral/dcm-agent | **Core engine** — DCM daemon scheduler, uploadstblogs library/binary, backup_logs |
| `rdkservices` | https://github.com/rdkcentral/rdkservices | **Trigger layer** — MaintenanceManager, SystemServices (UploadLogs, deep sleep) |
| `reboot-manager` | https://github.com/rdkcentral/reboot-manager | **Boot coordination** — reboot reason derivation, signal file for uploadstblogs |
| `telemetry` | https://github.com/rdkcentral/telemetry | **Telemetry consumer/producer** — scans PreviousLogs, receives T2 markers |
| `iarmmgrs` | https://github.com/rdkcentral/iarmmgrs | **Event bus** — IARM bus infrastructure; maintenanceMGR.h defines event constants |

> **Note on `rdk-e`:** `https://github.com/rdk-e/` is an enterprise GitHub org
> (Comcast internal) that requires SSO. Scripts such as `Start_uploadSTBLogs.sh`,
> `uploadSTBLogs.sh`, and `update-prev-reboot-info` likely live there and are not
> publicly accessible. The C migration underway in `dcm-agent` is intended to make the
> critical logic independent of those private shell scripts.

---

## dcm-agent (rdkcentral/dcm-agent)

### Sub-component: `uploadstblogs`

| File | Role |
|------|------|
| `uploadstblogs/src/uploadstblogs.c` | Main entry; `uploadstblogs_run()`, `uploadstblogs_execute()`, lock management |
| `uploadstblogs/src/context_manager.c` | Builds `RuntimeContext` from device properties + RFC/TR-181 |
| `uploadstblogs/src/strategy_selector.c` | Early-return decisions, upload strategy selection |
| `uploadstblogs/src/strategy_handler.c` + `strategies.c` | Per-strategy execution workflows |
| `uploadstblogs/src/archive_manager.c` | Log collection, tar.gz creation, naming |
| `uploadstblogs/src/upload_engine.c` | HTTP(S) upload, retry logic, CodeBig fallback |
| `uploadstblogs/src/event_manager.c` | IARM event emission, T2 telemetry hooks |
| `uploadstblogs/src/rbus_interface.c` | TR-181 parameter reads via RBUS |
| `uploadstblogs/src/uploadlogsnow.c` | UploadLogsNow on-demand workflow |
| `uploadstblogs/include/uploadstblogs.h` | Public API: `uploadstblogs_run()`, `UploadSTBLogsParams` |
| `uploadstblogs/include/uploadstblogs_types.h` | `RuntimeContext`, `TriggerType`, `Strategy` enums |

### Sub-component: `backup_logs`

| File | Role |
|------|------|
| `backup_logs/src/backup_logs.c` | Orchestration, signal file writing |
| `backup_logs/src/backup_engine.c` | 4-level rotation (HDD-disabled) / timestamped (HDD-enabled) |
| `backup_logs/src/sys_integration.c` | systemd integration, config reading |
| `backup_logs/src/config_manager.c` | Configuration loading from RDK property APIs |
| `backup_logs/src/special_files.c` | Special file handling and inclusion rules |

### Sub-component: `dcm` (main daemon)

| File | Role |
|------|------|
| `dcm.c` | `dcmRunJobs()` — calls `uploadstblogs_run()` on scheduler fire |
| `dcm_parseconf.c` | Parses XConf-delivered DCM settings |
| `dcm_schedjob.c` | Scheduler wrapper (cron-based job management) |
| `dcm_rbus.c` | RBUS connection for T2 version query and event subscription |
| `dcm_cronparse.c` | Cron expression parser for schedule delivery from XConf |

### Key interfaces exposed

```c
// uploadstblogs/include/uploadstblogs.h
typedef struct {
    int         flag;
    int         dcm_flag;
    bool        upload_on_reboot;
    const char* upload_protocol;   // "HTTP" or "HTTPS"
    const char* upload_http_link;  // cloud endpoint URL
    TriggerType trigger_type;      // TRIGGER_SCHEDULED, TRIGGER_ONDEMAND, TRIGGER_REBOOT, ...
    bool        rrd_flag;
    const char* rrd_file;
} UploadSTBLogsParams;

int uploadstblogs_run(const UploadSTBLogsParams* params);  // preferred C API
int uploadstblogs_execute(int argc, char** argv);           // legacy argv API
```

---

## rdkservices (rdkcentral/rdkservices)

### Sub-component: `MaintenanceManager`

| File | Role |
|------|------|
| `MaintenanceManager/MaintenanceManager.cpp` | Task orchestration, IARM event handler, Thunder API endpoints |
| `MaintenanceManager/MaintenanceManager.h` | Class and state definitions |
| `MaintenanceManager/maintenanceMGR.h` (from iarmmgrs) | IARM event constants |

**Key constants:**

```cpp
#define LOGUPLOAD_TASK  BIN_PATH "Start_uploadSTBLogs.sh"   // /lib/rdk/Start_uploadSTBLogs.sh
#define TASK_TIMEOUT    /* minutes; per-task watchdog timer */
```

**API surface (Thunder JSON-RPC `org.rdk.MaintenanceManager.1`):**

| Method | Effect |
|--------|--------|
| `startMaintenance` | Triggers solicited maintenance cycle (RFC → FW update → log upload) |
| `stopMaintenance` | Aborts current maintenance (RFC-gated) |
| `getMaintenanceActivityStatus` | Returns status: IDLE/STARTED/COMPLETE/ERROR/INCOMPLETE |
| `setMaintenanceMode` | FOREGROUND/BACKGROUND + opt-out setting |
| `getMaintenanceStartTime` | Returns scheduled start time |

**Event emitted:**

```
onMaintenanceStatusChange { "maintenanceStatus": "MAINTENANCE_COMPLETE" }
```

### Sub-component: `SystemServices/UploadLogs`

| File | Role |
|------|------|
| `SystemServices/uploadlogs.cpp` | `logUploadAsync()`, `LogUploadBeforeDeepSleep()` |
| `SystemServices/uploadlogs.h` | Function declarations |
| `SystemServices/SystemServices.cpp` | Registers Thunder API endpoints that call into uploadlogs |

**RFC flags consumed:**

| RFC Parameter | Effect |
|--------------|--------|
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.MTLS.mTlsLogUpload.Enable` | Switch to mTLS/xPKI upload path |
| `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadBeforeDeepSleep.Enable` | Enable pre-deep-sleep upload |

---

## reboot-manager (rdkcentral/reboot-manager)

### Sub-component: `reboot-reason-fetcher` (formerly `reboot-checker`)

| Directory | Role |
|-----------|------|
| `reboot-reason-fetcher/src/` | Reads PreviousLogs, derives reboot reason, writes signal file |
| `reboot-reason-fetcher/include/` | Header declarations |

**Signal file written:** `/tmp/Update_rebootInfo_invoked`  
**Signal files polled:** `/tmp/.backup_logs_done`, `stt_received`

### Sub-component: `reboot-helper`

| Directory | Role |
|-----------|------|
| `reboot-helper/src/` | Coordinates early-boot helper tasks |
| `reboot-helper/include/` | Header declarations |

### Sub-component: `rebootnow` (binary)

Handles software-initiated reboots. Relevant to log upload because:
- Classifies reboot reason (written to `/opt/secure/reboot/reboot.info`)
- Signals `telemetry2_0` and `parodus` during pre-reboot cleanup
- Writes `previousreboot.info` for cyclic reboot detection

**Classification categories used by log upload metadata:**

| Class | Example triggers |
|-------|-----------------|
| `APP_TRIGGERED` | HtmlDiagnostics, XRE, browser restart |
| `OPS_TRIGGERED` | Remote management, ACS |
| `MAINTENANCE_REBOOT` | Firmware update, maintenance window |
| `FIRMWARE_FAILURE` | Crash path, watchdog |

---

## telemetry (rdkcentral/telemetry)

### `telemetry2_0` daemon

| Directory | Role |
|-----------|------|
| `source/bulkdata/` | Profile and marker management |
| `source/scheduler/` | Report scheduling and upload |
| `source/xconf-client/` | Fetch telemetry profiles from XConf |
| `source/dcautil/` | Log marker scanner (PreviousLogs scan) |

**Signal file written:** `/tmp/.telemetry_prevlogs_done`  
**Signal files polled:** `/tmp/.backup_logs_done`

**T2 RBUS data model:**

| RBUS Parameter | Effect |
|---------------|--------|
| `Device.X_RDKCENTRAL-COM_T2.ReportProfiles` | Persistent telemetry profiles |
| `Device.X_RDKCENTRAL-COM_T2.Temp_ReportProfiles` | Temporary profiles |
| `Device.X_RDKCENTRAL-COM_T2.UploadDCMReport` | Trigger on-demand report upload |

**Important:** `telemetry2_0` uploads telemetry **reports** (analytics data) — not the
same as `uploadstblogs` which uploads raw log files. They use different endpoints.

---

## iarmmgrs (rdkcentral/iarmmgrs)

Provides the IARM bus event infrastructure. The `maintenance/include/` subdirectory
contains `maintenanceMGR.h` which defines:

```c
// maintenanceMGR.h (from iarmmgrs)
#define IARM_BUS_MAINTENANCE_MGR_NAME "MaintenanceMGR"
#define IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE  0

typedef enum {
    MAINT_RFC_COMPLETE       = (1 << 0),
    MAINT_RFC_ERROR          = (1 << 1),
    MAINT_LOGUPLOAD_COMPLETE = (1 << 2),   // = 4
    MAINT_LOGUPLOAD_ERROR    = (1 << 3),   // = 5 (approx)
    MAINT_FWDOWNLOAD_COMPLETE,
    MAINT_FWDOWNLOAD_ERROR,
    MAINT_REBOOT_REQUIRED,
    MAINT_FWDOWNLOAD_ABORTED,
    MAINT_CRITICAL_UPDATE,
    MAINT_RFC_INPROGRESS,
    MAINT_FWDOWNLOAD_INPROGRESS,
    MAINT_LOGUPLOAD_INPROGRESS = 16,       // = 16
} IARM_Maint_module_status_t;
```

Both `uploadstblogs` (sender) and `MaintenanceManager` (receiver) depend on these
constants for correctness.

---

## Configuration Sources (Cross-Cutting)

| Source | What It Provides | Consumer |
|--------|-----------------|---------|
| XConf cloud service | `LogUploadSettings:UploadRepository:URL`, protocol, schedule, `UploadOnReboot` | `dcm-agent` → DCM settings file |
| DCM settings file `/tmp/DCMSettings.conf` | Upload URL, protocol, UploadOnReboot flag | `uploadstblogs`, `SystemServices/uploadlogs.cpp` |
| RFC / TR-181 | mTLS flag, deep sleep flag, stop maintenance, RDKV FW upgrader | `uploadstblogs`, `MaintenanceManager`, `SystemServices` |
| `/etc/device.properties` | `DEVICE_TYPE`, `ENABLE_MAINTENANCE`, `BUILD_TYPE`, `FORCE_MTLS` | `uploadstblogs`, `SystemServices` |
| `/etc/dcm.properties` or `/opt/dcm.properties` | `LOG_SERVER` (TFTP server, legacy) | `SystemServices/uploadlogs.cpp` |

---

## Interface Contracts Between Repos

These are the precise contracts that must be maintained across the three repos involved
in the boot-time upload sequence:

### dcm-agent ↔ reboot-manager

| Contract | Details |
|----------|---------|
| `backup_logs` writes | `/tmp/.backup_logs_done` present = PreviousLogs is fully populated |
| `reboot-reason-fetcher` reads | PreviousLogs data (log content for reason derivation) |
| `reboot-reason-fetcher` writes | `/tmp/Update_rebootInfo_invoked` = metadata written to disk |

### dcm-agent ↔ telemetry

| Contract | Details |
|----------|---------|
| `backup_logs` writes | `/tmp/.backup_logs_done` present = PreviousLogs is fully populated |
| `telemetry2_0` reads | PreviousLogs log content for marker extraction |
| `telemetry2_0` writes | `/tmp/.telemetry_prevlogs_done` = marker scan complete |
| `uploadstblogs` writes | T2 markers via `t2_event_d()` / `t2_event_s()` |

### dcm-agent ↔ rdkservices (MaintenanceManager)

| Contract | Details |
|----------|---------|
| `uploadstblogs` writes | IARM event on `IARM_BUS_MAINTENANCE_MGR_NAME` bus |
| Event values | `MAINT_LOGUPLOAD_COMPLETE=4`, `MAINT_LOGUPLOAD_ERROR=5`, `MAINT_LOGUPLOAD_INPROGRESS=16` |
| `MaintenanceManager` expects | Exactly ONE terminal event (COMPLETE or ERROR) per upload invocation |
| Breaking change risk | Changing event values or bus name requires synchronized release |
