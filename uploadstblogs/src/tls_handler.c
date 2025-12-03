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
 * @file tls_handler.c
 * @brief TLS configuration implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tls_handler.h"
#include "rdk_debug.h"

bool configure_tls(const RuntimeContext* ctx, char* curl_args, size_t args_size)
{
    if (!ctx || !curl_args || args_size == 0) {
        return false;
    }
    
    // Build TLS configuration for curl - force TLS 1.2
    int ret = snprintf(curl_args, args_size, "--tlsv1.2 --ssl-reqd");
    
    if (ret < 0 || ret >= (int)args_size) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] TLS curl arguments too long\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    // Check if OCSP validation is enabled
    if (ctx && ctx->settings.ocsp_enabled) {
        // Add OCSP options if space allows
        size_t current_len = strlen(curl_args);
        const char* ocsp_args = " --cert-status";
        
        if (current_len + strlen(ocsp_args) < args_size - 1) {
            strcat(curl_args, ocsp_args);
            RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
                    "[%s:%d] OCSP validation enabled\n", __FUNCTION__, __LINE__);
        }
    }
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] TLS curl args: %s\n", __FUNCTION__, __LINE__, curl_args);
    
    return true;
}

const char* get_tls_version(void)
{
    /* TODO: Return TLS version string (e.g., "TLSv1.2") */
    return "TLSv1.2";
}

const char* get_cipher_suites(void)
{
    /* TODO: Return recommended cipher suites */
    return NULL;
}
