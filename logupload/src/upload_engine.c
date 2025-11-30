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
 * @file upload_engine.c
 * @brief Upload execution engine implementation
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "upload_engine.h"
#include "path_handler.h"
#include "retry_logic.h"
#include "event_manager.h"
#include "file_operations.h"
#include "rdk_debug.h"

/* Forward declaration for internal function */
static UploadResult single_attempt_upload(RuntimeContext* ctx, SessionState* session, UploadPath path);

bool execute_upload_cycle(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Starting upload cycle for archive: %s\n", 
            __FUNCTION__, __LINE__, session->archive_file);

    // Try primary path first
    UploadResult primary_result = attempt_upload(ctx, session, session->primary);
    
    if (primary_result == UPLOADSTB_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Upload successful on primary path\n", __FUNCTION__, __LINE__);
        session->success = true;
        emit_upload_success(ctx, session);
        return true;
    }

    // Check if we should try fallback
    if (should_fallback(ctx, session, primary_result) && session->fallback != PATH_NONE) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Primary path failed, attempting fallback\n", __FUNCTION__, __LINE__);
        
        switch_to_fallback(session);
        UploadResult fallback_result = attempt_upload(ctx, session, session->fallback);
        
        if (fallback_result == UPLOADSTB_SUCCESS) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Upload successful on fallback path\n", __FUNCTION__, __LINE__);
            session->used_fallback = true;
            session->success = true;
            emit_upload_success(ctx, session);
            return true;
        }
    }

    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
            "[%s:%d] Upload failed on all available paths\n", __FUNCTION__, __LINE__);
    session->success = false;
    emit_upload_failure(ctx, session);
    return false;
}

UploadResult attempt_upload(RuntimeContext* ctx, SessionState* session, UploadPath path)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Attempting upload with retry on path: %s\n", __FUNCTION__, __LINE__, 
            path == PATH_DIRECT ? "Direct" : 
            path == PATH_CODEBIG ? "CodeBig" : "Unknown");

    // Use retry_logic module to handle retries for this path
    return retry_upload(ctx, session, path, single_attempt_upload);
}

/**
 * @brief Single upload attempt function for retry logic
 * @param ctx Runtime context
 * @param session Session state  
 * @param path Upload path
 * @return UploadResult code
 */
static UploadResult single_attempt_upload(RuntimeContext* ctx, SessionState* session, UploadPath path)
{
    if (!ctx || !session) {
        return UPLOADSTB_FAILED;
    }

    // Execute the appropriate upload path without retry logic
    switch (path) {
        case PATH_DIRECT:
            return execute_direct_path(ctx, session);
        
        case PATH_CODEBIG:
            return execute_codebig_path(ctx, session);
        
        case PATH_NONE:
        default:
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Invalid upload path: %d\n", __FUNCTION__, __LINE__, path);
            return UPLOADSTB_FAILED;
    }
}

bool should_fallback(const RuntimeContext* ctx, const SessionState* session, UploadResult result)
{
    if (!ctx || !session) {
        return false;
    }

    // Don't fallback if upload was successful or explicitly aborted
    if (result == UPLOADSTB_SUCCESS || result == UPLOADSTB_ABORTED) {
        return false;
    }

    // Don't fallback if no fallback path is configured
    if (session->fallback == PATH_NONE) {
        return false;
    }

    // Don't fallback if we've already used the fallback
    if (session->used_fallback) {
        return false;
    }

    // Since retry_logic handles all retries, fallback should only occur
    // when a path has been completely exhausted (failed after all retries)
    if (result == UPLOADSTB_FAILED || result == UPLOADSTB_RETRY) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Primary path exhausted after retries, fallback available\n", 
                __FUNCTION__, __LINE__);
        return true;
    }

    return false;
}

void switch_to_fallback(SessionState* session)
{
    if (!session) {
        return;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Switching from primary path %s to fallback path %s\n", 
            __FUNCTION__, __LINE__, 
            session->primary == PATH_DIRECT ? "Direct" : 
            session->primary == PATH_CODEBIG ? "CodeBig" : "Unknown",
            session->fallback == PATH_DIRECT ? "Direct" : 
            session->fallback == PATH_CODEBIG ? "CodeBig" : "Unknown");

    // Swap primary and fallback paths
    UploadPath temp = session->primary;
    session->primary = session->fallback;
    session->fallback = temp;
    
    // Mark that we're using fallback
    session->used_fallback = true;
}

/**
 * @brief Upload archive file to server
 * @param ctx Runtime context
 * @param session Session state
 * @param archive_path Path to archive file
 * @return 0 on success, -1 on failure
 */
int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path)
{
    if (!ctx || !session || !archive_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!file_exists(archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive file does not exist: %s\n", 
                __FUNCTION__, __LINE__, archive_path);
        return -1;
    }

    long file_size = get_file_size(archive_path);
    if (file_size <= 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid archive file size: %ld\n", 
                __FUNCTION__, __LINE__, file_size);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Uploading archive: %s (size: %ld bytes)\n", 
            __FUNCTION__, __LINE__, archive_path, file_size);

    // Set archive path in session for upload functions
    strncpy(session->archive_file, archive_path, sizeof(session->archive_file) - 1);
    
    // Execute the upload cycle with the configured paths
    bool upload_success = execute_upload_cycle(ctx, session);
    
    if (upload_success) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Archive upload completed successfully\n", 
                __FUNCTION__, __LINE__);
        return 0;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Archive upload failed\n", 
                __FUNCTION__, __LINE__);
        return -1;
    }
}
