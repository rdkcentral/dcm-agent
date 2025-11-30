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
 * @file cleanup_handler.c
 * @brief Cleanup operations implementation
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "cleanup_handler.h"
#include "context_manager.h"
#include "event_manager.h"
#include "telemetry.h"
#include "file_operations.h"
#include "rdk_debug.h"

void finalize(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Finalizing upload session (success=%s, attempts: direct=%d, codebig=%d)\n", 
            __FUNCTION__, __LINE__, session->success ? "true" : "false",
            session->direct_attempts, session->codebig_attempts);

    // Update block markers based on upload results (script-aligned behavior)
    update_block_markers(ctx, session);

    // Remove archive file if upload was successful
    if (session->success && strlen(session->archive_file) > 0) {
        if (remove_archive(session->archive_file)) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Successfully removed archive: %s\n", 
                    __FUNCTION__, __LINE__, session->archive_file);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to remove archive: %s\n", 
                    __FUNCTION__, __LINE__, session->archive_file);
        }
    }

    // Clean up temporary directories
    if (!cleanup_temp_dirs(ctx)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Failed to clean some temporary directories\n", 
                __FUNCTION__, __LINE__);
    }

    // Send telemetry events based on final result
    const char* result_str = session->success ? "SUCCESS" : "FAILED";
    const char* path_used = session->used_fallback ? "FALLBACK" : "PRIMARY";
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Upload session complete: %s via %s path\n", 
            __FUNCTION__, __LINE__, result_str, path_used);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Upload session finalized\n", __FUNCTION__, __LINE__);
}

void enforce_privacy(const char* log_path)
{
    if (!log_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid log path\n", __FUNCTION__, __LINE__);
        return;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Enforcing privacy mode for: %s\n", __FUNCTION__, __LINE__, log_path);

    // Truncate log file to enforce privacy
    FILE* log_file = fopen(log_path, "w");
    if (log_file) {
        // Write a privacy marker
        fprintf(log_file, "[PRIVACY MODE] Log content cleared at %ld\n", time(NULL));
        fclose(log_file);
        
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Log file truncated for privacy: %s\n", 
                __FUNCTION__, __LINE__, log_path);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to truncate log file: %s (error: %s)\n", 
                __FUNCTION__, __LINE__, log_path, strerror(errno));
    }
}

void update_block_markers(const RuntimeContext* ctx, const SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Updating block markers based on upload results\n", __FUNCTION__, __LINE__);

    // Script behavior for blocking logic:
    // 1. If CodeBig succeeds → block Direct for 24 hours
    // 2. If CodeBig fails → block CodeBig for 30 minutes  
    // 3. If Direct succeeds → no blocking
    // 4. If Direct fails and CodeBig not attempted → no immediate blocking
    
    if (session->success) {
        // Upload succeeded - check which path was used for blocking
        if (session->used_fallback || session->codebig_attempts > 0) {
            // CodeBig was used successfully → block Direct path
            if (create_block_marker(PATH_DIRECT, 24 * 3600)) {  // 24 hours
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                        "[%s:%d] CodeBig success: blocking Direct for 24 hours\n", 
                        __FUNCTION__, __LINE__);
            }
        }
        // If Direct succeeded, no blocking needed (script behavior)
    } else {
        // Upload failed - create appropriate block markers
        
        if (session->codebig_attempts > 0) {
            // CodeBig was attempted but failed → block CodeBig
            if (create_block_marker(PATH_CODEBIG, 30 * 60)) {  // 30 minutes
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                        "[%s:%d] CodeBig failure: blocking CodeBig for 30 minutes\n", 
                        __FUNCTION__, __LINE__);
            }
        }
        
        // Note: Script doesn't block Direct on Direct failure - it may try CodeBig fallback
        // Direct is only blocked when CodeBig succeeds
    }
}

bool remove_archive(const char* archive_path)
{
    if (!archive_path || strlen(archive_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid archive path\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Attempting to remove archive: %s\n", __FUNCTION__, __LINE__, archive_path);

    // Check if file exists first
    if (access(archive_path, F_OK) != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Archive file does not exist: %s\n", __FUNCTION__, __LINE__, archive_path);
        return true;  // Consider non-existent file as "successfully removed"
    }

    // Remove the file
    if (unlink(archive_path) == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Successfully removed archive: %s\n", __FUNCTION__, __LINE__, archive_path);
        return true;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to remove archive %s: %s\n", 
                __FUNCTION__, __LINE__, archive_path, strerror(errno));
        return false;
    }
}

bool cleanup_temp_dirs(const RuntimeContext* ctx)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid context\n", __FUNCTION__, __LINE__);
        return false;
    }

    bool success = true;
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Cleaning up temporary directories\n", __FUNCTION__, __LINE__);

    // Clean up temporary files used during upload
    const char* httpresult_file = "/tmp/httpresult.txt";  // S3 presigned URL storage
    
    if (access(httpresult_file, F_OK) == 0) {
        if (unlink(httpresult_file) == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] Removed temp file: %s\n", __FUNCTION__, __LINE__, httpresult_file);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to remove temp file %s: %s\n", 
                    __FUNCTION__, __LINE__, httpresult_file, strerror(errno));
            success = false;
        }
    }
    
    return success;
}

bool create_block_marker(UploadPath path, int duration_seconds)
{
    const char* block_filename = NULL;
    
    // Determine block filename based on path (matching script behavior)
    switch (path) {
        case PATH_DIRECT:
            block_filename = "/tmp/.lastdirectfail_upl";  // Script: DIRECT_BLOCK_FILENAME
            break;
            
        case PATH_CODEBIG:
            block_filename = "/tmp/.lastcodebigfail_upl";  // Script: CB_BLOCK_FILENAME  
            break;
            
        default:
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Invalid path for block marker creation\n", __FUNCTION__, __LINE__);
            return false;
    }
    
    // Create the block marker file (touch equivalent)
    FILE* block_file = fopen(block_filename, "w");
    if (block_file) {
        // Write a timestamp for reference
        fprintf(block_file, "Block created at %ld for %d seconds\n", time(NULL), duration_seconds);
        fclose(block_file);
        
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Created block marker: %s (duration: %d seconds)\n", 
                __FUNCTION__, __LINE__, block_filename, duration_seconds);
        return true;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create block marker %s: %s\n", 
                __FUNCTION__, __LINE__, block_filename, strerror(errno));
        return false;
    }
}
