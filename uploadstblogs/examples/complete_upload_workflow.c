/*
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
 * @file complete_upload_workflow.c
 * @brief Complete upload workflow example with cleanup and verification
 * 
 * Demonstrates the full upload system lifecycle:
 * 1. Configuration loading
 * 2. Path strategy decision
 * 3. Upload execution with verification
 * 4. Retry logic with terminal failure detection  
 * 5. Cleanup and finalization
 * 6. Block marker management
 */

#include "uploadstblogs_types.h"
#include "context_manager.h"
#include "strategy_selector.h"
#include "path_handler.h"
#include "retry_logic.h"
#include "verification.h"
#include "cleanup_handler.h"
#include "upload_status.h"

/**
 * @brief Execute complete upload workflow with all components
 * 
 * @param archive_filename Path to archive file to upload
 * @return UploadResult Final upload result
 */
UploadResult execute_complete_upload_workflow(const char* archive_filename)
{
    if (!archive_filename) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "Invalid archive filename\n");
        return UPLOAD_FAILED;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "Starting complete upload workflow for: %s\n", archive_filename);
    
    // ========================================
    // PHASE 1: INITIALIZATION
    // ========================================
    
    RuntimeContext ctx = {0};
    SessionState session = {0};
    UploadResult final_result = UPLOAD_FAILED;
    
    // Load configuration (endpoints, retry limits, device info, etc.)
    if (load_configuration(&ctx) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "Failed to load configuration\n");
        return UPLOAD_FAILED;
    }
    
    // Initialize session state
    strncpy(session.archive_file, archive_filename, sizeof(session.archive_file) - 1);
    session.strategy = STRATEGY_AUTO;
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "Configuration loaded: Direct attempts=%d, CodeBig attempts=%d\n",
            ctx.retry.direct_max_attempts, ctx.retry.codebig_max_attempts);
    
    // ========================================
    // PHASE 2: PATH STRATEGY DECISION  
    // ========================================
    
    // Check if paths are blocked and decide strategy
    decide_paths(&ctx, &session);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "Upload strategy selected: Primary=%s, Fallback=%s\n",
            session.primary == PATH_DIRECT ? "Direct (mTLS)" : 
            session.primary == PATH_CODEBIG ? "CodeBig (OAuth)" : "None",
            session.fallback == PATH_DIRECT ? "Direct" : 
            session.fallback == PATH_CODEBIG ? "CodeBig" : "None");
    
    // ========================================
    // PHASE 3: PRIMARY PATH UPLOAD
    // ========================================
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "Attempting primary upload path\n");
    
    if (session.primary == PATH_DIRECT) {
        final_result = retry_upload(&ctx, &session, PATH_DIRECT, execute_direct_path);
    } else if (session.primary == PATH_CODEBIG) {
        final_result = retry_upload(&ctx, &session, PATH_CODEBIG, execute_codebig_path);
    }
    
    // Log primary path results
    if (final_result == UPLOAD_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "Primary path successful: HTTP %d, Curl %d, Attempts: %d\n",
                session.http_code, session.curl_code, 
                session.primary == PATH_DIRECT ? session.direct_attempts : session.codebig_attempts);
    } else {
        const char* reason = is_terminal_failure(session.http_code) ? "terminal failure" : "retry exhausted";
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "Primary path failed: HTTP %d, Curl %d (%s), Reason: %s\n",
                session.http_code, session.curl_code, 
                get_curl_error_desc(session.curl_code), reason);
    }
    
    // ========================================
    // PHASE 4: FALLBACK PATH (IF NEEDED)
    // ========================================
    
    if (final_result != UPLOAD_SUCCESS && session.fallback != PATH_NONE) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "Primary path failed, attempting fallback path\n");
        
        session.used_fallback = true;
        
        // Reset status codes for fallback attempt
        session.http_code = 0;
        session.curl_code = 0;
        
        if (session.fallback == PATH_DIRECT) {
            final_result = retry_upload(&ctx, &session, PATH_DIRECT, execute_direct_path);
        } else if (session.fallback == PATH_CODEBIG) {
            final_result = retry_upload(&ctx, &session, PATH_CODEBIG, execute_codebig_path);
        }
        
        // Log fallback results
        if (final_result == UPLOAD_SUCCESS) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "Fallback path successful: HTTP %d, Curl %d\n",
                    session.http_code, session.curl_code);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "Fallback path also failed: HTTP %d, Curl %d\n",
                    session.http_code, session.curl_code);
        }
    }
    
    // ========================================
    // PHASE 5: RESULT VERIFICATION & ANALYSIS
    // ========================================
    
    // Final verification of upload result
    UploadResult verified_result = verify_upload(&session);
    session.success = (verified_result == UPLOAD_SUCCESS);
    
    // Detailed result analysis
    if (session.success) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "Upload SUCCESSFUL via %s path (fallback: %s)\n",
                session.used_fallback ? "fallback" : "primary",
                session.used_fallback ? "yes" : "no");
        
        // Log performance metrics
        int total_attempts = session.direct_attempts + session.codebig_attempts;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "Upload metrics: Total attempts=%d, Direct=%d, CodeBig=%d\n",
                total_attempts, session.direct_attempts, session.codebig_attempts);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "Upload FAILED after all attempts\n");
        
        // Provide detailed failure analysis
        if (session.http_code != 0) {
            bool is_terminal = is_terminal_failure(session.http_code);
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "HTTP failure: %d (%s)\n", session.http_code,
                    is_terminal ? "terminal - no retry" : "temporary - was retried");
        }
        
        if (session.curl_code != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "Network failure: %s\n", get_curl_error_desc(session.curl_code));
        }
    }
    
    // ========================================
    // PHASE 6: CLEANUP & FINALIZATION
    // ========================================
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "Starting cleanup and finalization\n");
    
    // Complete cleanup (block markers, archive removal, temp cleanup)
    finalize(&ctx, &session);
    
    // Log final block status for future uploads
    bool direct_blocked = is_direct_blocked(24 * 3600);   // 24 hours
    bool codebig_blocked = is_codebig_blocked(30 * 60);   // 30 minutes
    
    if (direct_blocked || codebig_blocked) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "Path blocking status: Direct=%s, CodeBig=%s\n",
                direct_blocked ? "BLOCKED" : "available",
                codebig_blocked ? "BLOCKED" : "available");
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "No paths are currently blocked\n");
    }
    
    // ========================================
    // PHASE 7: FINAL RESULT REPORTING
    // ========================================
    
    const char* result_description = NULL;
    switch (final_result) {
        case UPLOAD_SUCCESS:
            result_description = "SUCCESS - Upload completed successfully";
            break;
        case UPLOAD_FAILED:
            result_description = "FAILED - Upload failed after all retry attempts";
            break;
        case UPLOAD_ABORTED:
            result_description = "ABORTED - Upload was intentionally aborted";
            break;
        default:
            result_description = "UNKNOWN - Unexpected result code";
            break;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "Upload workflow completed: %s\n", result_description);
    
    // Clean up configuration resources
    cleanup_configuration(&ctx);
    
    return final_result;
}

/**
 * @brief Example usage with different scenarios
 */
int main(int argc, char* argv[])
{
    const char* test_archive = (argc > 1) ? argv[1] : "/opt/logs/example_stb_logs.tar.gz";
    
    printf("RDK Upload System - Complete Workflow Example\n");
    printf("=============================================\n");
    printf("Archive: %s\n\n", test_archive);
    
    // Execute the complete workflow
    UploadResult result = execute_complete_upload_workflow(test_archive);
    
    // Provide user-friendly output
    switch (result) {
        case UPLOAD_SUCCESS:
            printf("\n✅ Upload completed successfully!\n");
            printf("Your logs have been uploaded and are available for analysis.\n");
            return 0;
            
        case UPLOAD_FAILED:
            printf("\n❌ Upload failed after all retry attempts.\n");
            printf("Please check network connectivity and try again later.\n");
            return 1;
            
        case UPLOAD_ABORTED:
            printf("\n⚠️ Upload was aborted.\n");
            printf("The upload process was intentionally stopped.\n");
            return 2;
            
        default:
            printf("\n❓ Unknown upload result.\n");
            printf("An unexpected error occurred during upload.\n");
            return 3;
    }
}