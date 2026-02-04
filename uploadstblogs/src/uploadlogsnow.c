/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file uploadlogsnow.c
 * @brief UploadLogsNow functionality implementation for logupload binary
 * 
 * This module provides the C implementation of the original UploadLogsNow.sh script
 * functionality, integrated as a special mode in the logupload binary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "uploadlogsnow.h"
#include "uploadstblogs_types.h"
#include "strategy_handler.h"
#include "../include/archive_manager.h"
#include "../include/file_operations.h"
#include "../include/strategy_selector.h"
#include "../include/upload_engine.h"
#include "rdk_debug.h"

#define LOG_UPLOADSTB "LOG.RDK.UPLOADSTB"
#define STATUS_FILE "/opt/loguploadstatus.txt"
#define DCM_TEMP_DIR "/tmp/DCM"

/**
 * @brief Write status message to log upload status file
 */
static int write_upload_status(const char* message)
{
    FILE* fp = fopen(STATUS_FILE, "w");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open status file: %s\n", 
                __FUNCTION__, __LINE__, STATUS_FILE);
        return -1;
    }
    
    time_t now = time(NULL);
    fprintf(fp, "%s %s", message, ctime(&now));
    fclose(fp);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Status updated: %s\n", __FUNCTION__, __LINE__, message);
    return 0;
}

/**
 * @brief Check if file should be excluded from copy operation
 */
static int should_exclude_file(const char* filename)
{
    const char* exclude_list[] = {
        "dcm",
        "PreviousLogs_backup", 
        "PreviousLogs"
    };
    
    for (size_t i = 0; i < sizeof(exclude_list)/sizeof(exclude_list[0]); i++) {
        if (strcmp(filename, exclude_list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Copy all files from source to destination, excluding specified items
 */
static int copy_files_to_dcm_path(const char* src_path, const char* dest_path)
{
    DIR* dir = opendir(src_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open source directory: %s\n", 
                __FUNCTION__, __LINE__, src_path);
        return -1;
    }
    
    struct dirent* entry;
    int copied_count = 0;
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Copying files from %s to %s\n", 
            __FUNCTION__, __LINE__, src_path, dest_path);
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Skip excluded files/directories
        if (should_exclude_file(entry->d_name)) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] Excluding file: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }
        
        // Construct full paths
        char src_file[MAX_PATH_LENGTH];
        char dest_file[MAX_PATH_LENGTH];
        
        // Check if paths would fit to prevent truncation
        size_t src_len = strlen(src_path) + 1 + strlen(entry->d_name) + 1;
        size_t dest_len = strlen(dest_path) + 1 + strlen(entry->d_name) + 1;
        
        if (src_len > sizeof(src_file) || dest_len > sizeof(dest_file)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Path too long, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }
        
        int src_ret = snprintf(src_file, sizeof(src_file), "%s/%s", src_path, entry->d_name);
        int dest_ret = snprintf(dest_file, sizeof(dest_file), "%s/%s", dest_path, entry->d_name);
        
        // Additional safety check for snprintf truncation
        if (src_ret < 0 || src_ret >= (int)sizeof(src_file) || 
            dest_ret < 0 || dest_ret >= (int)sizeof(dest_file)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Path formatting failed, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }
        
        // Use file operations utility for copy
        if (copy_file(src_file, dest_file)) {
            copied_count++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] Copied: %s\n", __FUNCTION__, __LINE__, entry->d_name);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to copy: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
        }
    }
    
    closedir(dir);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Successfully copied %d files/directories\n", 
            __FUNCTION__, __LINE__, copied_count);
    
    return copied_count;
}

/**
 * @brief Execute UploadLogsNow workflow
 */
int execute_uploadlogsnow_workflow(RuntimeContext* ctx)
{
    char dcm_log_path[MAX_PATH_LENGTH] = {0};
    int ret = -1;
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] UploadLogsNow workflow execution started\n", __FUNCTION__, __LINE__);
    
    // Write initial status
    write_upload_status("Triggered");
    
    // Use DCM_LOG_PATH from context or default
    if (strlen(ctx->dcm_log_path) > 0) {
        strncpy(dcm_log_path, ctx->dcm_log_path, sizeof(dcm_log_path) - 1);
    } else {
        strncpy(dcm_log_path, DCM_TEMP_DIR, sizeof(dcm_log_path) - 1);
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Using LOG_PATH=%s, DCM_LOG_PATH=%s\n", 
            __FUNCTION__, __LINE__, ctx->log_path, dcm_log_path);
    
    // Create DCM_LOG_PATH directory
    if (!create_directory(dcm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create DCM_LOG_PATH: %s\n", 
                __FUNCTION__, __LINE__, dcm_log_path);
        write_upload_status("Failed");
        return -1;
    }
    
    // Copy all log files to DCM_LOG_PATH
    int copied_files = copy_files_to_dcm_path(ctx->log_path, dcm_log_path);
    if (copied_files < 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to copy files to DCM path\n", __FUNCTION__, __LINE__);
        write_upload_status("Failed");
        goto cleanup;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading Logs through SNMP/TR69 Upload\n", __FUNCTION__, __LINE__);
    
    // Add timestamps to files (using UploadLogsNow-specific exclusions)
    if (add_timestamp_to_files_uploadlogsnow(dcm_log_path) != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to add timestamps to some files\n", __FUNCTION__, __LINE__);
        // Continue - not critical for upload
    }
    
    // Use existing archive creation function
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Creating archive using archive_manager\n", __FUNCTION__, __LINE__);
    
    write_upload_status("In progress");
    
    // Use the existing ONDEMAND workflow with archive creation
    SessionState session = {0};
    session.strategy = STRAT_ONDEMAND;
    
    // Update the DCM_LOG_PATH in context to point to our prepared directory
    strncpy(ctx->dcm_log_path, dcm_log_path, sizeof(ctx->dcm_log_path) - 1);
    
    // Use existing create_archive function
    if (create_archive(ctx, &session, dcm_log_path) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create log archive\n", __FUNCTION__, __LINE__);
        write_upload_status("Failed");
        goto cleanup;
    }
    
    // Check if archive was created successfully (following RRD pattern)
    char full_archive_path[MAX_PATH_LENGTH];
    int path_ret = snprintf(full_archive_path, sizeof(full_archive_path), "%s/%s", dcm_log_path, session.archive_file);
    
    // Check for snprintf truncation
    if (path_ret < 0 || path_ret >= (int)sizeof(full_archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive path too long: %s/%s\n", 
                __FUNCTION__, __LINE__, dcm_log_path, session.archive_file);
        write_upload_status("Failed");
        goto cleanup;
    }
    
    if (!file_exists(full_archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive file does not exist: %s\n", 
                __FUNCTION__, __LINE__, full_archive_path);
        write_upload_status("Failed");
        goto cleanup;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Archive created successfully: %s\n", 
            __FUNCTION__, __LINE__, full_archive_path);
    
    // Update session to contain full path for upload functions
    strncpy(session.archive_file, full_archive_path, sizeof(session.archive_file) - 1);
    session.archive_file[sizeof(session.archive_file) - 1] = '\0';
    
    // Follow RRD upload pattern: decide_paths() then execute_upload_cycle()
    decide_paths(ctx, &session);
    if (!execute_upload_cycle(ctx, &session)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed Uploading Logs through - SNMP/TR69\n", __FUNCTION__, __LINE__);
        write_upload_status("Failed");
        ret = -1;
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Uploaded Logs through - SNMP/TR69\n", __FUNCTION__, __LINE__);
        write_upload_status("Complete");
        ret = 0;
    }

cleanup:
    // Clean up DCM_LOG_PATH
    if (!remove_directory(dcm_log_path)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to cleanup DCM_LOG_PATH\n", __FUNCTION__, __LINE__);
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Cleaned up DCM_LOG_PATH: %s\n", 
                __FUNCTION__, __LINE__, dcm_log_path);
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] UploadLogsNow workflow completed with result: %d\n", 
            __FUNCTION__, __LINE__, ret);
    
    return ret;
}
