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
 * @file strategy_selector.c
 * @brief Upload strategy selection implementation
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "strategy_selector.h"
#include "file_operations.h"
#include "rdk_debug.h"

Strategy early_checks(const RuntimeContext* ctx)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid context\n", __FUNCTION__, __LINE__);
        return STRAT_DCM; // Default fallback
    }

    // Decision tree as per HLD:
    
    // 1. RRD_FLAG == 1 → STRAT_RRD
    if (ctx->flags.rrd_flag == 1) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Strategy: RRD (rrd_flag=1)\n", __FUNCTION__, __LINE__);
        return STRAT_RRD;
    }

    // 2. Privacy mode → STRAT_PRIVACY_ABORT
    if (is_privacy_mode(ctx)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Strategy: PRIVACY_ABORT (privacy enabled)\n", __FUNCTION__, __LINE__);
        return STRAT_PRIVACY_ABORT;
    }

    // 3. No previous logs → STRAT_NO_LOGS
    if (has_no_logs(ctx)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Strategy: NO_LOGS (no previous logs found)\n", __FUNCTION__, __LINE__);
        return STRAT_NO_LOGS;
    }

    // 4. TriggerType == 5 → STRAT_ONDEMAND
    if (ctx->flags.trigger_type == TRIGGER_ONDEMAND) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Strategy: ONDEMAND (trigger_type=5)\n", __FUNCTION__, __LINE__);
        return STRAT_ONDEMAND;
    }

    // 5. DCM_FLAG == 0 → STRAT_NON_DCM
    if (ctx->flags.dcm_flag == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Strategy: NON_DCM (dcm_flag=0)\n", __FUNCTION__, __LINE__);
        return STRAT_NON_DCM;
    }

    // 6. UploadOnReboot == 1 && FLAG == 1 → STRAT_REBOOT
    if (ctx->flags.upload_on_reboot == 1 && ctx->flags.flag == 1) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Strategy: REBOOT (upload_on_reboot=1, flag=1)\n", 
                __FUNCTION__, __LINE__);
        return STRAT_REBOOT;
    }

    // 7. Default → STRAT_DCM
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Strategy: DCM (default)\n", __FUNCTION__, __LINE__);
    return STRAT_DCM;
}

bool is_privacy_mode(const RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }

    bool privacy_enabled = ctx->settings.privacy_do_not_share;
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Privacy mode: %s\n", 
            __FUNCTION__, __LINE__, privacy_enabled ? "ENABLED" : "DISABLED");

    return privacy_enabled;
}

bool has_no_logs(const RuntimeContext* ctx)
{
    if (!ctx) {
        return true; // Treat invalid context as no logs
    }

    const char* prev_log_dir = ctx->paths.prev_log_path;
    
    if (strlen(prev_log_dir) == 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Previous log path not configured\n", __FUNCTION__, __LINE__);
        return true;
    }

    if (!dir_exists(prev_log_dir)) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Previous log directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, prev_log_dir);
        return true;
    }

    bool empty = is_directory_empty(prev_log_dir);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Previous logs directory %s: %s\n", 
            __FUNCTION__, __LINE__, prev_log_dir, empty ? "EMPTY" : "HAS FILES");

    return empty;
}

void decide_paths(const RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return;
    }

    // Path selection logic based on block status
    bool direct_blocked = ctx->settings.direct_blocked;
    bool codebig_blocked = ctx->settings.codebig_blocked;

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Path decision - Direct blocked: %s, CodeBig blocked: %s\n", 
            __FUNCTION__, __LINE__, 
            direct_blocked ? "YES" : "NO",
            codebig_blocked ? "YES" : "NO");

    // Default: Direct primary, CodeBig fallback
    if (!direct_blocked && !codebig_blocked) {
        session->primary = PATH_DIRECT;
        session->fallback = PATH_CODEBIG;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Paths: Primary=DIRECT, Fallback=CODEBIG\n", 
                __FUNCTION__, __LINE__);
    }
    // Direct blocked: CodeBig primary, no fallback
    else if (direct_blocked && !codebig_blocked) {
        session->primary = PATH_CODEBIG;
        session->fallback = PATH_NONE;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Paths: Primary=CODEBIG, Fallback=NONE (direct blocked)\n", 
                __FUNCTION__, __LINE__);
    }
    // CodeBig blocked: Direct primary, no fallback
    else if (!direct_blocked && codebig_blocked) {
        session->primary = PATH_DIRECT;
        session->fallback = PATH_NONE;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Paths: Primary=DIRECT, Fallback=NONE (codebig blocked)\n", 
                __FUNCTION__, __LINE__);
    }
    // Both blocked: No upload possible
    else {
        session->primary = PATH_NONE;
        session->fallback = PATH_NONE;
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Paths: Both DIRECT and CODEBIG are blocked - no upload possible\n", 
                __FUNCTION__, __LINE__);
    }
}
