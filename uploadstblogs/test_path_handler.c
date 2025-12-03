/*
 * Test program for path_handler.c integration with upload library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/path_handler.h"
#include "../include/uploadstblogs_types.h"

int main(int argc, char* argv[]) {
    printf("Testing path_handler integration with upload library\n");
    
    // Initialize test context
    RuntimeContext ctx;
    SessionState session;
    
    memset(&ctx, 0, sizeof(RuntimeContext));
    memset(&session, 0, sizeof(SessionState));
    
    // Setup test data
    strcpy(ctx.endpoints.endpoint_url, "https://test-endpoint.example.com/upload");
    strcpy(session.archive_file, "/tmp/test_archive.tar.gz");
    
    // Test direct path
    printf("Testing execute_direct_path...\n");
    UploadResult direct_result = execute_direct_path(&ctx, &session);
    printf("Direct path result: %s\n", 
           direct_result == UPLOAD_SUCCESS ? "SUCCESS" : 
           direct_result == UPLOAD_FAILED ? "FAILED" : 
           direct_result == UPLOAD_RETRY ? "RETRY" : "ABORTED");
    
    // Reset session for next test
    session.success = false;
    session.curl_code = 0;
    session.http_code = 0;
    
    // Test CodeBig path
    printf("Testing execute_codebig_path...\n");
    UploadResult codebig_result = execute_codebig_path(&ctx, &session);
    printf("CodeBig path result: %s\n", 
           codebig_result == UPLOAD_SUCCESS ? "SUCCESS" : 
           codebig_result == UPLOAD_FAILED ? "FAILED" : 
           codebig_result == UPLOAD_RETRY ? "RETRY" : "ABORTED");
    
    // Test pre-signing functions
    char presign_url[512];
    
    printf("Testing presign_request_direct...\n");
    bool direct_presign = presign_request_direct(&ctx, presign_url, sizeof(presign_url));
    printf("Direct presign result: %s, URL: %s\n", 
           direct_presign ? "SUCCESS" : "FAILED", presign_url);
    
    printf("Testing presign_request_codebig...\n");
    bool codebig_presign = presign_request_codebig(&ctx, presign_url, sizeof(presign_url));
    printf("CodeBig presign result: %s, URL: %s\n", 
           codebig_presign ? "SUCCESS" : "FAILED", presign_url);
    
    // Test S3 upload
    printf("Testing upload_to_s3...\n");
    bool s3_result = upload_to_s3(&ctx, &session, "https://s3.example.com/presigned-url", true);
    printf("S3 upload result: %s\n", s3_result ? "SUCCESS" : "FAILED");
    
    printf("Path handler integration test completed\n");
    return 0;
}