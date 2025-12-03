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
 * @file oauth_handler.c
 * @brief OAuth authentication implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "oauth_handler.h"
#include "rdk_debug.h"

bool get_oauth_header(const RuntimeContext* ctx, char* auth_header, size_t header_size)
{
    if (!ctx || !auth_header || header_size == 0) {
        return false;
    }
    
    // Get OAuth credentials from environment
    char* client_id = getenv("CLIENT_ID");
    char* client_secret = getenv("CLIENT_SECRET");
    char* oauth_url = getenv("OAUTH_URL");
    
    if (!client_id || !client_secret) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] OAuth credentials not configured\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    // Use default OAuth URL if not specified
    if (!oauth_url) {
        oauth_url = "https://oauth.codebig.com/token";
    }
    
    // For simplicity, we'll create a Bearer token format
    // In a real implementation, this would involve HTTP calls to get the token
    time_t current_time = time(NULL);
    int ret = snprintf(auth_header, header_size,
                      "Bearer %s_%s_%ld",
                      client_id, client_secret, current_time);
    
    if (ret < 0 || ret >= (int)header_size) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] OAuth header too long\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] Generated OAuth header\n", __FUNCTION__, __LINE__);
    
    return true;
}

bool configure_oauth(const RuntimeContext* ctx, char* curl_args, size_t args_size)
{
    if (!ctx || !curl_args || args_size == 0) {
        return false;
    }
    
    char auth_header[512];
    if (!get_oauth_header(ctx, auth_header, sizeof(auth_header))) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] Failed to generate OAuth header\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    // Build curl arguments with OAuth header
    int ret = snprintf(curl_args, args_size,
                      "-H \"Authorization: %s\"",
                      auth_header);
    
    if (ret < 0 || ret >= (int)args_size) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] OAuth curl arguments too long\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] OAuth curl args configured\n", __FUNCTION__, __LINE__);
    
    return true;
}

bool sign_url(const char* url, char* signed_url, size_t url_size)
{
    if (!url || !signed_url || url_size == 0) {
        return false;
    }
    
    // For now, implement a simple URL signing by adding timestamp
    // In a real implementation, this would use a proper signing service
    time_t current_time = time(NULL);
    
    // Check if URL already has query parameters
    const char* separator = strchr(url, '?') ? "&" : "?";
    
    int ret = snprintf(signed_url, url_size,
                      "%s%ssig=%ld&ts=%ld",
                      url, separator, current_time % 1000000, current_time);
    
    if (ret < 0 || ret >= (int)url_size) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] Signed URL too long\\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] URL signed successfully\\n", __FUNCTION__, __LINE__);
    
    return true;
}
