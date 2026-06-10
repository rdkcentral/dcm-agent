# L2 Test Coverage Report - DCM Agent Repository

Date: 2026-06-09
Mode: Coverage cross-check against module functions and functional-test assets under test/functional-tests/

## Summary

Total source functions (approx): ~167
Functions with direct L2 coverage: ~81
Functions with indirect L2 coverage: ~31
Functions with no L2 coverage: ~55

Active L2 test functions: 158
Disabled L2 test functions: 0
Active feature scenarios: 69
Proposed new test scenarios: 24

- High priority: 10
- Medium priority: 9
- Low priority: 5

Test files active: 19
Test files disabled (commented out): 0

Estimated current L2 functional coverage: ~67%
Target L2 functional coverage: ~80%

## Basis For These Numbers

1. Total source functions is a repository-wide approximation based on function prototypes exposed in module header files, used as a stable proxy for the functional surface reviewed by L2 tests.
2. Direct coverage means the function area is explicitly exercised by one or more pytest cases and matching feature scenarios in test/functional-tests/.
3. Indirect coverage means the function area is not asserted in isolation but is exercised transitively through broader boot, configuration, upload, or cleanup flows.
4. No coverage means no strong evidence was found in the current pytest and feature inventory that the function area is exercised by L2 functional tests.

## Cross-Check Scope

This review covers all current functional-test assets under test/functional-tests/ for these submodules:

1. DCM core
2. backup_logs
3. uploadstblogs
4. usbLogUpload

## Cross-Check Snapshot By Submodule

| Submodule | Approx source functions reviewed | Pytest L2 test functions | Feature scenarios | Directly covered functions (approx) | Indirectly covered functions (approx) | No coverage (approx) |
|---|---:|---:|---:|---:|---:|---:|
| DCM core | 36 | 31 | 7 | 23 | 7 | 6 |
| backup_logs | 20 | 50 | 38 | 14 | 4 | 2 |
| uploadstblogs | 97 | 70 | 17 | 38 | 17 | 42 |
| usbLogUpload | 14 | 7 | 7 | 6 | 3 | 5 |
| Total | 167 | 158 | 69 | 81 | 31 | 55 |

## Detailed Pytest Coverage By Submodule

### dcmd

Pytest files reviewed: 7

1. test_start_dcm-agent.py
	Validates daemon startup, default configuration fetch, waiting-for-telemetry behavior, duplicate-start protection, and teardown.
2. test_bootup_sequence.py
	Validates RBUS event subscriptions, telemetry communication, receipt of Setconfig and Processconfig events, scheduler start, and parser output for protocol, URL, timezone, reboot flag, and cron values.
3. test_existence_of_dcmsettingsFile.py
	Checks presence of the DCM settings file and maintenance configuration file expected by the daemon.
4. test_log_upload_onreboot_true_case.py
	Covers upload-on-reboot enabled behavior, cron presence, firmware cron scheduling, upload cron scheduling, firmware update launch, and upload launch.
5. test_log_upload_onreboot_false_case.py
	Covers upload-on-reboot disabled flow, script-start behavior, and coexistence of upload and firmware cron scheduling.
6. test_log_upload_onreboot_MM_case.py
	Covers maintenance-manager-enabled behavior together with upload and firmware cron scheduling.
7. test_log_upload_cron_NULL_case.py
	Covers null-cron handling and validates that the upload path is not triggered incorrectly.

Current strength:

1. Strong boot and configuration-path validation.
2. Good coverage of reboot flag and scheduling behavior.
3. Partial but meaningful coverage of RBUS/telemetry interaction through daemon logs and end-to-end behavior.

Main gaps:

1. Negative-path validation for RBUS initialization and reconnection.
2. Detailed cron parser edge conditions.
3. Scheduler stop/remove and malformed-config branches.

### backuplog

Pytest files reviewed: 4

1. test_backup_engine.py
	Covers HDD-enabled strategy, HDD-disabled four-slot rotation, last_reboot marker creation, exclusion of backup_logs.log, and file pattern matching for .txt, .log, and bootlog files.
2. test_backuplog_config_manager.py
	Covers configuration loading, missing or invalid property handling, validation defaults, and reload-oriented configuration behavior.
3. test_backuplogs_system_integration.py
	Covers initialization success, backup execution lifecycle, strategy selection logging, completion logging, systemd notification attempts, cleanup behavior, and repeated-run rotation evolution.
4. test_backuplogs_special_files.py
	Covers special file configuration parsing, comments and empty-line handling, max-entry limits, copy/move behavior for named files, initialization logging, execute-all completion, and graceful handling of missing source files.

Current strength:

1. Good behavioral coverage across engine, config, integration, and special-file processing.
2. Strong regression value around backup rotation semantics.
3. Good validation of operational logging and graceful handling of missing files.

Main gap:

1. These pytest assets exist in the repository but are not currently executed by the active L2 runner scripts.

### uploadstblogs

Pytest files reviewed: 6 general uploadstblogs files, plus 1 uploadLogsNow-specific file documented separately below.

1. test_uploadstblogs_error_handling.py
	Covers corrupted device properties, malformed configuration, config error logging, invalid-config no-upload behavior, expected exit code behavior, oversized-file handling, partial-upload behavior, empty-log conditions, and telemetry/logging on error.
2. test_uploadstblogs_normal_upload.py
	Covers normal upload initialization and large-file collection behavior.
3. test_uploadstblogs_retry_logic.py
	Covers network failures, retry count, retry delay, interruption recovery, HTTP 500 handling, failure telemetry, server-error logging, and exit-code behavior.
4. test_uploadstblogs_security.py
	Covers mTLS certificate loading, valid certificate flow, telemetry markers, invalid server certificate rejection, handshake failure logging, missing client certificate handling, path traversal prevention, and symlink attack prevention.
5. test_uploadstblogs_resource_management.py
	Covers temporary archive cleanup, lock-file removal, file-handle closure, orphaned resource detection, cleanup on failure, memory reasonableness, no-leak expectations, memory release after completion, heavy-load behavior, and concurrent request locking.
6. test_uploadstblogs_upload_strategies.py
	Covers on-demand upload, reboot upload, DCM-scheduled upload, RBUS-triggered flows, strategy selection, parameter combinations, telemetry, and strategy logging.

Current strength:

1. Broadest behavioral spread in the repository across normal, error, retry, security, resource, and strategy dimensions.
2. Good focus on operational risk areas such as network errors, certificate issues, and concurrency.
3. Useful end-to-end validation of strategy selection and RBUS-triggered flows.

Main gaps:

1. Several tests validate behavior through logs and process return codes rather than direct state inspection.
2. One strategy-path test can skip at runtime when rbuscli is unavailable.
3. Some internal helper functions remain only indirectly covered.

### usblogupload

Pytest files reviewed: 1

1. test_usb_logupload.py
	Covers missing log path, archive creation attempts, MAC-address/file logging path, temporary-directory cleanup, successful USB upload completion, invalid command usage, and invalid mount-point behavior.

Current strength:

1. Good entry-point validation for CLI usage and mount-path outcomes.
2. Includes both success and failure return-code checks.

Main gaps:

1. Assertions on archive contents and cleanup side effects are still relatively shallow.
2. Coverage is concentrated in a single pytest file.

### loguploadnow

Pytest files reviewed: 1

1. test_uploadLogsNow.py
	Covers immediate-trigger behavior, RFC endpoint configuration through RBUS CLI, context initialization, test-log creation, upload success verification through logs and status files, and archive-processing evidence.

Current strength:

1. Good targeted validation of the on-demand upload trigger path.
2. Stronger success verification than simple process-exit assertions because it also checks log and status evidence.

Main gaps:

1. Depends on environment readiness for RFC endpoint setup and command availability.
2. Shares implementation surface with uploadstblogs, so some failures will still only be covered transitively.

## Evidence Reviewed

1. Functional pytest files under test/functional-tests/tests/: 19 active files
2. Feature files under test/functional-tests/features/: 18 files with 69 active scenarios
3. Commented-out pytest test functions found: 0
4. Explicit skip markers found: 1 conditional runtime skip in test_uploadstblogs_upload_strategies.py when rbuscli is unavailable

## Important CI Note

The summary above is based on all pytest and feature files added under test/functional-tests/.

Current CI execution is narrower:

1. test/run_l2.sh executes 7 DCM-core test files
2. test/run_uploadstblogs_l2.sh executes 7 uploadstblogs test files and 1 usbLogUpload test file
3. backup_logs has functional-test assets in the repository, but those files are not currently invoked by the existing L2 runner scripts

Because of that, repository-present L2 coverage is materially higher than current CI-executed L2 coverage.

## Gap Areas Driving The Proposed New Scenarios

### High Priority

1. backup_logs runner integration gaps, so existing coverage becomes CI-active
2. uploadstblogs uncovered APIs around retry, RBUS-trigger edge cases, and failure-path handling
3. DCM core negative paths for RBUS, cron parsing, scheduling, and fallback configuration behavior

### Medium Priority

1. usbLogUpload failure and environment edge cases
2. uploadstblogs resource cleanup and concurrent-trigger combinations not explicitly asserted today
3. Orphan feature-to-pytest mappings where scenarios exist but do not prove full API-surface exercise

### Low Priority

1. Additional platform-variation checks
2. Non-critical fallback and diagnostic branches
3. Expanded observability assertions around logs and status files

## Method Notes

1. This is a functional coverage review, not a gcov or llvm-cov measurement.
2. The source-function denominator uses header-level prototypes as the approximation baseline, not every internal static helper in .c files.
3. If the denominator is expanded to include all internal/static implementation helpers, the percentage will be lower.
