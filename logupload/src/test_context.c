#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "context_manager.h"
#include "validation.h"
#include "log_collector.h"
#include "archive_manager.h"
#include "uploadstblogs_types.h"


int main(void) {
    RuntimeContext ctx;

    printf("========================================\n");
    printf("UploadSTBLogs Context Initialization Test\n");
    printf("========================================\n\n");

    // Test full context initialization (includes RDK logger init)
    printf("Testing init_context()...\n");
    printf("This will initialize:\n");
    printf("  - RDK Logger\n");
    printf("  - Environment properties\n");
    printf("  - TR-181 parameters via RBUS\n");
    printf("  - Device MAC address\n\n");

    if (!init_context(&ctx)) {
        printf("ERROR: Context initialization failed.\n");
        return 1;
    }
    printf("SUCCESS: Context initialized\n\n");

    printf("=== Path Configuration ===\n");
    printf("LOG_PATH:         %s\n", ctx.paths.log_path);
    printf("PREV_LOG_PATH:    %s\n", ctx.paths.prev_log_path);
    printf("DRI_LOG_PATH:     %s\n", ctx.paths.dri_log_path);
    printf("RRD_LOG_FILE:     %s\n", ctx.paths.rrd_file);
    printf("Temp Dir:         %s\n", ctx.paths.temp_dir);
    printf("Archive Path:     %s\n", ctx.paths.archive_path);
    printf("Telemetry Path:   %s\n", ctx.paths.telemetry_path);
    printf("DCM Log File:     %s\n", ctx.paths.dcm_log_file);
    printf("DCM Log Path:     %s\n", ctx.paths.dcm_log_path);
    printf("IARM Binary:      %s\n", ctx.paths.iarm_event_binary);


    printf("=== Retry Configuration ===\n");
    printf("Direct Block Time:     %d seconds (%d hours)\n",
           ctx.retry.direct_retry_delay, ctx.retry.direct_retry_delay / 3600);
    printf("CodeBig Block Time:    %d seconds (%d minutes)\n",
           ctx.retry.codebig_retry_delay, ctx.retry.codebig_retry_delay / 60);
    printf("Direct Max Attempts:   %d\n", ctx.retry.direct_max_attempts);
    printf("CodeBig Max Attempts:  %d\n", ctx.retry.codebig_max_attempts);
    printf("Curl Timeout:          %d seconds\n", ctx.retry.curl_timeout);
    printf("Curl TLS Timeout:      %d seconds\n\n", ctx.retry.curl_tls_timeout);

    printf("=== Upload Settings ===\n");
    printf("OCSP Enabled:          %s\n", ctx.settings.ocsp_enabled ? "YES" : "NO");
    printf("Encryption Enabled:    %s\n", ctx.settings.encryption_enable ? "YES" : "NO");
    printf("Direct Blocked:        %s\n", ctx.settings.direct_blocked ? "YES" : "NO");
    printf("CodeBig Blocked:       %s\n", ctx.settings.codebig_blocked ? "YES" : "NO");
    printf("TLS Enabled:           %s\n", ctx.settings.tls_enabled ? "YES" : "NO");
    printf("Maintenance Enabled:   %s\n", ctx.settings.maintenance_enabled ? "YES" : "NO");

    printf("=== Upload Endpoints (TR-181) ===\n");
    if (strlen(ctx.endpoints.endpoint_url) > 0) {
        printf("Upload Endpoint URL:   %s\n", ctx.endpoints.endpoint_url);
    } else {
        printf("Upload Endpoint URL:   (not configured)\n");
    }

    printf("=== Device Information ===\n");
    if (strlen(ctx.device.mac_address) > 0) {
        printf("MAC Address:           %s\n", ctx.device.mac_address);
    } else {
        printf("MAC Address:           (not available)\n");
    }
    if (strlen(ctx.device.device_type) > 0) {
        printf("Device Type:           %s\n", ctx.device.device_type);
    } else {
        printf("Device Type:           (not configured)\n");
    }
    if (strlen(ctx.device.build_type) > 0) {
        printf("Build Type:            %s\n", ctx.device.build_type);
    } else {
        printf("Build Type:            (not configured)\n");
    }

    printf("========================================\n");
    printf("Testing System Validation\n");
    printf("========================================\n");

    if (!validate_system(&ctx)) {
        printf("WARNING: System validation failed - some components may be missing\n\n");
    } else {
        printf("SUCCESS: System validation passed\n\n");
    }

    printf("========================================\n");
    printf("Testing Log Collection & Archiving\n");
    printf("========================================\n");

    // Initialize session state for archive test
    SessionState session;
    memset(&session, 0, sizeof(SessionState));
    session.strategy = STRAT_DCM;

    printf("\nTest 1: Normal Archive Preparation\n");
    printf("-----------------------------------\n");
    printf("This will:\n");
    printf("  1. Collect logs from LOG_PATH and PREV_LOG_PATH\n");
    printf("  2. Add timestamps to log filenames\n");
    printf("  3. Create tar.gz archive\n");
    printf("  4. Store archive path in session\n\n");

    if (prepare_archive(&ctx, &session)) {
        printf("SUCCESS: Archive created at: %s\n", session.archive_file);

        long archive_size = get_archive_size(session.archive_file);
        if (archive_size > 0) {
            printf("Archive size: %ld bytes (%.2f MB)\n",
                   archive_size, archive_size / (1024.0 * 1024.0));
        }
        printf("\n");
    } else {
        printf("WARNING: Archive preparation failed\n\n");
    }

    // Test RRD archive if RRD file exists
    if (access(ctx.paths.rrd_file, F_OK) == 0) {
        printf("Test 2: RRD Archive Preparation\n");
        printf("--------------------------------\n");
        printf("RRD file found: %s\n", ctx.paths.rrd_file);
        printf("For RRD strategy, file is already in tar.gz format\n");
        printf("No additional archiving needed\n\n");

        SessionState rrd_session;
        memset(&rrd_session, 0, sizeof(SessionState));
        rrd_session.strategy = STRAT_RRD;

        if (prepare_rrd_archive(&ctx, &rrd_session)) {
            printf("SUCCESS: RRD archive ready: %s\n", rrd_session.archive_file);

            long rrd_size = get_archive_size(rrd_session.archive_file);
            if (rrd_size > 0) {
                printf("Archive size: %ld bytes (%.2f MB)\n",
                       rrd_size, rrd_size / (1024.0 * 1024.0));
            }
            printf("\n");
        } else {
            printf("WARNING: RRD archive preparation failed\n\n");
        }
    } else {
        printf("Test 2: RRD Archive Preparation\n");
        printf("--------------------------------\n");
        printf("SKIPPED: RRD file not found at: %s\n\n", ctx.paths.rrd_file);
    }

    printf("========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n");

    // Cleanup resources
    cleanup_context();

    return 0;
}
