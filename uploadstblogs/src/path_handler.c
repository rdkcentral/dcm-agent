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
#ifndef GTEST_ENABLE
#include "uploadUtil.h"
#include "mtls_upload.h"
#include "codebig_upload.h"
#include "upload_status.h"
#endif

/* Forward declarations */
static UploadResult attempt_proxy_fallback(RuntimeContext* ctx, SessionState* session, const char* archive_filepath, const char* md5_ptr);
static UploadResult perform_metadata_post(RuntimeContext* ctx, SessionState* session, const char* endpoint_url, const char* archive_filepath, const char* md5_ptr, MtlsAuth_t* auth);
static UploadResult perform_s3_put_with_fallback(RuntimeContext* ctx, SessionState* session, const char* archive_filepath, const char* md5_ptr, MtlsAuth_t* auth);

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
    
    // Use endpoint_url from TR-181 if available, otherwise fall back to upload_http_link from CLI
    char *endpoint_url = (strlen(ctx->endpoints.endpoint_url) > 0) ? 
                          ctx->endpoints.endpoint_url : 
                          ctx->endpoints.upload_http_link;
    
    // Debug: Log the URL being used
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Using upload URL: %s\n",
            __FUNCTION__, __LINE__, endpoint_url ? endpoint_url : "(NULL)");
    
    if (!endpoint_url || strlen(endpoint_url) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] No valid upload URL configured (endpoint_url and upload_http_link both empty)\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }
    
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
    
    // NOTE: This function is called by retry_upload(), which handles the retry loop
    // Script behavior (line 508-525):
    // - Retry loop calls sendTLSSSRRequest (metadata POST only)
    // - If POST succeeds (HTTP 200), S3 PUT is done ONCE outside retry loop
    // - If S3 PUT fails, proxy fallback is attempted
    
    // Stage 1: Metadata POST (this will be retried by retry_upload)
    UploadResult post_result = perform_metadata_post(ctx, session, endpoint_url, archive_filepath, md5_ptr, NULL);
    
    if (post_result != UPLOADSTB_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Metadata POST failed - HTTP: %d, Curl: %d\n",
                __FUNCTION__, __LINE__, session->http_code, session->curl_code);
        return post_result;  // Return to retry_upload for potential retry
    }
    
    // Stage 2: S3 PUT (done once, with proxy fallback if it fails)
    // Matches script lines 576-650
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Metadata POST succeeded, proceeding with S3 PUT\n", __FUNCTION__, __LINE__);
    
    return perform_s3_put_with_fallback(ctx, session, archive_filepath, md5_ptr, NULL);
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
    
    // Stage 1: Metadata POST
    // performCodeBigMetadataPost signature: (curl, filepath, extra_fields, server_type, http_code_out)
    long http_code = 0;
    int metadata_result = performCodeBigMetadataPost(
        NULL,                             // curl (NULL = library will init/cleanup)
        archive_filepath,                 // filepath
        md5_ptr,                          // extra_fields (MD5 hash, can be NULL)
        HTTP_SSR_CODEBIG,                 // server_type parameter
        &http_code                        // http_code_out
    );

    if (metadata_result != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Metadata POST failed with error code: %d, HTTP: %ld\n",
                __FUNCTION__, __LINE__, metadata_result, http_code);
        session->curl_code = metadata_result;
        session->http_code = (int)http_code;
        return UPLOADSTB_FAILED;
    }

    // Read S3 presigned URL from /tmp/httpresult.txt
    char s3_url[1024] = {0};
    if (extractS3PresignedUrl("/tmp/httpresult.txt", s3_url, sizeof(s3_url)) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to extract S3 URL from httpresult.txt\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] CodeBig metadata POST succeeded. S3 URL: %s\n",
            __FUNCTION__, __LINE__, s3_url);

    // Stage 2: S3 PUT
    // performCodeBigS3Put signature: (s3_url, src_file)
    int s3_result = performCodeBigS3Put(s3_url, archive_filepath);

    // Update session state with result
    session->curl_code = s3_result;
    session->http_code = (s3_result == 0) ? 200 : 0;  // Assume 200 on success

    if (s3_result != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] S3 PUT failed with error code: %d\n",
                __FUNCTION__, __LINE__, s3_result);
        report_curl_error(s3_result);
        return UPLOADSTB_FAILED;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] CodeBig upload completed successfully\n", __FUNCTION__, __LINE__);
    return UPLOADSTB_SUCCESS;
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

/**
 * @brief Perform metadata POST to get S3 presigned URL
 * @param ctx Runtime context
 * @param session Session state
 * @param endpoint_url Upload endpoint URL
 * @param archive_filepath Path to archive file
 * @param md5_ptr MD5 hash (can be NULL)
 * @param auth mTLS auth (can be NULL)
 * @return UploadResult code
 * 
 * Matches script sendTLSSSRRequest (line 344-370): POST filename to get presigned URL
 * Result saved to /tmp/httpresult.txt
 */
static UploadResult perform_metadata_post(RuntimeContext* ctx, SessionState* session, 
                                          const char* endpoint_url, const char* archive_filepath, 
                                          const char* md5_ptr, MtlsAuth_t* auth)
{
    // Use library function performHttpMetadataPostEx which handles all curl operations internally
    UploadStatusDetail upload_status;
    memset(&upload_status, 0, sizeof(UploadStatusDetail));
    
    // Call library function - it handles curl init, OCSP setup, POST, and cleanup
    int result = performHttpMetadataPostEx(
        NULL,                           // curl (NULL = library will init/cleanup)
        endpoint_url,                   // upload URL
        archive_filepath,               // file path
        md5_ptr,                        // extra_fields (MD5 hash, can be NULL)
        auth,                           // mTLS auth (can be NULL)
        ctx->settings.ocsp_enabled,     // OCSP enabled flag
        &upload_status                  // output status
    );
    
    // Update session state with results
    session->curl_code = upload_status.curl_code;
    session->http_code = upload_status.http_code;
    
    // Report curl error if present
    if (upload_status.curl_code != 0) {
        report_curl_error(upload_status.curl_code);
    }
    
    // Report certificate errors
    int curl_ret = upload_status.curl_code;
    if (curl_ret == 35 || curl_ret == 51 || curl_ret == 53 || curl_ret == 54 ||
        curl_ret == 58 || curl_ret == 59 || curl_ret == 60 || curl_ret == 64 ||
        curl_ret == 66 || curl_ret == 77 || curl_ret == 80 || curl_ret == 82 ||
        curl_ret == 83 || curl_ret == 90 || curl_ret == 91) {
        // Extract FQDN from endpoint_url
        char fqdn[256] = {0};
        const char* start = strstr(endpoint_url, "://");
        if (start) {
            start += 3;
            const char* end = strchr(start, '/');
            size_t len = end ? (size_t)(end - start) : strlen(start);
            if (len < sizeof(fqdn)) {
                strncpy(fqdn, start, len);
            }
        }
        report_cert_error(curl_ret, fqdn);
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Metadata POST result - HTTP: %ld, Curl: %d, Result: %d\n",
            __FUNCTION__, __LINE__, upload_status.http_code, upload_status.curl_code, result);
    
    // Verify result
    return verify_upload(session);
}

/**
 * @brief Perform S3 PUT with proxy fallback
 * @param ctx Runtime context
 * @param session Session state
 * @param archive_filepath Path to archive file
 * @param md5_ptr MD5 hash (can be NULL)
 * @param auth mTLS auth (can be NULL)
 * @return UploadResult code
 * 
 * Matches script lines 576-650: Extract S3 URL, do S3 PUT, try proxy on failure
 */
static UploadResult perform_s3_put_with_fallback(RuntimeContext* ctx, SessionState* session,
                                                  const char* archive_filepath, const char* md5_ptr,
                                                  MtlsAuth_t* auth)
{
    // Extract S3 presigned URL from /tmp/httpresult.txt
    char s3_url[1024] = {0};
    if (extractS3PresignedUrl("/tmp/httpresult.txt", s3_url, sizeof(s3_url)) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to extract S3 URL from httpresult.txt\n",
                __FUNCTION__, __LINE__);
        return UPLOADSTB_FAILED;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] S3 upload query success. Got S3 URL: %s\n",
            __FUNCTION__, __LINE__, s3_url);
    
    // Perform S3 PUT upload
    int s3_result = performS3PutUpload(s3_url, archive_filepath, auth);
    
    // Get HTTP code from curl info (script line 608)
    // Note: performS3PutUpload already updates session state via __uploadutil_set_status
    
    // Read curl info file to get codes (matches script pattern)
    FILE* curl_info = fopen("/tmp/logupload_curl_info", "r");
    if (curl_info) {
        long http_code = 0;
        fscanf(curl_info, "%ld", &http_code);
        session->http_code = (int)http_code;
        fclose(curl_info);
    }
    session->curl_code = s3_result;
    
    // Report curl error
    if (s3_result != 0) {
        report_curl_error(s3_result);
        t2_val_notify("LUCurlErr_split", ""); // Script line 614
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] S3 PUT result - HTTP: %d, Curl: %d\n",
            __FUNCTION__, __LINE__, session->http_code, session->curl_code);
    
    // Verify S3 PUT result
    UploadResult s3_verified = verify_upload(session);
    
    if (s3_verified == UPLOADSTB_SUCCESS) {
        t2_count_notify("TEST_lu_success");  // Script line 616
        session->success = true;
        return UPLOADSTB_SUCCESS;
    }
    
    // S3 PUT failed - try proxy fallback (matches script line 625-650)
    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
            "[%s:%d] S3 PUT failed, attempting proxy fallback\n", __FUNCTION__, __LINE__);
    
    UploadResult proxy_result = attempt_proxy_fallback(ctx, session, archive_filepath, md5_ptr);
    if (proxy_result == UPLOADSTB_SUCCESS) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] Proxy fallback succeeded\n", __FUNCTION__, __LINE__);
        session->success = true;
        return UPLOADSTB_SUCCESS;
    }
    
    // Both S3 PUT and proxy failed
    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
            "[%s:%d] Failed uploading logs through HTTP\n", __FUNCTION__, __LINE__);
    t2_count_notify("SYST_ERR_LogUpload_Failed");  // Script line 656
    session->success = false;
    return s3_verified;  // Return original S3 result for retry decision
}


