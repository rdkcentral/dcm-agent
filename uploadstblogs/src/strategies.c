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
 * @file strategies.c
 * @brief Upload strategy implementations
 * 
 * Combines strategy_dcm, strategy_ondemand, and strategy_reboot functionality.
 * Each strategy has its own setup, archive, upload, and cleanup phases.
 * 
 * Strategy Summary:
 * - DCM: Batched uploads from DCM_LOG_PATH, entire directory deleted after upload
 * - ONDEMAND: Immediate upload from temp directory, original logs preserved
 * - REBOOT: Previous boot logs with timestamp manipulation and permanent backup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include "strategy_handler.h"
#include "archive_manager.h"
#include "upload_engine.h"
#include "file_operations.h"
#include "common_device_api.h"
#include "system_utils.h"
#include "rbus_interface.h"
#include "rdk_debug.h"
#include "event_manager.h"

#define ONDEMAND_TEMP_DIR "/tmp/log_on_demand"

/* ==========================
   DCM Strategy Implementation
   ========================== */

/* Forward declarations */
static int dcm_setup(RuntimeContext* ctx, SessionState* session);
static int dcm_archive(RuntimeContext* ctx, SessionState* session);
static int dcm_upload(RuntimeContext* ctx, SessionState* session);
static int dcm_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success);

/**
 * @brief Read upload_flag from DCMSettings.conf
 * @return true if upload is enabled, false otherwise
 * 
 * Shell script equivalent:
 * if [ -f "/tmp/DCMSettings.conf" ]; then
 *     upload_flag=`cat /tmp/DCMSettings.conf | grep 'urn:settings:LogUploadSettings:upload' | cut -d '=' -f2 | sed 's/^"//' | sed 's/"$//'`
 * fi
 */
static bool read_dcm_upload_flag(void)
{
    const char* dcm_settings_file = "/tmp/DCMSettings.conf";
    FILE* fp = fopen(dcm_settings_file, "r");
    
    if (!fp) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                "[%s:%d] DCMSettings.conf not found, assuming upload enabled\n",
                __FUNCTION__, __LINE__);
        return true;  // Default to enabled if file doesn't exist
    }
    
    bool upload_enabled = false;
    char line[512];
    
    // Search for "urn:settings:LogUploadSettings:upload" line
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "urn:settings:LogUploadSettings:upload")) {
            // Extract value after '='
            char* equals = strchr(line, '=');
            if (equals) {
                equals++; // Move past '='
                
                // Skip whitespace and quotes
                while (*equals && (isspace(*equals) || *equals == '"')) {
                    equals++;
                }
                
                // Check if value is "true"
                if (strncasecmp(equals, "true", 4) == 0) {
                    upload_enabled = true;
                }
                
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                        "[%s:%d] DCM upload_flag from DCMSettings.conf: %s\n",
                        __FUNCTION__, __LINE__, upload_enabled ? "true" : "false");
            }
            break;
        }
    }
    
    fclose(fp);
    return upload_enabled;
}

/* Handler definition */
const StrategyHandler dcm_strategy_handler = {
    .setup_phase = dcm_setup,
    .archive_phase = dcm_archive,
    .upload_phase = dcm_upload,
    .cleanup_phase = dcm_cleanup
};

/**
 * @brief Setup phase for DCM strategy
 * 
 * Shell script equivalent (uploadDCMLogs lines 698-705):
 * 1. Change to DCM_LOG_PATH (files already there from batching)
 * 2. Check upload_flag
 * 3. Add timestamps to files in DCM_LOG_PATH
 */
static int dcm_setup(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid context parameter\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Starting setup phase\n", __FUNCTION__, __LINE__);

    // Check if DCM_LOG_PATH exists and has files
    if (!dir_exists(ctx->dcm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] DCM_LOG_PATH does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->dcm_log_path);
        return -1;
    }

    // Check upload_flag from DCMSettings.conf (matches script behavior)
    if (!read_dcm_upload_flag()) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] DCM upload_flag is false, skipping DCM upload\n", 
                __FUNCTION__, __LINE__);
        return -1;  // Signal to skip upload
    }

    // Add timestamps to all files in DCM_LOG_PATH
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Adding timestamps to files in DCM_LOG_PATH\n", 
            __FUNCTION__, __LINE__);
    
    int ret = add_timestamp_to_files(ctx->dcm_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to add timestamps to some files\n", 
                __FUNCTION__, __LINE__);
        // Continue anyway, not critical
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Setup phase complete\n", __FUNCTION__, __LINE__);

    return 0;
}

/**
 * @brief Archive phase for DCM strategy
 * 
 * Shell script equivalent (uploadDCMLogs lines 706-717):
 * - Collect PCAP files to DCM_LOG_PATH if mediaclient
 * - Create tar.gz archive from all files in DCM_LOG_PATH
 * - Sleep 60 seconds
 */
static int dcm_archive(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters (ctx=%p, session=%p)\n", 
                __FUNCTION__, __LINE__, (void*)ctx, (void*)session);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Starting archive phase\n", __FUNCTION__, __LINE__);

    // Collect PCAP files directly to DCM_LOG_PATH if mediaclient
    if (ctx->include_pcap) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Collecting PCAP file to DCM_LOG_PATH\n", __FUNCTION__, __LINE__);
        int count = collect_pcap_logs(ctx, ctx->dcm_log_path);
        if (count > 0) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Collected %d PCAP file\n", __FUNCTION__, __LINE__, count);
        }
    }

    // Create archive from DCM_LOG_PATH (files already have timestamps)
    int ret = create_archive(ctx, session, ctx->dcm_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive\n", __FUNCTION__, __LINE__);
        return -1;
    }

#ifndef L2_TEST_ENABLED
    sleep(60);
#endif
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Archive phase complete\n", __FUNCTION__, __LINE__);

    return 0;
}

/**
 * @brief Upload phase for DCM strategy
 * 
 * Shell script equivalent (uploadDCMLogs lines 718-732):
 * - Upload archive via HTTP
 * - Clear old packet captures
 */
static int dcm_upload(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters (ctx=%p, session=%p)\n", 
                __FUNCTION__, __LINE__, (void*)ctx, (void*)session);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Starting upload phase\n", __FUNCTION__, __LINE__);

    // Construct full archive path using session archive filename
    char archive_path[MAX_PATH_LENGTH];
    if (!join_path(archive_path, sizeof(archive_path), 
                   ctx->dcm_log_path, session->archive_file)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading DCM logs: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    // Upload the archive (session->success is set by execute_upload_cycle)
    int ret = upload_archive(ctx, session, archive_path);

    // Clear old packet captures
    if (ctx->include_pcap) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Clearing old packet captures\n", __FUNCTION__, __LINE__);
        clear_old_packet_captures(ctx->log_path);
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Upload phase complete\n", __FUNCTION__, __LINE__);

    return ret;
}

/**
 * @brief Cleanup phase for DCM strategy
 * 
 * Shell script equivalent (uploadDCMLogs lines 735-737):
 * - Delete entire DCM_LOG_PATH directory
 * - No permanent backup created
 * - No timestamp removal (directory deleted anyway)
 */
static int dcm_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid context parameter\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Starting cleanup phase (upload_success=%d)\n", 
            __FUNCTION__, __LINE__, upload_success);

    // Delete entire DCM_LOG_PATH directory
    if (dir_exists(ctx->dcm_log_path)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Removing DCM_LOG_PATH: %s\n", 
                __FUNCTION__, __LINE__, ctx->dcm_log_path);
        
        if (!remove_directory(ctx->dcm_log_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to remove DCM_LOG_PATH\n", 
                    __FUNCTION__, __LINE__);
            return -1;
        }
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] DCM: Cleanup phase complete. DCM_LOG_PATH removed.\n", 
            __FUNCTION__, __LINE__);

    return 0;
}



/* ==========================
   ONDEMAND Strategy Implementation
   ========================== */


/* Forward declarations */
static int ondemand_setup(RuntimeContext* ctx, SessionState* session);
static int ondemand_archive(RuntimeContext* ctx, SessionState* session);
static int ondemand_upload(RuntimeContext* ctx, SessionState* session);
static int ondemand_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success);

/* Handler definition */
const StrategyHandler ondemand_strategy_handler = {
    .setup_phase = ondemand_setup,
    .archive_phase = ondemand_archive,
    .upload_phase = ondemand_upload,
    .cleanup_phase = ondemand_cleanup
};

/**
 * @brief Setup phase for ONDEMAND strategy
 * 
 * Shell script equivalent (uploadLogOnDemand lines 747-763):
 * 1. Check if logs exist in LOG_PATH
 * 2. Create /tmp/log_on_demand
 * 3. Copy *.txt* and *.log* to temp directory
 * 4. Create PERM_LOG_PATH timestamp
 * 5. Log to lastlog_path
 * 6. Delete old tar file if exists
 */
static int ondemand_setup(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Starting setup phase\n", __FUNCTION__, __LINE__);
    
    // Verify context
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Context in setup: ctx=%p, MAC='%s', device_type='%s'\n",
            __FUNCTION__, __LINE__, (void*)ctx,
            ctx ? ctx->mac_address : "(NULL CTX)",
            (ctx && strlen(ctx->device_type) > 0) ? ctx->device_type : "(empty/NULL)");

    // Check if LOG_PATH has .txt or .log files
    // Script uploadLogOnDemand lines 741-752:
    // ret=`ls $LOG_PATH/*.txt`
    // if [ ! $ret ]; then ret=`ls $LOG_PATH/*.log`
    if (!dir_exists(ctx->log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] LOG_PATH does not exist: %s\n", __FUNCTION__, __LINE__, ctx->log_path);
        return -1;
    }

    if (!has_log_files(ctx->log_path)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] No .txt or .log files in LOG_PATH, aborting\n", __FUNCTION__, __LINE__);
        emit_no_logs_ondemand();
        return -1;
    }

    // Create temp directory: /tmp/log_on_demand
    if (dir_exists(ONDEMAND_TEMP_DIR)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Temp directory already exists, cleaning: %s\n", 
                __FUNCTION__, __LINE__, ONDEMAND_TEMP_DIR);
        remove_directory(ONDEMAND_TEMP_DIR);
    }

    if (!create_directory(ONDEMAND_TEMP_DIR)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create temp directory: %s\n", 
                __FUNCTION__, __LINE__, ONDEMAND_TEMP_DIR);
        return -1;
    }

    // Copy log files from LOG_PATH to temp directory
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Copying logs from %s to %s\n", 
            __FUNCTION__, __LINE__, ctx->log_path, ONDEMAND_TEMP_DIR);

    int count = collect_logs(ctx, session, ONDEMAND_TEMP_DIR);
    if (count <= 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] No log files collected\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Collected %d log files\n", __FUNCTION__, __LINE__, count);

    // Create timestamp for permanent log path (for logging purposes only)
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p-logbackup", tm_info);

    char perm_log_path[MAX_PATH_LENGTH];
    int written = snprintf(perm_log_path, sizeof(perm_log_path), "%s/%s", 
                          ctx->log_path, timestamp);
    
    if (written >= (int)sizeof(perm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Permanent log path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Log to lastlog_path file
    char lastlog_path_file[MAX_PATH_LENGTH];
    written = snprintf(lastlog_path_file, sizeof(lastlog_path_file), "%s/lastlog_path", 
                      ctx->telemetry_path);
    
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
    snprintf(old_tar, sizeof(old_tar), "%s/%s", 
             ONDEMAND_TEMP_DIR, session->archive_file);
    
    if (file_exists(old_tar)) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removing old tar file: %s\n", 
                __FUNCTION__, __LINE__, old_tar);
        remove_file(old_tar);
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Setup phase complete\n", __FUNCTION__, __LINE__);

    return 0;
}

/**
 * @brief Archive phase for ONDEMAND strategy
 * 
 * Shell script equivalent (uploadLogOnDemand lines 769-771):
 * - NO timestamp modification (files keep original names)
 * - Create tar.gz from all files in temp directory
 * - Sleep 2 seconds after tar creation
 */
static int ondemand_archive(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Starting archive phase\n", __FUNCTION__, __LINE__);

    // Debug: verify context is valid
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Context before create_archive: ctx=%p, MAC='%s', device_type='%s'\n",
            __FUNCTION__, __LINE__, 
            (void*)ctx,
            ctx && ctx->mac_address ? ctx->mac_address : "(NULL/INVALID)",
            (ctx && strlen(ctx->device_type) > 0) ? ctx->device_type : "(empty/NULL)");

    // Create archive from temp directory (NO timestamp modification)
    int ret = create_archive(ctx, session, ONDEMAND_TEMP_DIR);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] After create_archive: ret=%d, session->archive_file='%s'\n",
            __FUNCTION__, __LINE__, ret, session ? session->archive_file : "(NULL SESSION)");
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive\n", __FUNCTION__, __LINE__);
        return -1;
    }

    sleep(2);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Archive phase complete\n", __FUNCTION__, __LINE__);

    return 0;
}

/**
 * @brief Upload phase for ONDEMAND strategy
 * 
 * Shell script equivalent (uploadLogOnDemand lines 772-784):
 * - Upload via HTTP if uploadLog is true
 * - Handle upload result and set maintenance_error_flag
 */
static int ondemand_upload(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Starting upload phase\n", __FUNCTION__, __LINE__);

    // Check if upload is enabled
    if (!ctx->flag) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Upload flag is false, skipping upload\n", 
                __FUNCTION__, __LINE__);
        return 0;
    }

    // Construct full archive path
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] session->archive_file='%s'\n",
            __FUNCTION__, __LINE__, session->archive_file);
    
    char archive_path[MAX_PATH_LENGTH];
    snprintf(archive_path, sizeof(archive_path), "%s/%s", 
             ONDEMAND_TEMP_DIR, session->archive_file);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading archive: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    // Upload the archive (session->success is set by execute_upload_cycle)
    int ret = upload_archive(ctx, session, archive_path);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Upload phase complete (result=%d)\n", 
            __FUNCTION__, __LINE__, ret);

    return ret;
}

/**
 * @brief Cleanup phase for ONDEMAND strategy
 * 
 * Shell script equivalent (uploadLogOnDemand lines 789-795):
 * - Delete tar file from temp directory
 * - Delete entire temp directory
 * - Original logs in LOG_PATH remain untouched
 */
static int ondemand_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Starting cleanup phase (upload_success=%d)\n", 
            __FUNCTION__, __LINE__, upload_success);

    // Delete tar file
    char tar_path[MAX_PATH_LENGTH];
    snprintf(tar_path, sizeof(tar_path), "%s/%s", 
             ONDEMAND_TEMP_DIR, session->archive_file);

    if (file_exists(tar_path)) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removing tar file: %s\n", 
                __FUNCTION__, __LINE__, tar_path);
        remove_file(tar_path);
    }

    // Delete entire temp directory
    if (dir_exists(ONDEMAND_TEMP_DIR)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Removing temp directory: %s\n", 
                __FUNCTION__, __LINE__, ONDEMAND_TEMP_DIR);
        
        if (!remove_directory(ONDEMAND_TEMP_DIR)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to remove temp directory\n", 
                    __FUNCTION__, __LINE__);
            return -1;
        }
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] ONDEMAND: Cleanup phase complete. Original logs preserved in %s\n", 
            __FUNCTION__, __LINE__, ctx->log_path);

    return 0;
}



/* ==========================
   REBOOT Strategy Implementation
   ========================== */


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

    // Check if PREV_LOG_PATH exists and has .txt or .log files
    // Script uploadLogOnReboot lines 805-816:
    // ret=`ls $PREV_LOG_PATH/*.txt`
    // if [ ! $ret ]; then ret=`ls $PREV_LOG_PATH/*.log`
    if (!dir_exists(ctx->prev_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] PREV_LOG_PATH does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->prev_log_path);
        return -1;
    }

    if (!has_log_files(ctx->prev_log_path)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] No .txt or .log files in PREV_LOG_PATH, aborting\n", __FUNCTION__, __LINE__);
        emit_no_logs_reboot(ctx);
        return -1;
    }

    // Check system uptime and sleep if needed
    // Script lines 818-836: if uptime < 900s, sleep 330s
    double uptime_seconds = 0.0;
    if (get_system_uptime(&uptime_seconds)) {
        if (uptime_seconds < 900.0) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] System uptime %.0f seconds < 900s, sleeping for 330s\n", 
                    __FUNCTION__, __LINE__, uptime_seconds);
            
            // Script checks ENABLE_MAINTENANCE but both paths result in 330s sleep
            // For simplicity, just sleep (background job with wait has same effect)
#ifndef L2_TEST_ENABLED
            sleep(330);
#endif
            
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Done sleeping\n", __FUNCTION__, __LINE__);
        } else {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Device uptime %.0f seconds >= 900s, skipping sleep\n", 
                    __FUNCTION__, __LINE__, uptime_seconds);
        }
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to get system uptime, skipping sleep\n", 
                __FUNCTION__, __LINE__);
    }

    // Delete old backup files (3+ days old)
    // Remove old timestamp directories and logbackup directories
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Cleaning old backups (3+ days)\n", __FUNCTION__, __LINE__);
    
    int removed = remove_old_directories(ctx->log_path, "*-*-*-*-*M-", 3);
    if (removed > 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Removed %d old timestamp directories\n", 
                __FUNCTION__, __LINE__, removed);
    }
    
    removed = remove_old_directories(ctx->log_path, "*-*-*-*-*M-logbackup", 3);
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
                          ctx->log_path, timestamp);
    
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
                      ctx->telemetry_path);
    
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
    written = snprintf(old_tar, sizeof(old_tar), "%s/logs.tar.gz", ctx->prev_log_path);
    
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
    
    int ret = add_timestamp_to_files(ctx->prev_log_path);
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
    if (ctx->include_pcap) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Collecting PCAP file to PREV_LOG_PATH\n", __FUNCTION__, __LINE__);
        int count = collect_pcap_logs(ctx, ctx->prev_log_path);
        if (count > 0) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Collected %d PCAP file\n", __FUNCTION__, __LINE__, count);
        }
    }
    
    // Create archive from PREV_LOG_PATH (files already have timestamps)
    int ret = create_archive(ctx, session, ctx->prev_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive\n", __FUNCTION__, __LINE__);
        return -1;
    }
#ifndef L2_TEST_ENABLED
    sleep(60);
#endif

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

    // Check reboot reason and RFC settings (matches script logic)
    // Script: if [ "$uploadLog" == "true" ] || [ -z "$reboot_reason" -a "$DISABLE_UPLOAD_LOGS_UNSHEDULED_REBOOT" == "false" ]
    // Note: When DCM_FLAG=0 (Non-DCM), script ALWAYS passes "true" regardless of UploadOnReboot value
    //       When DCM_FLAG=1 (DCM mode), upload_on_reboot determines the behavior
    bool should_upload = false;
    const char* reboot_info_path = "/opt/secure/reboot/previousreboot.info";
    
    // Non-DCM mode (DCM_FLAG=0): Always upload (script line 999: uploadLogOnReboot true)
    if (ctx->dcm_flag == 0) {
        should_upload = true;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Non-DCM mode (dcm_flag=0), will always upload logs\n", 
                __FUNCTION__, __LINE__);
    }
    // DCM mode (DCM_FLAG=1): Check upload_on_reboot flag
    else if (ctx->upload_on_reboot) {
        should_upload = true;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] DCM mode: Upload enabled from settings (upload_on_reboot=true)\n", 
                __FUNCTION__, __LINE__);
    } else {
        // Check reboot reason file for scheduled reboot (grep -i "Scheduled Reboot\|MAINTENANCE_REBOOT")
        bool is_scheduled_reboot = false;
        FILE* reboot_file = fopen(reboot_info_path, "r");
        if (reboot_file) {
            char line[512];
            while (fgets(line, sizeof(line), reboot_file)) {
                // Look for "Scheduled Reboot" or "MAINTENANCE_REBOOT" (case insensitive)
                if (strcasestr(line, "Scheduled Reboot") || strcasestr(line, "MAINTENANCE_REBOOT")) {
                    is_scheduled_reboot = true;
                    break;
                }
            }
            fclose(reboot_file);
        }
        
        // Get RFC setting for unscheduled reboot upload via RBUS
        bool disable_unscheduled_upload = false;
        if (!rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UploadLogsOnUnscheduledReboot.Disable",
                                &disable_unscheduled_upload)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to get UploadLogsOnUnscheduledReboot.Disable RFC, assuming false\n", 
                    __FUNCTION__, __LINE__);
            disable_unscheduled_upload = false;
        }
        
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Reboot reason check - Scheduled: %d, Disable unscheduled RFC: %d\n", 
                __FUNCTION__, __LINE__, is_scheduled_reboot, disable_unscheduled_upload);
        
        // Upload if: reboot reason is empty (unscheduled) AND RFC doesn't disable it
        // Script logic: [ -z "$reboot_reason" -a "$DISABLE_UPLOAD_LOGS_UNSHEDULED_REBOOT" == "false" ]
        if (!is_scheduled_reboot && !disable_unscheduled_upload) {
            should_upload = true;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Unscheduled reboot and RFC allows upload\n", __FUNCTION__, __LINE__);
        }
    }
    
    if (!should_upload) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Upload not allowed based on reboot reason and RFC settings\n", 
                __FUNCTION__, __LINE__);
        return 0;
    }

    // Construct full archive path using session archive filename
    char archive_path[MAX_PATH_LENGTH];
    int written = snprintf(archive_path, sizeof(archive_path), "%s/%s", 
                          ctx->prev_log_path, session->archive_file);
    
    if (written >= (int)sizeof(archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading main logs: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    // Upload main logs (session->success is set by execute_upload_cycle)
    int ret = upload_archive(ctx, session, archive_path);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Main log upload complete (result=%d)\n", 
            __FUNCTION__, __LINE__, ret);

    // Upload DRI logs if directory exists (using separate session to avoid state corruption)
    if (ctx->include_dri && dir_exists(ctx->dri_log_path)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] DRI log directory exists, uploading DRI logs\n", 
                __FUNCTION__, __LINE__);

        // Generate DRI archive filename: {MAC}_DRI_Logs_{timestamp}.tgz
        char dri_filename[MAX_FILENAME_LENGTH];
        if (!generate_archive_name(dri_filename, sizeof(dri_filename), 
                                   ctx->mac_address, "DRI_Logs")) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Failed to generate DRI archive filename\n", 
                    __FUNCTION__, __LINE__);
        } else {
            char dri_archive[MAX_PATH_LENGTH];
            int written = snprintf(dri_archive, sizeof(dri_archive), "%s/%s", 
                                  ctx->prev_log_path, dri_filename);
        
        if (written >= (int)sizeof(dri_archive)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] DRI archive path too long\n", __FUNCTION__, __LINE__);
        } else {
            // Create DRI archive
            int dri_ret = create_dri_archive(ctx, dri_archive);
        
            if (dri_ret == 0) {
#ifndef L2_TEST_ENABLED
                sleep(60);
#endif
            
                // Upload DRI logs using separate session state
                SessionState dri_session = *session;  // Copy current session config
                dri_session.direct_attempts = 0;       // Reset attempt counters
                dri_session.codebig_attempts = 0;
                dri_ret = upload_archive(ctx, &dri_session, dri_archive);
            
                // Send telemetry for DRI upload (matches script lines 883, 886)
                // Script sends SYST_INFO_PDRILogUpload for both success and failure
                t2_count_notify("SYST_INFO_PDRILogUpload");
                
                if (dri_ret == 0) {
                    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                            "[%s:%d] DRI log upload succeeded, removing DRI directory\n", 
                            __FUNCTION__, __LINE__);
                    remove_directory(ctx->dri_log_path);
                } else {
                    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                            "[%s:%d] DRI log upload failed\n", __FUNCTION__, __LINE__);
                }
            
                // Clean up DRI archive
                remove_file(dri_archive);
            }
        }
        }
    }

    // Clear old packet captures
    if (ctx->include_pcap) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Clearing old packet captures\n", __FUNCTION__, __LINE__);
        clear_old_packet_captures(ctx->log_path);
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
                          ctx->prev_log_path, session->archive_file);
    
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
    
    int ret = remove_timestamp_from_files(ctx->prev_log_path);
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
    
    ret = move_directory_contents(ctx->prev_log_path, perm_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to move some files to permanent backup\n", 
                __FUNCTION__, __LINE__);
    }

    // Clean PREV_LOG_PATH
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Cleaning PREV_LOG_PATH\n", __FUNCTION__, __LINE__);
    
    clean_directory(ctx->prev_log_path);

    // Recreate PREV_LOG_BACKUP_PATH for next boot cycle
    // Script lines 900-902: rm -rf + mkdir -p PREV_LOG_BACKUP_PATH
    // PREV_LOG_BACKUP_PATH = $LOG_PATH/PreviousLogs_backup/
    char prev_log_backup_path[MAX_PATH_LENGTH];
    written = snprintf(prev_log_backup_path, sizeof(prev_log_backup_path), "%s/PreviousLogs_backup", 
                      ctx->log_path);
    
    if (written >= (int)sizeof(prev_log_backup_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] PREV_LOG_BACKUP_PATH too long\n", __FUNCTION__, __LINE__);
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Recreating PREV_LOG_BACKUP_PATH for next boot: %s\n", 
                __FUNCTION__, __LINE__, prev_log_backup_path);
        
        if (dir_exists(prev_log_backup_path)) {
            remove_directory(prev_log_backup_path);
        }
        
        if (!create_directory(prev_log_backup_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to create PREV_LOG_BACKUP_PATH\n", __FUNCTION__, __LINE__);
        }
    }

    // If DCM mode with upload_on_reboot=false, add permanent path to DCM batch list
    // Script line 1019: echo $PERM_LOG_PATH >> $DCM_UPLOAD_LIST
    if (ctx->dcm_flag == 1 && ctx->upload_on_reboot == 0) {
        char dcm_upload_list[MAX_PATH_LENGTH];
        int written = snprintf(dcm_upload_list, sizeof(dcm_upload_list), "%s/dcm_upload", ctx->log_path);
        
        if (written >= (int)sizeof(dcm_upload_list)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] DCM upload list path too long\n", __FUNCTION__, __LINE__);
        } else {
            FILE* fp = fopen(dcm_upload_list, "a");
            if (fp) {
                fprintf(fp, "%s\n", perm_log_path);
                fclose(fp);
            }
        }
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] REBOOT/NON_DCM: Cleanup phase complete. Logs backed up to: %s\n", 
            __FUNCTION__, __LINE__, perm_log_path);

    return 0;
}

