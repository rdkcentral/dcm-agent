#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "context_manager.h"
#include "archive_manager.h"
#include "file_operations.h"


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
    printf("Persistent Path:  %s\n\n", ctx.paths.persistent_path);
    

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
    if (strlen(ctx.endpoints.proxy_bucket) > 0) {
        printf("Proxy Bucket:          %s\n", ctx.endpoints.proxy_bucket);
    } else {
        printf("Proxy Bucket:          (not configured)\n");
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
    printf("Test completed successfully!\n");
    printf("========================================\n");

    // Test archive creation functionality
    printf("\n========================================\n");
    printf("Testing Archive Creation Functionality\n");
    printf("========================================\n\n");
    
    // Create a temporary test directory with sample log files
    const char* test_log_dir = "/tmp/test_uploadstblogs_logs";
    printf("Creating test log directory: %s\n", test_log_dir);
    
    if (dir_exists(test_log_dir)) {
        printf("  Cleaning existing test directory...\n");
        remove_directory(test_log_dir);
    }
    
    if (!create_directory(test_log_dir)) {
        printf("ERROR: Failed to create test directory\n");
        cleanup_context();
        return 1;
    }
    printf("  Test directory created successfully\n\n");
    
    // Create sample log files
    printf("Creating sample log files...\n");
    char test_file_path[512];
    
    snprintf(test_file_path, sizeof(test_file_path), "%s/test_receiver.log", test_log_dir);
    FILE* fp = fopen(test_file_path, "w");
    if (fp) {
        fprintf(fp, "Sample receiver log content\n");
        fprintf(fp, "Timestamp: 2025-11-26 10:30:00\n");
        fprintf(fp, "Event: System started\n");
        fclose(fp);
        printf("  Created: %s\n", test_file_path);
    }
    
    snprintf(test_file_path, sizeof(test_file_path), "%s/applications.log", test_log_dir);
    fp = fopen(test_file_path, "w");
    if (fp) {
        fprintf(fp, "Sample applications log content\n");
        fprintf(fp, "Application startup sequence\n");
        fclose(fp);
        printf("  Created: %s\n", test_file_path);
    }
    
    snprintf(test_file_path, sizeof(test_file_path), "%s/system.txt", test_log_dir);
    fp = fopen(test_file_path, "w");
    if (fp) {
        fprintf(fp, "System information and diagnostics\n");
        fclose(fp);
        printf("  Created: %s\n\n", test_file_path);
    }
    
    // Test archive creation
    printf("Testing create_archive()...\n");
    SessionState session = {0};
    session.strategy = STRAT_ONDEMAND;
    
    int ret = create_archive(&ctx, &session, test_log_dir);
    if (ret == 0) {
        printf("SUCCESS: Archive created\n");
        printf("  Archive filename: %s\n", session.archive_file);
        
        // Check archive exists and get size
        char archive_path[MAX_PATH_LENGTH];
        snprintf(archive_path, sizeof(archive_path), "%s/%s", test_log_dir, session.archive_file);
        
        if (file_exists(archive_path)) {
            long archive_size = get_archive_size(archive_path);
            printf("  Archive location: %s\n", archive_path);
            printf("  Archive size: %ld bytes\n", archive_size);
            
            // Verify archive format
            if (strstr(session.archive_file, ctx.device.mac_address) &&
                strstr(session.archive_file, "_Logs_") &&
                strstr(session.archive_file, ".tgz")) {
                printf("  Archive naming format: ✓ VALID (MAC_Logs_timestamp.tgz)\n");
            } else {
                printf("  Archive naming format: ✗ UNEXPECTED\n");
            }
        } else {
            printf("WARNING: Archive file not found at expected location\n");
        }
    } else {
        printf("ERROR: Archive creation failed\n");
    }
    
    // Test DRI archive creation
    printf("\nTesting create_dri_archive()...\n");
    const char* test_dri_dir = "/tmp/test_dri_logs";
    
    if (!dir_exists(test_dri_dir)) {
        create_directory(test_dri_dir);
    }
    
    // Create sample DRI file
    snprintf(test_file_path, sizeof(test_file_path), "%s/dri_data.log", test_dri_dir);
    fp = fopen(test_file_path, "w");
    if (fp) {
        fprintf(fp, "Sample DRI log content\n");
        fclose(fp);
        printf("  Created sample DRI file\n");
    }
    
    strncpy(ctx.paths.dri_log_path, test_dri_dir, sizeof(ctx.paths.dri_log_path) - 1);
    
    char dri_archive_path[MAX_PATH_LENGTH];
    snprintf(dri_archive_path, sizeof(dri_archive_path), "%s/dri_test.tgz", test_log_dir);
    
    ret = create_dri_archive(&ctx, dri_archive_path);
    if (ret == 0) {
        printf("SUCCESS: DRI archive created\n");
        
        // Find the created DRI archive
        DIR* dir = opendir(test_log_dir);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, "DRI_Logs") && strstr(entry->d_name, ".tgz")) {
                    char full_dri_path[MAX_PATH_LENGTH];
                    snprintf(full_dri_path, sizeof(full_dri_path), "%s/%s", test_log_dir, entry->d_name);
                    long dri_size = get_archive_size(full_dri_path);
                    printf("  DRI archive: %s\n", entry->d_name);
                    printf("  DRI archive size: %ld bytes\n", dri_size);
                    
                    if (strstr(entry->d_name, ctx.device.mac_address) &&
                        strstr(entry->d_name, "_DRI_Logs_")) {
                        printf("  DRI naming format: ✓ VALID (MAC_DRI_Logs_timestamp.tgz)\n");
                    }
                    break;
                }
            }
            closedir(dir);
        }
    } else {
        printf("ERROR: DRI archive creation failed\n");
    }
    
    // Cleanup test directories
    printf("\nCleaning up test files...\n");
    remove_directory(test_log_dir);
    remove_directory(test_dri_dir);
    printf("  Cleanup complete\n");

    printf("\n========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n");

    // Cleanup resources
    cleanup_context();

    return 0;
}
