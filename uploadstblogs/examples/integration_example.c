/**
 * @file integration_example.c
 * @brief Example showing enhanced upload system integration with real status codes
 * 
 * This example demonstrates the complete upload flow with verification:
 * 1. Initialize session and context
 * 2. Execute upload with real HTTP/curl status capture
 * 3. Handle results intelligently with terminal failure detection
 * 4. Provide detailed error reporting
 */

#include "uploadstblogs_types.h"
#include "path_handler.h"
#include "retry_logic.h"
#include "verification.h"
#include "strategy_selector.h"
#include "context_manager.h"
#include "upload_status.h"

/**
 * @brief Complete upload workflow with verification integration
 * 
 * @param archive_filename Path to file to upload
 * @return UploadResult Final upload result
 */
UploadResult upload_with_verification(const char* archive_filename)
{
    // 1. Initialize runtime context
    RuntimeContext ctx = {0};
    SessionState session = {0};
    
    // Load configuration (endpoints, retry limits, etc.)
    if (load_configuration(&ctx) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "Failed to load configuration\n");
        return UPLOAD_FAILED;
    }
    
    // Set up session
    strncpy(session.archive_file, archive_filename, sizeof(session.archive_file) - 1);
    
    // 2. Decide upload strategy and paths
    decide_paths(&ctx, &session);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "Upload strategy: Primary=%s, Fallback=%s\n",
            session.primary == PATH_DIRECT ? "Direct" : "CodeBig",
            session.fallback == PATH_DIRECT ? "Direct" : 
            session.fallback == PATH_CODEBIG ? "CodeBig" : "None");
    
    // 3. Try primary path with verification and retry logic
    UploadResult primary_result = UPLOAD_FAILED;
    
    if (session.primary == PATH_DIRECT) {
        primary_result = retry_upload(&ctx, &session, PATH_DIRECT, 
                                     (UploadResult (*)(RuntimeContext*, SessionState*, UploadPath))execute_direct_path);
    } else if (session.primary == PATH_CODEBIG) {
        primary_result = retry_upload(&ctx, &session, PATH_CODEBIG, 
                                     (UploadResult (*)(RuntimeContext*, SessionState*, UploadPath))execute_codebig_path);
    }
    
    // 4. If primary failed, try fallback (if available)
    if (primary_result != UPLOAD_SUCCESS && session.fallback != PATH_NONE) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "Primary path failed, trying fallback path\n");
        
        session.used_fallback = true;
        
        if (session.fallback == PATH_DIRECT) {
            primary_result = retry_upload(&ctx, &session, PATH_DIRECT, 
                                         (UploadResult (*)(RuntimeContext*, SessionState*, UploadPath))execute_direct_path);
        } else if (session.fallback == PATH_CODEBIG) {
            primary_result = retry_upload(&ctx, &session, PATH_CODEBIG, 
                                         (UploadResult (*)(RuntimeContext*, SessionState*, UploadPath))execute_codebig_path);
        }
    }
    
    // 5. Log final results with detailed verification analysis
    if (primary_result == UPLOAD_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "Upload successful - HTTP: %d, Curl: %d, Used fallback: %s\n",
                session.http_code, session.curl_code, 
                session.used_fallback ? "Yes" : "No");
    } else {
        const char* curl_error = get_curl_error_desc(session.curl_code);
        bool is_terminal = is_terminal_failure(session.http_code);
        
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "Upload failed - HTTP: %d, Curl: %d (%s), Terminal failure: %s\n",
                session.http_code, session.curl_code, curl_error,
                is_terminal ? "Yes (won't retry)" : "No (retryable)");
        
        // Additional diagnostic information
        if (session.curl_code != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "Network issue detected: %s\n", curl_error);
        }
        
        if (is_terminal) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "Terminal HTTP failure - check authentication, permissions, or endpoint\n");
        } else {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "Temporary failure - could retry with different configuration\n");
        }
    }
    
    // 6. Cleanup resources
    cleanup_configuration(&ctx);
    
    return primary_result;
}

/**
 * @brief Example usage of the enhanced upload system
 */
int main()
{
    const char* test_file = "/opt/logs/stb_log_archive.tar.gz";
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "Starting upload with verification\n");
    
    UploadResult result = upload_with_verification(test_file);
    
    switch (result) {
        case UPLOAD_SUCCESS:
            printf("Upload completed successfully!\n");
            return 0;
            
        case UPLOAD_FAILED:
            printf("Upload failed after all retry attempts\n");
            return 1;
            
        case UPLOAD_ABORTED:
            printf("Upload was aborted\n");
            return 2;
            
        default:
            printf("Unknown upload result\n");
            return 3;
    }
}