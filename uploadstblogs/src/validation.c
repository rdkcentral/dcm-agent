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
 * @file validation.c
 * @brief System validation implementation
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <secure_wrapper.h>
#include "validation.h"
#include "file_operations.h"
#include "event_manager.h"
#include "rdk_debug.h"


bool validate_system(const RuntimeContext* ctx)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Context pointer is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Starting system validation\n", __FUNCTION__, __LINE__);

    // Validate directories
    if (!validate_directories(ctx)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Directory validation failed\n", __FUNCTION__, __LINE__);
        return false;
    }

    // Validate configuration
    if (!validate_configuration()) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Configuration validation failed\n", __FUNCTION__, __LINE__);
        return false;
    }

    // Validate CodeBig access (checkcodebigaccess equivalent)
    if (!validate_codebig_access()) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] CodeBig access validation failed - CodeBig uploads may not work\n", __FUNCTION__, __LINE__);
        // Note: This is a warning, not a failure - Direct uploads can still work
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] System validation successful\n", __FUNCTION__, __LINE__);
    return true;
}

bool validate_directories(const RuntimeContext* ctx)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Context pointer is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    bool all_valid = true;

    // Check LOG_PATH - critical directory
    if (strlen(ctx->log_path) > 0) {
        if (!dir_exists(ctx->log_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] LOG_PATH does not exist: %s (will be created if needed)\n", 
                    __FUNCTION__, __LINE__, ctx->log_path);
        } else {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] LOG_PATH exists: %s\n", 
                    __FUNCTION__, __LINE__, ctx->log_path);
        }
    }

    // Check PREV_LOG_PATH - critical for upload (matches script behavior)
    if (strlen(ctx->prev_log_path) > 0) {
        if (!dir_exists(ctx->prev_log_path)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] The Previous Logs folder is missing: %s\n", 
                    __FUNCTION__, __LINE__, ctx->prev_log_path);
            // Script sends MAINT_LOGUPLOAD_ERROR=5 when PREV_LOG_PATH is missing
            emit_folder_missing_error();
        } else {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] PREV_LOG_PATH exists: %s\n", 
                    __FUNCTION__, __LINE__, ctx->prev_log_path);
        }
    }

    // Check temp directory - critical
    if (strlen(ctx->temp_dir) > 0) {
        if (!dir_exists(ctx->temp_dir)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Temp directory does not exist: %s\n", 
                    __FUNCTION__, __LINE__, ctx->temp_dir);
            all_valid = false;
        } else {
            // Check if writable
            if (access(ctx->temp_dir, W_OK) != 0) {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Temp directory is not writable: %s\n", 
                        __FUNCTION__, __LINE__, ctx->temp_dir);
                all_valid = false;
            } else {
                RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Temp directory is valid: %s\n", 
                        __FUNCTION__, __LINE__, ctx->temp_dir);
            }
        }
    }

    // Check telemetry path - will be created if needed
    if (strlen(ctx->telemetry_path) > 0) {
        if (!dir_exists(ctx->telemetry_path)) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Telemetry path does not exist: %s (will be created)\n", 
                    __FUNCTION__, __LINE__, ctx->telemetry_path);
        }
    }

    // Check DRI log path if DRI logs are included
    if (ctx->include_dri && strlen(ctx->dri_log_path) > 0) {
        if (!dir_exists(ctx->dri_log_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] DRI log path does not exist: %s\n", 
                    __FUNCTION__, __LINE__, ctx->dri_log_path);
        }
    }

    return all_valid;
}

bool validate_configuration(void)
{
    bool all_valid = true;

    // Check for include.properties - critical
    if (!file_exists("/etc/include.properties")) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] /etc/include.properties not found\n", 
                __FUNCTION__, __LINE__);
        all_valid = false;
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] /etc/include.properties exists\n", 
                __FUNCTION__, __LINE__);
    }

    // Check for device.properties - critical
    if (!file_exists("/etc/device.properties")) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] /etc/device.properties not found\n", 
                __FUNCTION__, __LINE__);
        all_valid = false;
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] /etc/device.properties exists\n", 
                __FUNCTION__, __LINE__);
    }

    // Check for debug.ini - for RDK logging
    if (!file_exists("/etc/debug.ini")) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] /etc/debug.ini not found (logging may be affected)\n", 
                __FUNCTION__, __LINE__);
        // Not critical - logging can still work with fallback
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] /etc/debug.ini exists\n", 
                __FUNCTION__, __LINE__);
    }

    return all_valid;
}

bool validate_codebig_access(void)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Starting CodeBig access validation (checkcodebigaccess)\n", __FUNCTION__, __LINE__);

    // Execute GetServiceUrl command to test CodeBig access
    // This is equivalent to the original script's checkCodebigAccess function
    int ret = v_secure_system("GetServiceUrl 2 temp");
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Exit code for codebigcheck: %d\n", __FUNCTION__, __LINE__, ret);
    
    if (ret == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] CodebigAccess Present: %d\n", __FUNCTION__, __LINE__, ret);
        return true;
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] CodebigAccess Not Present: %d\n", __FUNCTION__, __LINE__, ret);
        return false;
    }
}

