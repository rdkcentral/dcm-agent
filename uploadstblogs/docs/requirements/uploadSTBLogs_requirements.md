# Requirements – Migration of `uploadSTBLogs.sh` to C

## 1. Functional Scope
The C migration must replicate the shell script’s logic for conditional log packaging and upload:
- Early decision branch (RRD flag, privacy mode, no previous logs, continue).
- Strategy selection (Non-DCM, OnDemand, Reboot, DCM).
- Archive creation (timestamp adjustments, packaging, optional DRI and PCAP inclusion).
- Transport selection with fallback (Direct vs CodeBig).
- Authentication (mTLS for Direct, OAuth for CodeBig).
- Retry and fallback handling.
- Verification → cleanup + notification (events + telemetry).
- Security layer (cert handling, TLS/MTLS, optional OCSP validation).
- Support modules: configuration, log collection, file ops, event emission.

## 2. Inputs

| Source | Description | Type |
|--------|-------------|------|
| CLI Args | 1:TFTP_SERVER, 2:FLAG, 3:DCM_FLAG, 4:UploadOnReboot, 5:UploadProtocol, 6:UploadHttpLink, 7:TriggerType, 8:RRD_FLAG, 9:RRD_UPLOADLOG_FILE | Strings / ints |
| Environment Files | `/etc/include.properties`, `/etc/device.properties` | Key/value |
| Sourced Scripts | `$RDK_PATH/utils.sh`, `$RDK_PATH/logfiles.sh`, optional `t2Shared_api.sh`, `exec_curl_mtls.sh` | Functions |
| TR-181 / RFC | Endpoint URL, encryption enable, privacy mode, unscheduled reboot disable, remote debugger issue type | Dynamic config |
| File System | Previous logs directory, block marker files, OCSP marker files, reboot reason file | State |
| Runtime | Uptime, time-of-day, network reachability, curl/TLS exit codes | Dynamic |

## 3. Outputs

| Output | Description |
|--------|-------------|
| Archive `.tgz` | Packaged logs (main, DRI optional) |
| Upload Result | Success / failure / aborted events |
| Telemetry Counters | Success, failure, curl error, cert error, fallback engaged |
| Block Markers | Files marking blocked direct or CodeBig path |
| Cleanup Effects | Removal of temp archive, pruning old timestamped logs |

## 4. Dependencies & Interfaces

| Dependency | Purpose |
|------------|---------|
| TR-181 accessor | Fetch RFC and endpoint values |
| Curl / libcurl | HTTPS pre-sign & upload |
| OpenSSL (optional) | MD5 checksum (if encryption flag) |
| Event sender binary | Emit IARM events |
| Tar/Gzip facility | Create archive (streamed) |
| Time / stat syscalls | Block marker age, timestamp logic |

## 5. Constraints

| Area | Constraint |
|------|-----------|
| Performance | Minimize process spawning; stream archive creation |
| Memory | Low footprint (< few MB); fixed buffers |
| CPU | Compression acceptable; avoid heavy hashing beyond MD5 |
| Portability | POSIX C; avoid shell-only constructs |
| Security | Privacy abort must prevent data exposure; TLS enforced |
| Reliability | Deterministic fallback and retries; safe early exits |
| Concurrency | Single-instance lock via flock |

## 6. Edge Cases

| Edge Case | Requirement |
|-----------|-------------|
| RRD flag set | Bypass normal strategies, upload specific file |
| Privacy DO_NOT_SHARE | Truncate logs; no upload; emit abort event |
| No previous logs | Early exit; emit no-logs event |
| HTTP 404 pre-sign | Terminal failure (no retry) |
| Both paths blocked | Immediate failure |
| Curl timeout | Retry if attempts remain |
| Cert error codes | Log + telemetry; may retry |
| OnDemand trigger | No timestamp renaming for persistent logs |
| Uptime < threshold for reboot | Delay (sleep) with abort awareness |

## 7. Error Handling

| Domain | Approach |
|--------|---------|
| Archive creation | Abort strategy; failure event |
| Pre-sign request | Evaluate HTTP code; 200 proceed, 404 terminate, else retry/fallback |
| Upload transfer | Retry if permissible else fallback/fail |
| TLS cert error | Telemetry + potential retry |
| Missing dirs/files | Early fail with event |
| Signal abort | Set aborted state; cleanup gracefully |

## 8. Security & Privacy

- Mask signatures before logging URLs.
- Enforce TLSv1.2 minimum.
- OCSP stapling conditional via markers.
- Privacy abort truncates files (O_TRUNC) and stops further processing.

## 9. Observability

- Log each stage (strategy chosen, path selected, attempt counts, HTTP codes).
- Telemetry counters keyed to success, failure, fallback, curl and cert errors.
  
## 10. Migration Non-Functional Requirements

| Requirement | Description |
|-------------|-------------|
| Diagram Alignment | Modules limited strictly to diagram nodes |
| Deterministic Flow | Linear transitions matching diagram |
| Minimal Abstractions | No extra managers beyond represented nodes |
| Maintainability | Clear strategy and path decisions |

## 11. Acceptance Criteria

| Criterion | Pass Condition |
|-----------|----------------|
| Strategy Fidelity | All diagram branches executed correctly |
| Upload Success | Archive sent, events & telemetry populated |
| Fallback Behavior | Alternate path used when primary fails (non-terminal) |
| Privacy Enforcement | Logs truncated; no upload |
| Block Logic | Honors block durations; sets markers appropriately |
| Single Instance | Second invocation blocked by lock |
| TLS Error Logging | Cert errors recorded & counted |

## 12. Non-Scope

| Item | Reason |
|------|--------|
| Additional protocols (SCP/MQTT) | Not in diagram |
| Extended telemetry taxonomy | Keep minimal per diagram |
| Complex plugin architecture | Unnecessary for current mapping |
