/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file strategy_reboot.c
 * @brief REBOOT/NON_DCM strategy handler implementation
 *
 * REBOOT/NON_DCM Strategy Workflow:
 * - Working Directory: PREV_LOG_PATH
 * - Source: PREV_LOG_PATH (previous boot logs)
 * - Timestamps added before upload
 * - Timestamps removed after upload
 * - Permanent backup always created
 * - Includes PCAP and DRI logs
 * - Sleep delay if uptime < 15min
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "strategy_handler.h"
#include "log_collector.h"
#include "archive_manager.h"
#include "upload_engine.h"
#include "file_operations.h"
#include "t2MtlsUtils.h"
#include "common_device_api.h"
#include "rdk_debug.h"

/* Forward declarations */
static int reboot_setup(RuntimeContext* ctx, SessionState* session);
static int reboot_archive(RuntimeContext* ctx, SessionState* session);
static int reboot_upload(RuntimeContext* ctx, SessionState* session);
static int reboot_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success);

/* Static storage for permanent log path (used across phases) */
static char perm_log_path_storage[MAX_PATH_LENGTH] = {0};

/* Handler definition */
const StrategyHandler reboot_strategy_handler = {
    .setup_phase = reboot_setup,
    .archive_phase = reboot_archive,
    .upload_phase = reboot_upload,
    .cleanup_phase = reboot_cleanup
};

/**
 * @brief Setup phase for REBOOT/NON_DCM strategy
 * 
 * Shell script equivalent (uploadLogOnReboot lines 820-848):
 * 1. Check system uptime, sleep 330s if < 900s
 * 2. Delete old backups (3+ days old)
 * 3. Create PERM_LOG_PATH timestamp
 * 4. Log to lastlog_path
 * 5. Delete old tar file
 * 6. Add timestamps to all files in PREV_LOG_PATH
 */
static int reboot_setup(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Starting setup phase\n", __FUNCTION__, __LINE__);

    // Check if PREV_LOG_PATH exists and has files
    if (!dir_exists(ctx->paths.prev_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] PREV_LOG_PATH does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.prev_log_path);
        return -1;
    }

    // Check system uptime and sleep if needed
    double uptime_seconds = 0.0;
    if (get_system_uptime(&uptime_seconds) && uptime_seconds < 900.0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] System uptime %.0f seconds < 900s, sleeping for 330s\n", 
                __FUNCTION__, __LINE__, uptime_seconds);
        sleep(330);
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Done sleeping\n", __FUNCTION__, __LINE__);
    } else if (uptime_seconds >= 900.0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Device uptime %.0f seconds >= 900s, skipping sleep\n", 
                __FUNCTION__, __LINE__, uptime_seconds);
    }

    // Delete old backup files (3+ days old)
    // Remove old timestamp directories and logbackup directories
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Cleaning old backups (3+ days)\n", __FUNCTION__, __LINE__);
    
    int removed = remove_old_directories(ctx->paths.log_path, "*-*-*-*-*M-", 3);
    if (removed > 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removed %d old timestamp directories\n", 
                __FUNCTION__, __LINE__, removed);
    }
    
    removed = remove_old_directories(ctx->paths.log_path, "*-*-*-*-*M-logbackup", 3);
    if (removed > 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removed %d old logbackup directories\n", 
                __FUNCTION__, __LINE__, removed);
    }

    // Create timestamp for permanent log path
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p-logbackup", tm_info);

    char perm_log_path[MAX_PATH_LENGTH];
    int written = snprintf(perm_log_path, sizeof(perm_log_path), "%s/%s", 
                          ctx->paths.log_path, timestamp);
    
    if (written >= (int)sizeof(perm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Permanent log path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Store for use in cleanup phase
    strncpy(perm_log_path_storage, perm_log_path, sizeof(perm_log_path_storage) - 1);
    perm_log_path_storage[sizeof(perm_log_path_storage) - 1] = '\0';

    // Log to lastlog_path
    char lastlog_path_file[MAX_PATH_LENGTH];
    written = snprintf(lastlog_path_file, sizeof(lastlog_path_file), "%s/lastlog_path", 
                      ctx->paths.telemetry_path);
    
    if (written >= (int)sizeof(lastlog_path_file)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Lastlog path file too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    FILE* fp = fopen(lastlog_path_file, "a");
    if (fp) {
        fprintf(fp, "%s\n", perm_log_path);
        fclose(fp);
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Logged to lastlog_path: %s\n", 
                __FUNCTION__, __LINE__, perm_log_path);
    }

    // Delete old tar file if exists
    char old_tar[MAX_PATH_LENGTH];
    written = snprintf(old_tar, sizeof(old_tar), "%s/logs.tar.gz", ctx->paths.prev_log_path);
    
    if (written >= (int)sizeof(old_tar)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Old tar path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    if (file_exists(old_tar)) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removing old tar file: %s\n", 
                __FUNCTION__, __LINE__, old_tar);
        remove_file(old_tar);
    }

    // Add timestamps to all files in PREV_LOG_PATH
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Adding timestamps to files in PREV_LOG_PATH\n", 
            __FUNCTION__, __LINE__);
    
    int ret = add_timestamp_to_files(ctx->paths.prev_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to add timestamps to some files\n", 
                __FUNCTION__, __LINE__);
        // Continue anyway, not critical
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Setup phase complete\n", __FUNCTION__, __LINE__);

    return 0;
}

/**
 * @brief Archive phase for REBOOT/NON_DCM strategy
 * 
 * Shell script equivalent (uploadLogOnReboot lines 853-869):
 * - Collect PCAP files to PREV_LOG_PATH if mediaclient
 * - Create tar.gz archive from PREV_LOG_PATH
 * - Sleep 60 seconds
 */
static int reboot_archive(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Starting archive phase\n", __FUNCTION__, __LINE__);

    // Collect PCAP files directly to PREV_LOG_PATH if mediaclient
    if (ctx->settings.include_pcap) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Collecting PCAP file to PREV_LOG_PATH\n", __FUNCTION__, __LINE__);
        int count = collect_pcap_logs(ctx, ctx->paths.prev_log_path);
        if (count > 0) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Collected %d PCAP file\n", __FUNCTION__, __LINE__, count);
        }
    }
    
    // Create archive from PREV_LOG_PATH (files already have timestamps)
    int ret = create_archive(ctx, session, ctx->paths.prev_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive\n", __FUNCTION__, __LINE__);
        return -1;
    }

    sleep(60);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Archive phase complete\n", __FUNCTION__, __LINE__);

    return 0;
}

/**
 * @brief Upload phase for REBOOT/NON_DCM strategy
 * 
 * Shell script equivalent (uploadLogOnReboot lines 853-890):
 * - Check reboot reason and RFC settings
 * - Upload main logs if allowed
 * - Upload DRI logs if directory exists
 * - Clear old packet captures
 */
static int reboot_upload(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Starting upload phase\n", __FUNCTION__, __LINE__);

    // Check if upload is allowed
    if (!ctx->flags.flag) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Upload not allowed, skipping upload\n", 
                __FUNCTION__, __LINE__);
        return 0;
    }

    // Construct full archive path using session archive filename
    char archive_path[MAX_PATH_LENGTH];
    int written = snprintf(archive_path, sizeof(archive_path), "%s/%s", 
                          ctx->paths.prev_log_path, session->archive_file);
    
    if (written >= (int)sizeof(archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading main logs: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    // Upload main logs
    int ret = upload_archive(ctx, session, archive_path);
    
    if (ret == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Main log upload succeeded\n", __FUNCTION__, __LINE__);
        session->success = true;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Main log upload failed\n", __FUNCTION__, __LINE__);
        session->success = false;
    }

    // Upload DRI logs if directory exists
    if (ctx->settings.include_dri && dir_exists(ctx->paths.dri_log_path)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] DRI log directory exists, uploading DRI logs\n", 
                __FUNCTION__, __LINE__);

        char dri_archive[MAX_PATH_LENGTH];
        int written = snprintf(dri_archive, sizeof(dri_archive), "%s/dri_logs.tar.gz", 
                              ctx->paths.prev_log_path);
        
        if (written >= (int)sizeof(dri_archive)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] DRI archive path too long\n", __FUNCTION__, __LINE__);
        } else {
            // Create DRI archive
            int dri_ret = create_dri_archive(ctx, dri_archive);
        
            if (dri_ret == 0) {
                sleep(60);
            
                // Upload DRI logs
                dri_ret = upload_archive(ctx, session, dri_archive);
            
                if (dri_ret == 0) {
                    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                            "[%s:%d] DRI log upload succeeded, removing DRI directory\n", 
                            __FUNCTION__, __LINE__);
                    remove_directory(ctx->paths.dri_log_path);
                } else {
                    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                            "[%s:%d] DRI log upload failed\n", __FUNCTION__, __LINE__);
                }
            
                // Clean up DRI archive
                remove_file(dri_archive);
            }
        }
    }

    // Clear old packet captures
    if (ctx->settings.include_pcap) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Clearing old packet captures\n", __FUNCTION__, __LINE__);
        clear_old_packet_captures(ctx->paths.log_path);
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Upload phase complete\n", __FUNCTION__, __LINE__);

    return ret;
}

/**
 * @brief Cleanup phase for REBOOT/NON_DCM strategy
 * 
 * Shell script equivalent (uploadLogOnReboot lines 893-906):
 * - Always runs (regardless of upload success)
 * - Delete tar file
 * - Remove timestamps from filenames (restore original names)
 * - Create permanent backup directory
 * - Move all files to permanent backup
 * - Clean PREV_LOG_PATH
 */
static int reboot_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Starting cleanup phase (upload_success=%d)\n", 
            __FUNCTION__, __LINE__, upload_success);

    sleep(5);

    // Delete tar file
    char tar_path[MAX_PATH_LENGTH];
    int written = snprintf(tar_path, sizeof(tar_path), "%s/%s", 
                          ctx->paths.prev_log_path, session->archive_file);
    
    if (written >= (int)sizeof(tar_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Tar path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (file_exists(tar_path)) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removing tar file: %s\n", 
                __FUNCTION__, __LINE__, tar_path);
        remove_file(tar_path);
    }

    // Remove timestamps from filenames (restore original names)
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Removing timestamps from filenames\n", __FUNCTION__, __LINE__);
    
    int ret = remove_timestamp_from_files(ctx->paths.prev_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to remove timestamps from some files\n", 
                __FUNCTION__, __LINE__);
        // Continue anyway
    }

    // Get permanent backup path (stored in setup phase)
    const char* perm_log_path = perm_log_path_storage;

    // Create permanent backup directory
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Creating permanent backup directory: %s\n", 
            __FUNCTION__, __LINE__, perm_log_path);
    
    if (!create_directory(perm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create permanent backup directory\n", 
                __FUNCTION__, __LINE__);
        return -1;
    }

    // Move all files from PREV_LOG_PATH to permanent backup
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Moving files to permanent backup\n", __FUNCTION__, __LINE__);
    
    ret = move_directory_contents(ctx->paths.prev_log_path, perm_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to move some files to permanent backup\n", 
                __FUNCTION__, __LINE__);
    }

    // Clean PREV_LOG_PATH
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Cleaning PREV_LOG_PATH\n", __FUNCTION__, __LINE__);
    
    clean_directory(ctx->paths.prev_log_path);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Cleanup phase complete. Logs backed up to: %s\n", 
            __FUNCTION__, __LINE__, perm_log_path);

    return 0;
}
