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
 * @file path_handler.c
 * @brief Upload path handling implementation
 */

#include <stdio.h>
#include <string.h>
#include "path_handler.h"
#include "mtls_handler.h"
#include "oauth_handler.h"
#include "verification.h"
#include "rdk_debug.h"

// Include the upload library headers
#include "uploadUtil.h"
#include "mtls_upload.h"
#include "codebig_upload.h"
#include "upload_status.h"

UploadResult execute_direct_path(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Executing Direct (mTLS) upload path for file: %s\n",
            __FUNCTION__, __LINE__, session->archive_file);
    
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid parameters for direct path\n",
                __FUNCTION__, __LINE__);
        return UPLOAD_FAILED;
    }

    // Prepare upload parameters
    char *archive_filepath = session->archive_file;
    char *endpoint_url = ctx->endpoints.endpoint_url;
    
    // Call the enhanced mTLS upload function
    UploadStatusDetail upload_status;
    int upload_result = uploadFileWithTwoStageFlowEx(
        endpoint_url,     // upload_url parameter
        archive_filepath, // src_file parameter
        &upload_status    // detailed status output
    );
    
    // Update session state with real status codes
    session->curl_code = upload_status.curl_code;
    session->http_code = upload_status.http_code;
    
    // Use verification module to determine result
    UploadResult verified_result = verify_upload(session);
    
    if (verified_result == UPLOAD_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] Direct upload verified successful\n",
                __FUNCTION__, __LINE__);
        session->success = true;
        return UPLOAD_SUCCESS;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Direct upload failed with result: %d\n",
                __FUNCTION__, __LINE__, upload_result);
        
        // Try proxy fallback for mediaclient devices (matching script behavior)
        if (strlen(ctx->device.device_type) > 0 && 
            strcmp(ctx->device.device_type, "mediaclient") == 0 &&
            strlen(ctx->endpoints.proxy_bucket) > 0) {
            
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                    "[%s:%d] Trying logupload through Proxy server: %s\n",
                    __FUNCTION__, __LINE__, ctx->endpoints.proxy_bucket);
            
            // Read S3 URL from /tmp/httpresult.txt (saved during presign step)
            char s3_url[1024] = {0};
            char proxy_url[1024] = {0};
            
            FILE* result_file = fopen("/tmp/httpresult.txt", "r");
            if (result_file && fgets(s3_url, sizeof(s3_url), result_file)) {
                fclose(result_file);
                
                // Remove trailing newline
                char* newline = strchr(s3_url, '\n');
                if (newline) *newline = '\0';
                
                // Extract S3 bucket hostname: sed "s|.*https://||g" | cut -d "/" -f1
                char* https_pos = strstr(s3_url, "https://");
                if (https_pos) {
                    char* bucket_start = https_pos + 8; // Skip "https://"
                    char* path_start = strchr(bucket_start, '/');
                    char* query_start = strchr(bucket_start, '?');
                    
                    if (path_start || query_start) {
                        // Build proxy URL: replace bucket with PROXY_BUCKET, keep path, remove query
                        const char* path_part = path_start ? path_start : "";
                        if (query_start && (!path_start || query_start < path_start)) {
                            // Query comes before path, no path part
                            path_part = "";
                        } else if (query_start && path_start && query_start > path_start) {
                            // Remove query parameters from path
                            size_t path_len = query_start - path_start;
                            static char clean_path[512];
                            strncpy(clean_path, path_start, path_len);
                            clean_path[path_len] = '\0';
                            path_part = clean_path;
                        }
                        
                        snprintf(proxy_url, sizeof(proxy_url), "https://%s%s", 
                                ctx->endpoints.proxy_bucket, path_part);
                        
                        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                                "[%s:%d] Original S3 URL: %s\n", __FUNCTION__, __LINE__, s3_url);
                        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                                "[%s:%d] Constructed proxy URL: %s\n", __FUNCTION__, __LINE__, proxy_url);
                        
                        // Upload to proxy using enhanced function
                        UploadStatusDetail proxy_status;
                        int proxy_result = performS3PutUploadEx(proxy_url, archive_filepath, NULL, &proxy_status);
                        
                        // Update session state with real status codes
                        session->curl_code = proxy_status.curl_code;
                        session->http_code = proxy_status.http_code;
                        
                        UploadResult proxy_verified = verify_upload(session);
                        if (proxy_verified == UPLOAD_SUCCESS) {
                            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                                    "[%s:%d] Proxy upload verified successful\n",
                                    __FUNCTION__, __LINE__);
                            session->success = true;
                            return UPLOAD_SUCCESS;
                        } else {
                            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                                    "[%s:%d] Proxy upload failed with result: %d\n",
                                    __FUNCTION__, __LINE__, proxy_result);
                        }
                    }
                }
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                        "[%s:%d] Could not read S3 URL from /tmp/httpresult.txt for proxy fallback\n",
                        __FUNCTION__, __LINE__);
                if (result_file) fclose(result_file);
            }
        }
        
        session->success = false;
        return UPLOAD_FAILED;
    }
}

UploadResult execute_codebig_path(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Executing CodeBig (OAuth) upload path for file: %s\n",
            __FUNCTION__, __LINE__, session->archive_file);
    
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid parameters for CodeBig path\n",
                __FUNCTION__, __LINE__);
        return UPLOAD_FAILED;
    }

    // Prepare upload parameters
    char *archive_filepath = session->archive_file;
    char *endpoint_url = ctx->endpoints.endpoint_url;
    
    // Call the enhanced CodeBig upload function
    UploadStatusDetail upload_status;
    int upload_result = uploadFileWithCodeBigFlowEx(
        archive_filepath,    // src_file parameter
        HTTP_SSR_CODEBIG,   // server_type parameter
        &upload_status      // detailed status output
    );
    
    // Update session state with real status codes
    session->curl_code = upload_status.curl_code;
    session->http_code = upload_status.http_code;
    
    // Use verification module to determine result
    UploadResult verified_result = verify_upload(session);
    
    if (verified_result == UPLOAD_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] CodeBig upload verified successful\n",
                __FUNCTION__, __LINE__);
        session->success = true;
        return UPLOAD_SUCCESS;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] CodeBig upload failed - HTTP: %d, Curl: %d, Message: %s\n",
                __FUNCTION__, __LINE__, session->http_code, session->curl_code, 
                upload_status.error_message);
        session->success = false;
        return verified_result;
    }
}


