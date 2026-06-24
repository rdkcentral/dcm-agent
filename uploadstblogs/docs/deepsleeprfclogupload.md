# DeepSleep RFC Logupload Workflow

## Scope

This document captures the exact runtime flow when the power state transitions from ON to LIGHT_SLEEP in `entservices-systemservices`, and how that invocation is handled by `dcm-agent/uploadstblogs`.

It includes both RFC cases for:

`Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadBeforeDeepSleep.Enable`

## Source Paths

- `rdkcentral/entservices-systemservices`:
  - `plugin/SystemServicesImplementation.cpp`
  - `plugin/uploadlogs.cpp`
- `rdkcentral/dcm-agent`:
  - `uploadstblogs/src/uploadstblogs.c`
  - `uploadstblogs/src/strategy_selector.c`
  - `uploadstblogs/src/strategies.c`
  - `uploadstblogs/src/context_manager.c`

## Trigger Point in SystemServices

When power mode changes, `SystemServicesImplementation::OnSystemPowerStateChanged(...)` checks:

- New state is `LIGHT_SLEEP` (or `STANDBY`)
- Previous state is `ON`

Then it reads RFC:

`RFC_LOG_UPLOAD = Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadBeforeDeepSleep.Enable`

Behavior:

- If RFC value is boolean `true` -> calls `UploadLogsAsync(result)`
- If RFC is missing, read fails, wrong type, or value is `false` -> no upload is started from this path

Also, on transition to `DEEP_SLEEP`, SystemServices aborts an active upload process if one is running.

## Exact UploadLogsAsync -> logupload Flow

`UploadLogsAsync()` in SystemServices:

1. Checks if another upload PID is tracked.
2. If running, calls `AbortLogUpload(...)`.
3. Starts a new upload via `UploadLogs::logUploadAsync()`.

`logUploadAsync()` (`plugin/uploadlogs.cpp`) builds and executes:

- Binary: `/usr/bin/logupload`
- Via `fork()` + `execve()`

Argument vector used:

1. `/usr/bin/logupload`
2. `<tftp_server>`
3. `0`        (FLAG)
4. `1`        (DCM_FLAG)
5. `false`    (UploadOnReboot)
6. `<upload_protocol>`
7. `<upload_httplink>`
8. `1`
9. `false`

## How dcm-agent Interprets This Invocation

`uploadstblogs` binary entry:

- `main()` -> `uploadstblogs_execute(argc, argv)`
- acquires `/tmp/.log-upload.lock`
- initializes context
- parses args
- chooses strategy via `early_checks(...)`

Important for this call:

- `FLAG = 0`
- `DCM_FLAG = 1`

In `strategy_selector.c`, this selects `STRAT_DCM`.

## Which Function Uploads and Which Folder Is Uploaded

### Upload function

For this path, actual upload is done by:

- `upload_archive(ctx, session, archive_path)`

called from:

- `dcm_upload(...)` in `uploadstblogs/src/strategies.c`

### Folder packaged into archive

For this path, archive input directory is:

- `ctx->dcm_log_path`

used in:

- `dcm_archive(...)` -> `create_archive(ctx, session, ctx->dcm_log_path)`

`ctx->dcm_log_path` resolution from `context_manager.c`:

- Reads `DCM_LOG_PATH` from `/etc/device.properties`
- If absent, defaults to:

`/tmp/DCM/`

Therefore, for the SystemServices deep-sleep RFC-triggered call, logs are uploaded from:

- configured `DCM_LOG_PATH`
- default fallback: `/tmp/DCM/`

## RFC True vs False: Exact Behavior

### Case A: RFC is true

Condition:

- Transition is `ON` -> `LIGHT_SLEEP`
- RFC read succeeds
- RFC boolean value is `true`

Result:

- `UploadLogsAsync()` is called
- `/usr/bin/logupload` is executed
- dcm-agent selects `STRAT_DCM`
- archive is created from `ctx->dcm_log_path` (default `/tmp/DCM/`)
- upload performed through `upload_archive(...)`

### Case B: RFC is false

Condition:

- Transition is `ON` -> `LIGHT_SLEEP`
- RFC value is `false` (or read/type validation does not satisfy true branch)

Result:

- `UploadLogsAsync()` is not called by this power-state RFC gate
- `/usr/bin/logupload` is not started from this specific path
- no DCM archive/upload is triggered from this RFC path

## Deep Sleep Transition Note

When state becomes `DEEP_SLEEP`, SystemServices checks active upload PID and aborts if running. This affects an in-flight upload started earlier, but does not change the true/false gate behavior at ON -> LIGHT_SLEEP.

## Mode State Diagram (Power + RFC Gate)

```mermaid
stateDiagram-v2
  [*] --> ON

  ON --> LIGHT_SLEEP: Power change ON->LIGHT_SLEEP
  state LIGHT_SLEEP {
    [*] --> EvaluateRFC
    EvaluateRFC --> StartUpload: RFC_LOG_UPLOAD == true
    EvaluateRFC --> NoUpload: RFC_LOG_UPLOAD != true
  }

  StartUpload --> UploadRunning: UploadLogsAsync() + execve(/usr/bin/logupload)
  NoUpload --> IdleInLightSleep: No upload from RFC gate

  LIGHT_SLEEP --> DEEP_SLEEP: Power change to DEEP_SLEEP
  UploadRunning --> UploadAborted: AbortLogUpload() on DEEP_SLEEP

  DEEP_SLEEP --> LIGHT_SLEEP: Wake path
  LIGHT_SLEEP --> ON: Resume active mode

  UploadAborted --> [*]
```

## Combined Workflow Diagram (rdkservices + dcm-agent)

```mermaid
flowchart LR
  subgraph RDK[entservices-systemservices repo]
    PM[Power Manager Event]
    S1[SystemServicesImplementation::OnSystemPowerStateChanged]
    R1[getRFCParameter RFC_LOG_UPLOAD]
    U1[UploadLogsAsync]
    U2[UploadLogs::logUploadAsync]
    EX[execve /usr/bin/logupload]
    AB[AbortLogUpload on DEEP_SLEEP]
  end

  subgraph DCM[dcm-agent repo]
    M1[uploadstblogs main]
    E1[uploadstblogs_execute]
    L1[Acquire /tmp/.log-upload.lock]
    C1[init_context + parse args]
    S2[early_checks -> STRAT_DCM]
    A1[dcm_archive create_archive ctx->dcm_log_path]
    A2[dcm_upload upload_archive]
  end

  PM --> S1
  S1 --> R1
  R1 -->|true| U1
  R1 -->|false or invalid| N1[No upload from RFC path]
  U1 --> U2 --> EX --> M1 --> E1 --> L1 --> C1 --> S2 --> A1 --> A2
  S1 -->|state becomes DEEP_SLEEP| AB

  D1[Source folder from DCM_LOG_PATH or default /tmp/DCM/] --> A1
```

## End-to-End Sequence

```mermaid
sequenceDiagram
    participant PM as Power Manager
    participant SS as SystemServicesImplementation
    participant RFC as RFC Store
    participant UL as uploadlogs.cpp
    participant LU as /usr/bin/logupload
    participant DCM as dcm-agent uploadstblogs

    PM->>SS: OnSystemPowerStateChanged(ON, LIGHT_SLEEP)
    SS->>RFC: getRFCParameter(RFC_LOG_UPLOAD)

    alt RFC_LOG_UPLOAD == true
        SS->>UL: UploadLogsAsync()
        UL->>UL: logUploadAsync()
        UL->>LU: fork + execve(/usr/bin/logupload ...)
        LU->>DCM: uploadstblogs_execute(argc, argv)
        DCM->>DCM: early_checks -> STRAT_DCM
        DCM->>DCM: create_archive(ctx->dcm_log_path)
        DCM->>DCM: upload_archive(...)
    else RFC_LOG_UPLOAD != true
        SS-->>SS: Do not start upload from this gate
    end

    PM->>SS: OnSystemPowerStateChanged(*, DEEP_SLEEP)
    SS->>SS: AbortLogUpload() if PID is active
```
