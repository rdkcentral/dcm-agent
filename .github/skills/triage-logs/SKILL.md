---
name: triage-logs
description: >
  Triage any dcm-agent behavioral issue on RDK devices by correlating device
  log bundles with source code. Covers daemon hangs, log upload failures,
  DCM configuration errors, uploadSTBLogs failures, backup_logs issues,
  USB log upload problems, RBUS communication errors, and cron scheduling
  issues. The user states the issue; this skill guides systematic root-cause
  analysis regardless of issue type.
---

# Log Triage Skill

## Purpose

Systematically correlate device log bundles with dcm-agent source code to
identify root causes, characterize impact, and propose unit-test and
functional-test reproduction scenarios — for **any** behavioral anomaly reported
by the user.

---

## Usage

Invoke this skill when:
- A device log bundle is available under `logs/` (or attached separately)
- The user describes a behavioral anomaly (examples: DCM daemon not starting,
  log upload failures, configuration parsing errors, upload retry loops,
  authentication failures, backup logs not working, USB upload issues, cron
  job scheduling problems, RBUS communication failures)
- You need to write a reproduction scenario for an existing or proposed fix

**The user's stated issue drives the investigation.** Do not assume a specific
failure mode — read the issue description first, then follow the steps below.

---

## Step 1: Orient to the Log Bundle

**Log bundle layout** (typical RDK device):
```
logs/<MAC>/<SESSION_TIMESTAMP>/logs/
    dcm.log.0                      ← Primary DCM daemon log (start here)
    uploadstblogs.log.0            ← uploadSTBLogs log upload execution
    dcmscript.log.0                ← DCM script execution logs
    backup_logs.log.0              ← Log backup operations
    usb_logupload.log.0            ← USB log upload operations
    messages.txt.0                 ← System messages
    top_log.txt.0                  ← CPU/memory snapshots
    /opt/logs/                     ← Actual log files being uploaded
    /nvram/DCMresponse.txt         ← DCM configuration from XConf
    /nvram/dcm.properties          ← DCM settings
```

Include any log files surfaced by the user's issue description.

**Log timestamp prefix format**: `YYMMDD-HH:MM:SS` or RFC3339
- Session folder names are **local-time snapshots** (format: `MM-DD-YY-HH:MMxM`)
- Log lines use device local time

---

## Step 2: Map Daemon Startup and Components

Read the startup section of `dcm.log.0` (first ~50 lines) to identify:

| What to find | Log pattern |
|---|---|
| Daemon start | `DCM daemon starting` or `dcmDaemonMainInit` |
| Configuration loaded | `DCMresponse.txt` parsing |
| RBUS initialization | `rbus_open` or `RBUS_Initialize` |
| Cron job scheduling | `dcm_cronparse` or cron expression parsing |
| Log upload schedule | `DCM_LOG_UPLOAD` schedule setup |
| Firmware update schedule | `DCM_FW_UPDATE` schedule setup |

**Key components in dcm-agent**:
- Main daemon (`dcmd`) — initialization, configuration, RBUS, cron scheduling
- uploadSTBLogs — log collection, archiving, upload execution
- uploadLogsNow — on-demand log upload trigger
- backup_logs — log backup and rotation
- usbLogUpload — USB-based log upload
- dcm_rbus — RBUS interface for remote control

---

## Step 3: Identify the Anomaly Window

Based on the **user's stated issue**, search for the relevant evidence pattern:

### DCM Daemon Not Starting / Crashes
```bash
grep -n "dcmDaemonMainInit\|ERROR\|FATAL\|Segmentation\|core dump" dcm.log.0
grep -n "dcmd" messages.txt.0 | tail -50
```
Check for:
- Configuration file missing or malformed (`/nvram/DCMresponse.txt`)
- RBUS initialization failure
- Memory allocation failures
- Dependency library missing (rbus, curl, ssl)

### Log Upload Failures
```bash
grep -n "uploadSTBLogs\|upload\|ERROR\|HTTP\|curl\|Failed" uploadstblogs.log.0
grep -n "S3\|presign\|mTLS\|OAuth\|authentication" uploadstblogs.log.0
```
Look for:
- HTTP status codes (4xx client errors, 5xx server errors)
- Curl error codes
- Authentication failures (certificate errors, OAuth token issues)
- Pre-sign request failures
- Network connectivity issues
- Retry exhaustion

### Configuration Parsing Errors
```bash
grep -n "dcm_parseconf\|parse\|ERROR\|Invalid" dcm.log.0
cat /nvram/DCMresponse.txt  # Check configuration format
```
Verify:
- JSON/XML syntax validity
- Required fields present (URL, schedule)
- Upload protocol configuration (HTTP, HTTPS)
- Authentication settings

### Cron Scheduling Issues
```bash
grep -n "dcm_cronparse\|cron\|schedule\|ERROR" dcm.log.0
```
Check:
- Cron expression validity
- Schedule parsing errors
- Job execution timing
- Missed schedule windows

### RBUS Communication Errors
```bash
grep -n "rbus\|RBUS_ERROR\|connection\|method" dcm.log.0
```
Verify:
- RBUS daemon (rtrouted) running
- Method registration success
- Event subscription success
- Method invocation errors

### Upload Strategy Issues
```bash
grep -n "strategy\|RRD\|OnDemand\|Reboot\|DCM\|Non-DCM" uploadstblogs.log.0
```
Identify:
- Which strategy was selected
- Strategy selection logic
- Trigger conditions met/not met
- Early abort conditions (privacy mode, no logs)

### Archive/Packaging Failures
```bash
grep -n "archive\|tar\|gzip\|packaging\|collection" uploadstblogs.log.0
```
Check for:
- Disk space issues
- File permission errors
- Tar/gzip failures
- Log file collection errors

---

## Step 4: Correlate with Source Code

Map log evidence to source files:

| Issue Area | Source Files |
|---|---|
| Daemon initialization | `dcm.c`, `dcm_parseconf.c` |
| RBUS interface | `dcm_rbus.c` |
| Cron parsing | `dcm_cronparse.c` |
| Job scheduling | `dcm_schedjob.c` |
| Configuration parsing | `dcm_parseconf.c` |
| uploadSTBLogs main logic | `uploadstblogs/src/uploadstblogs.c` |
| Upload strategies | `uploadstblogs/src/strategy_*.c`, `uploadstblogs/src/strategy_selector.c` |
| Upload engine | `uploadstblogs/src/upload_engine.c` |
| Retry logic | `uploadstblogs/src/retry_logic.c` |
| Archive management | `uploadstblogs/src/archive_manager.c` |
| Authentication | `uploadstblogs/src/` (mTLS/OAuth handling) |
| RBUS interface | `uploadstblogs/src/rbus_interface.c` |
| On-demand upload | `uploadstblogs/src/uploadlogsnow.c` |

### Example: Upload Failure Correlation

If logs show:
```
ERROR: HTTP 403 Forbidden - pre-sign request failed
ERROR: retry_logic: Max retries exhausted for Direct path
```

1. Check `uploadstblogs/src/upload_engine.c` for pre-sign logic
2. Check `uploadstblogs/src/retry_logic.c` for retry configuration
3. Verify authentication configuration in `/nvram/DCMresponse.txt`
4. Check certificate paths and OAuth token generation

---

## Step 5: Reproduce Locally

Create a minimal reproduction scenario:

### For Configuration Issues
```c
// Test configuration parsing
#include "dcm_parseconf.h"

void test_parse_bad_config() {
    DCMDHandle handle = {};
    // Create test config with issue
    FILE *f = fopen("/tmp/test_dcmresponse.txt", "w");
    fprintf(f, "{invalid json}");
    fclose(f);
    
    int ret = dcmParseConfig(&handle, "/tmp/test_dcmresponse.txt");
    // Should fail gracefully
    assert(ret != 0);
}
```

### For Upload Issues
```bash
# Test uploadSTBLogs manually
export LOG_PATH=/opt/logs/
export PERSISTENT_PATH=/opt/
export DCM_FLAG=1
export UploadOnReboot=1

# Run with debug logging
DEBUG=1 ./uploadstblogs 2>&1 | tee upload_debug.log
```

### For RBUS Issues
```bash
# Check RBUS daemon
systemctl status rtrouted

# Test RBUS method invocation
rbuscli get Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.DCM.Enable
```

---

## Step 6: Test Gap Analysis

Identify untested code paths that could harbor the bug:

### Check Unit Test Coverage
```bash
# Generate coverage report
./configure --enable-gcov
make clean && make check
gcov *.c
```

Look for:
- Error path coverage in suspected functions
- Configuration parsing edge cases
- Network error handling
- Retry logic branches
- Strategy selection conditions

### Check L2 Test Coverage
Review `test/functional-tests/tests/` for:
- Missing test scenarios matching the bug
- Edge cases not covered
- Error injection tests

---

## Step 7: Propose Fix and Test

### Fix Template
```c
// BEFORE: Missing error check
int ret = upload_to_s3(archive_path);
// Continue without checking ret

// AFTER: Proper error handling
int ret = upload_to_s3(archive_path);
if (ret != 0) {
    DCM_LOG_ERROR("Upload failed: %d", ret);
    // Trigger retry logic or fallback
    return handle_upload_error(ret);
}
```

### Test Template
```cpp
// Add unit test for the fix
TEST(UploadEngineTest, HandleUploadFailureGracefully) {
    // Mock upload failure
    EXPECT_CALL(mockCurl, curl_easy_perform(_))
        .WillOnce(Return(CURLE_COULDNT_CONNECT));
    
    int ret = upload_to_s3("test.tgz");
    
    // Verify error handling
    EXPECT_NE(ret, 0);
    // Verify cleanup happened
    EXPECT_FALSE(file_exists("test.tgz"));
}
```

---

## Output Format

Present findings in this structure:

```markdown
## Triage Summary

**Issue:** <user's stated problem>
**Evidence:** <key log excerpts with timestamps>
**Root Cause:** <likely cause based on code analysis>
**Impact:** <what fails, user-visible effect>

## Code Location

**File:** <source file path>
**Function:** <function name>
**Line:** <approximate line number>

## Reproduction

[bash or C code to reproduce]

## Proposed Fix

[code diff or description]

## Test Coverage

**Existing:** [what tests exist]
**Missing:** [tests needed to prevent regression]

## Next Steps

1. [immediate action]
2. [follow-up verification]
```

---

## Example Triage Flow

**User:** "uploadSTBLogs keeps trying to upload but fails with HTTP 403"

**Step 1:** Located `uploadstblogs.log.0`, found repeated:
```
2026-03-24 10:15:32 ERROR: Pre-sign request failed: HTTP 403 Forbidden
2026-03-24 10:15:42 INFO: Retry attempt 2/5
2026-03-24 10:15:52 ERROR: Pre-sign request failed: HTTP 403 Forbidden
```

**Step 2:** Checked `/nvram/DCMresponse.txt` — found OAuth token field empty

**Step 3:** In `uploadstblogs/src/upload_engine.c`, pre-sign logic doesn't validate
OAuth configuration before attempting request

**Root Cause:** Missing validation of OAuth token before making pre-sign request

**Fix:** Add validation in `prepare_upload_request()`:
```c
if (auth_type == AUTH_TYPE_OAUTH && !config->oauth_token) {
    DCM_LOG_ERROR("OAuth token not configured");
    return ERR_INVALID_CONFIG;
}
```

**Test:** Add `TEST(UploadEngineTest, RejectMissingOAuthToken)`
