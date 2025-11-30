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
#include "verification.h"
#include "telemetry.h"
#include "md5_utils.h"
#include "rdk_debug.h"

// Include the upload library headers
#include "uploadUtil.h"
#include "mtls_upload.h"
#include "codebig_upload.h"
#include "upload_status.h"

/* Forward declarations */
static UploadResult attempt_proxy_fallback(RuntimeContext* ctx, SessionState* session, const char* archive_filepath, const char* md5_ptr);

UploadResult execute_direct_path(RuntimeContext* ctx, SessionState* session)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Executing Direct (mTLS) upload path for file: %s\n",
            __FUNCTION__, __LINE__, session->archive_file);
    
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid parameters for direct path\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }

    // Prepare upload parameters
    char *archive_filepath = session->archive_file;
    char *endpoint_url = ctx->endpoints.endpoint_url;
    
    // Calculate MD5 if encryption enabled (matches script line 440)
    char md5_base64[64] = {0};
    const char *md5_ptr = NULL;
    if (ctx->settings.encryption_enable) {
        if (calculate_file_md5(archive_filepath, md5_base64, sizeof(md5_base64))) {
            md5_ptr = md5_base64;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                    "[%s:%d] RFC_EncryptCloudUpload_Enable: true, MD5: %s\n",
                    __FUNCTION__, __LINE__, md5_base64);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Failed to calculate MD5 for encryption\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                "[%s:%d] RFC_EncryptCloudUpload_Enable: false\n",
                __FUNCTION__, __LINE__);
    }
    
    // Report mTLS usage telemetry (matches script line 355)
    report_mtls_usage();
    
    // Call the enhanced mTLS upload function
    UploadStatusDetail upload_status;
    int upload_result = uploadFileWithTwoStageFlowEx(
        endpoint_url,                     // upload_url parameter
        archive_filepath,                 // src_file parameter
        md5_ptr,                          // MD5 hash (NULL if not enabled)
        ctx->settings.ocsp_enabled,       // OCSP enabled flag
        &upload_status                    // detailed status output
    );
    
    // Update session state with real status codes
    session->curl_code = upload_status.curl_code;
    session->http_code = upload_status.http_code;
    
    // Report curl error if present (matches script lines 338, 614, 645)
    if (upload_status.curl_code != 0) {
        report_curl_error(upload_status.curl_code);
    }
    
    // Report certificate error if present (matches script line 307)
    // Certificate error codes: 35,51,53,54,58,59,60,64,66,77,80,82,83,90,91
    int curl_code = upload_status.curl_code;
    if (curl_code == 35 || curl_code == 51 || curl_code == 53 || curl_code == 54 ||
        curl_code == 58 || curl_code == 59 || curl_code == 60 || curl_code == 64 ||
        curl_code == 66 || curl_code == 77 || curl_code == 80 || curl_code == 82 ||
        curl_code == 83 || curl_code == 90 || curl_code == 91) {
        report_cert_error(curl_code, upload_status.fqdn);
    }
    
    // Use verification module to determine result
    UploadResult verified_result = verify_upload(session);
    
    if (verified_result == UPLOADSTB_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] Direct upload verified successful\n",
                __FUNCTION__, __LINE__);
        session->success = true;
        return UPLOADSTB_SUCCESS;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Direct upload failed with result: %d\n",
                __FUNCTION__, __LINE__, upload_result);
        
        // Try proxy fallback for mediaclient devices (matching script behavior)
        UploadResult fallback_result = attempt_proxy_fallback(ctx, session, archive_filepath, md5_ptr);
        if (fallback_result == UPLOADSTB_SUCCESS) {
            return UPLOADSTB_SUCCESS;
        }
        
        session->success = false;
        return UPLOADSTB_FAILED;
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
        return UPLOADSTB_FAILED;
    }

    // Prepare upload parameters
    char *archive_filepath = session->archive_file;
    
    // Calculate MD5 if encryption enabled (matches script line 440)
    char md5_base64[64] = {0};
    const char *md5_ptr = NULL;
    if (ctx->settings.encryption_enable) {
        if (calculate_file_md5(archive_filepath, md5_base64, sizeof(md5_base64))) {
            md5_ptr = md5_base64;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                    "[%s:%d] RFC_EncryptCloudUpload_Enable: true, MD5: %s\n",
                    __FUNCTION__, __LINE__, md5_base64);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Failed to calculate MD5 for encryption\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
                "[%s:%d] RFC_EncryptCloudUpload_Enable: false\n",
                __FUNCTION__, __LINE__);
    }
    
    // Call the enhanced CodeBig upload function
    UploadStatusDetail upload_status;
    uploadFileWithCodeBigFlowEx(
        archive_filepath,                 // src_file parameter
        HTTP_SSR_CODEBIG,                 // server_type parameter
        md5_ptr,                          // MD5 hash (NULL if not enabled)
        ctx->settings.ocsp_enabled,       // OCSP enabled flag
        &upload_status                    // detailed status output
    );
    
    // Update session state with real status codes
    session->curl_code = upload_status.curl_code;
    session->http_code = upload_status.http_code;
    
    // Report curl error if present (matches script line 338)
    if (upload_status.curl_code != 0) {
        report_curl_error(upload_status.curl_code);
    }
    
    // Use verification module to determine result
    UploadResult verified_result = verify_upload(session);
    
    if (verified_result == UPLOADSTB_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] CodeBig upload verified successful\n",
                __FUNCTION__, __LINE__);
        session->success = true;
        return UPLOADSTB_SUCCESS;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] CodeBig upload failed - HTTP: %d, Curl: %d, Message: %s\n",
                __FUNCTION__, __LINE__, session->http_code, session->curl_code, 
                upload_status.error_message);
        session->success = false;
        return verified_result;
    }
}

/**
 * @brief Attempt proxy fallback upload for mediaclient devices
 * @param ctx Runtime context
 * @param session Session state
 * @param archive_filepath Path to archive file
 * @param md5_ptr MD5 hash pointer (can be NULL)
 * @return UploadResult code
 */
static UploadResult attempt_proxy_fallback(RuntimeContext* ctx, SessionState* session, const char* archive_filepath, const char* md5_ptr)
{
    // Check if proxy fallback is applicable (mediaclient devices only)
    if (strlen(ctx->device.device_type) == 0 || 
        strcmp(ctx->device.device_type, "mediaclient") != 0 ||
        strlen(ctx->endpoints.proxy_bucket) == 0) {
        return UPLOADSTB_FAILED;
    }
    
    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
            "[%s:%d] Trying logupload through Proxy server: %s\n",
            __FUNCTION__, __LINE__, ctx->endpoints.proxy_bucket);
    
    // Read S3 URL from /tmp/httpresult.txt (saved during presign step)
    char s3_url[1024] = {0};
    char proxy_url[1024] = {0};
    
    FILE* result_file = fopen("/tmp/httpresult.txt", "r");
    if (!result_file || !fgets(s3_url, sizeof(s3_url), result_file)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Could not read S3 URL from /tmp/httpresult.txt for proxy fallback\n",
                __FUNCTION__, __LINE__);
        if (result_file) fclose(result_file);
        return UPLOADSTB_FAILED;
    }
    fclose(result_file);
    
    // Remove trailing newline
    char* newline = strchr(s3_url, '\n');
    if (newline) *newline = '\0';
    
    // Extract S3 bucket hostname: sed "s|.*https://||g" | cut -d "/" -f1
    char* https_pos = strstr(s3_url, "https://");
    if (!https_pos) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid S3 URL format in httpresult.txt\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }
    
    char* bucket_start = https_pos + 8; // Skip "https://"
    char* path_start = strchr(bucket_start, '/');
    char* query_start = strchr(bucket_start, '?');
    
    if (!path_start && !query_start) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] No path component found in S3 URL\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }
    
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
    
    // Check if the combined URL will fit in the buffer
    size_t proxy_bucket_len = strlen(ctx->endpoints.proxy_bucket);
    size_t path_part_len = strlen(path_part);
    size_t total_len = 8 + proxy_bucket_len + path_part_len + 1; // "https://" + bucket + path + null
    
    if (total_len >= sizeof(proxy_url)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Proxy URL too long (%zu bytes), skipping proxy fallback\n",
                __FUNCTION__, __LINE__, total_len);
        return UPLOADSTB_FAILED;
    }
    
    // Use safer string construction to avoid truncation warnings
    int ret = snprintf(proxy_url, sizeof(proxy_url), "https://%.*s%.*s", 
                       (int)(sizeof(proxy_url) - 9 - path_part_len - 1), ctx->endpoints.proxy_bucket,
                       (int)(sizeof(proxy_url) - 9 - proxy_bucket_len - 1), path_part);
    
    if (ret < 0 || ret >= sizeof(proxy_url)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to construct proxy URL, truncation occurred\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Original S3 URL: %s\n", __FUNCTION__, __LINE__, s3_url);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Constructed proxy URL: %s\n", __FUNCTION__, __LINE__, proxy_url);
    
    // Upload to proxy using enhanced function
    UploadStatusDetail proxy_status;
    int proxy_result = performS3PutUploadEx(proxy_url, archive_filepath, NULL, 
                                            md5_ptr, ctx->settings.ocsp_enabled, &proxy_status);
    
    // Update session state with real status codes
    session->curl_code = proxy_status.curl_code;
    session->http_code = proxy_status.http_code;
    
    // Report curl error if present
    if (proxy_status.curl_code != 0) {
        report_curl_error(proxy_status.curl_code);
    }
    
    UploadResult proxy_verified = verify_upload(session);
    if (proxy_verified == UPLOADSTB_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] Proxy upload verified successful\n",
                __FUNCTION__, __LINE__);
        session->success = true;
        return UPLOADSTB_SUCCESS;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Proxy upload failed with result: %d\n",
                __FUNCTION__, __LINE__, proxy_result);
        return UPLOADSTB_FAILED;
    }
}


