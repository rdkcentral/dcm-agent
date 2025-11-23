#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "context_manager.h"


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
    printf("Archive Path:     %s\n\n", ctx.paths.archive_path);

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
    printf("CodeBig Blocked:       %s\n\n", ctx.settings.codebig_blocked ? "YES" : "NO");

    printf("=== Upload Endpoints (TR-181) ===\n");
    if (strlen(ctx.endpoints.endpoint_url) > 0) {
        printf("Upload Endpoint URL:   %s\n\n", ctx.endpoints.endpoint_url);
    } else {
        printf("Upload Endpoint URL:   (not configured)\n\n");
    }

    printf("=== Device Information ===\n");
    if (strlen(ctx.device.mac_address) > 0) {
        printf("MAC Address:           %s\n\n", ctx.device.mac_address);
    } else {
        printf("MAC Address:           (not available)\n\n");
    }

    printf("========================================\n");
    printf("Test completed successfully!\n");
    printf("========================================\n");

    // Cleanup resources
    cleanup_context();

    return 0;
}
