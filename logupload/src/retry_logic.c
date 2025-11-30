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
 * @file retry_logic.c
 * @brief Retry logic implementation
 */

#include <stdio.h>
#include <unistd.h>
#include "retry_logic.h"
#include "verification.h"
#include "telemetry.h"
#include "rdk_debug.h"

UploadResult retry_upload(RuntimeContext* ctx, SessionState* session, 
                         UploadPath path,
                         UploadResult (*attempt_func)(RuntimeContext*, SessionState*, UploadPath))
{
    if (!ctx || !session || !attempt_func) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid parameters for retry upload\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Starting retry upload for path: %s\n",
            __FUNCTION__, __LINE__, 
            path == PATH_DIRECT ? "Direct" : 
            path == PATH_CODEBIG ? "CodeBig" : "Unknown");

    UploadResult result = UPLOADSTB_FAILED;
    
    do {
        // Increment attempt counter before trying
        increment_attempts(session, path);
        
        // Report upload attempt telemetry (matches script line 511)
        report_upload_attempt();
        
        // Attempt the upload
        result = attempt_func(ctx, session, path);
        
        // If successful, we're done
        if (result == UPLOADSTB_SUCCESS) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                    "[%s:%d] Upload successful after %d attempts\n",
                    __FUNCTION__, __LINE__, 
                    path == PATH_DIRECT ? session->direct_attempts : session->codebig_attempts);
            return result;
        }
        
        // Check if we should continue retrying
        if (should_retry(ctx, session, path, result)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                    "[%s:%d] Upload failed, retrying immediately (attempt %d)\n",
                    __FUNCTION__, __LINE__,
                    path == PATH_DIRECT ? session->direct_attempts : session->codebig_attempts);
            
            // No delay - retry immediately like the original script
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Upload failed, no more retries (total attempts: %d)\n",
                    __FUNCTION__, __LINE__,
                    path == PATH_DIRECT ? session->direct_attempts : session->codebig_attempts);
            break;
        }
        
    } while (should_retry(ctx, session, path, result));
    
    return result;
}

bool should_retry(const RuntimeContext* ctx, const SessionState* session, UploadPath path, UploadResult result)
{
    if (!ctx || !session) {
        return false;
    }

    // Never retry if upload was successful or explicitly aborted
    if (result == UPLOADSTB_SUCCESS || result == UPLOADSTB_ABORTED) {
        return false;
    }
    
    // Handle special case: HTTP 000 indicates network failure
    // Script treats this as fallback trigger, not retry within same path
    if (session->http_code == 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                "[%s:%d] Network failure detected (HTTP 000), no retry - triggers fallback\n",
                __FUNCTION__, __LINE__);
        return false;
    }
    
    // Don't retry terminal failures - in script, only 404 is terminal
    if (is_terminal_failure(session->http_code)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                "[%s:%d] Terminal failure detected (HTTP %d), not retrying\n",
                __FUNCTION__, __LINE__, session->http_code);
        return false;
    }

    // Check attempt limits based on path
    switch (path) {
        case PATH_DIRECT:
            if (session->direct_attempts >= ctx->retry.direct_max_attempts) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                        "[%s:%d] Direct path max attempts reached (%d/%d)\n",
                        __FUNCTION__, __LINE__, 
                        session->direct_attempts, ctx->retry.direct_max_attempts);
                return false;
            }
            break;
            
        case PATH_CODEBIG:
            if (session->codebig_attempts >= ctx->retry.codebig_max_attempts) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                        "[%s:%d] CodeBig path max attempts reached (%d/%d)\n",
                        __FUNCTION__, __LINE__,
                        session->codebig_attempts, ctx->retry.codebig_max_attempts);
                return false;
            }
            break;
            
        case PATH_NONE:
        default:
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Invalid path for retry check: %d\n",
                    __FUNCTION__, __LINE__, path);
            return false;
    }

    // Retry for failed or retry-marked uploads
    return (result == UPLOADSTB_FAILED || result == UPLOADSTB_RETRY);
}

void increment_attempts(SessionState* session, UploadPath path)
{
    if (!session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid session for increment attempts\n",
                __FUNCTION__, __LINE__);
        return;
    }
    
    switch (path) {
        case PATH_DIRECT:
            session->direct_attempts++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                    "[%s:%d] Direct attempts incremented to: %d\n",
                    __FUNCTION__, __LINE__, session->direct_attempts);
            break;
            
        case PATH_CODEBIG:
            session->codebig_attempts++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                    "[%s:%d] CodeBig attempts incremented to: %d\n",
                    __FUNCTION__, __LINE__, session->codebig_attempts);
            break;
            
        case PATH_NONE:
        default:
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Invalid path for increment attempts: %d\n",
                    __FUNCTION__, __LINE__, path);
            break;
    }
}
