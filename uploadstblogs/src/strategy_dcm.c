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
 * @file strategy_dcm.c
 * @brief DCM strategy handler implementation
 *
 * DCM Strategy Workflow:
 * - Working Directory: DCM_LOG_PATH
 * - Source: DCM_LOG_PATH (batched logs from previous runs + current logs)
 * - Timestamps added before upload
 * - No permanent backup
 * - Entire directory deleted after upload
 * - Includes PCAP, no DRI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "strategy_handler.h"
#include "log_collector.h"
#include "archive_manager.h"
#include "upload_engine.h"
#include "file_operations.h"
#include "rdk_debug.h"

/* Forward declarations */
static int dcm_setup(RuntimeContext* ctx, SessionState* session);
static int dcm_archive(RuntimeContext* ctx, SessionState* session);
static int dcm_upload(RuntimeContext* ctx, SessionState* session);
static int dcm_cleanup(RuntimeContext* ctx, SessionState* session, bool upload_success);

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
    if (!dir_exists(ctx->paths.dcm_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] DCM_LOG_PATH does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.dcm_log_path);
        return -1;
    }

    // Check if upload flag is set
    if (!ctx->flags.flag) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Upload flag is false, skipping DCM upload\n", 
                __FUNCTION__, __LINE__);
        return -1;  // Signal to skip upload
    }

    // Add timestamps to all files in DCM_LOG_PATH
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Adding timestamps to files in DCM_LOG_PATH\n", 
            __FUNCTION__, __LINE__);
    
    int ret = add_timestamp_to_files(ctx->paths.dcm_log_path);
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
    if (ctx->settings.include_pcap) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Collecting PCAP file to DCM_LOG_PATH\n", __FUNCTION__, __LINE__);
        int count = collect_pcap_logs(ctx, ctx->paths.dcm_log_path);
        if (count > 0) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Collected %d PCAP file\n", __FUNCTION__, __LINE__, count);
        }
    }

    // Create archive from DCM_LOG_PATH (files already have timestamps)
    int ret = create_archive(ctx, session, ctx->paths.dcm_log_path);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive\n", __FUNCTION__, __LINE__);
        return -1;
    }

    sleep(60);

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
    int written = snprintf(archive_path, sizeof(archive_path), "%s/%s", 
                          ctx->paths.dcm_log_path, session->archive_file);
    
    if (written >= (int)sizeof(archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading DCM logs: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    // Upload the archive
    int ret = upload_archive(ctx, session, archive_path);
    
    if (ret == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] DCM log upload succeeded\n", __FUNCTION__, __LINE__);
        session->success = true;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] DCM log upload failed\n", __FUNCTION__, __LINE__);
        session->success = false;
    }

    // Clear old packet captures
    if (ctx->settings.include_pcap) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Clearing old packet captures\n", __FUNCTION__, __LINE__);
        clear_old_packet_captures(ctx->paths.log_path);
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
    if (dir_exists(ctx->paths.dcm_log_path)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Removing DCM_LOG_PATH: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.dcm_log_path);
        
        if (!remove_directory(ctx->paths.dcm_log_path)) {
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
