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
 * @file ocsp_validator.c
 * @brief OCSP validation implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ocsp_validator.h"
#include "rdk_debug.h"

bool is_ocsp_validation_enabled(const RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    
    // Check context setting first
    if (ctx->settings.ocsp_enabled) {
        RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
                "[%s:%d] OCSP enabled via context settings\n", __FUNCTION__, __LINE__);
        return true;
    }
    
    // Check marker files
    if (check_ocsp_markers()) {
        RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
                "[%s:%d] OCSP enabled via marker files\n", __FUNCTION__, __LINE__);
        return true;
    }
    
    return false;
}

bool configure_ocsp(const RuntimeContext* ctx, char* curl_args, size_t args_size)
{
    if (!ctx || !curl_args || args_size == 0) {
        return false;
    }
    
    if (!is_ocsp_validation_enabled(ctx)) {
        // OCSP not enabled, return empty args
        curl_args[0] = '\0';
        return true;
    }
    
    // Add OCSP stapling options to curl
    int ret = snprintf(curl_args, args_size, "--cert-status");
    
    if (ret < 0 || ret >= (int)args_size) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] OCSP curl arguments too long\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] OCSP curl args: %s\n", __FUNCTION__, __LINE__, curl_args);
    
    return true;
}

bool check_ocsp_markers(void)
{
    // Check for /tmp/.EnableOCSPStapling and /tmp/.EnableOCSPCA marker files
    bool stapling_enabled = (access("/tmp/.EnableOCSPStapling", F_OK) == 0);
    bool ca_enabled = (access("/tmp/.EnableOCSPCA", F_OK) == 0);
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] OCSP markers - Stapling: %s, CA: %s\n",
            __FUNCTION__, __LINE__, 
            stapling_enabled ? "enabled" : "disabled",
            ca_enabled ? "enabled" : "disabled");
    
    return (stapling_enabled || ca_enabled);
}
