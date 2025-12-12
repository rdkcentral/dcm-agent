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
 * @file strategy_ondemand.c
 * @brief ONDEMAND strategy handler implementation
 *
 * ONDEMAND Strategy Workflow:
 * - Working Directory: /tmp/log_on_demand
 * - Source: LOG_PATH (current logs)
 * - No timestamp modification
 * - No permanent backup
 * - Original logs preserved
 * - Temp directory deleted after upload
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
#include "rdk_debug.h"
#include "event_manager.h"

#define ONDEMAND_TEMP_DIR "/tmp/log_on_demand"

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
            ctx ? ctx->device.mac_address : "(NULL CTX)",
            (ctx && strlen(ctx->device.device_type) > 0) ? ctx->device.device_type : "(empty/NULL)");

    // Check if LOG_PATH has .txt or .log files
    // Script uploadLogOnDemand lines 741-752:
    // ret=`ls $LOG_PATH/*.txt`
    // if [ ! $ret ]; then ret=`ls $LOG_PATH/*.log`
    if (!dir_exists(ctx->paths.log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] LOG_PATH does not exist: %s\n", __FUNCTION__, __LINE__, ctx->paths.log_path);
        return -1;
    }

    if (!has_log_files(ctx->paths.log_path)) {
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
            __FUNCTION__, __LINE__, ctx->paths.log_path, ONDEMAND_TEMP_DIR);

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
                          ctx->paths.log_path, timestamp);
    
    if (written >= (int)sizeof(perm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Permanent log path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Log to lastlog_path file
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
            ctx && ctx->device.mac_address ? ctx->device.mac_address : "(NULL/INVALID)",
            (ctx && strlen(ctx->device.device_type) > 0) ? ctx->device.device_type : "(empty/NULL)");

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
    if (!ctx->flags.flag) {
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
            __FUNCTION__, __LINE__, ctx->paths.log_path);

    return 0;
}
