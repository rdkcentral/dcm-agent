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
 * @file verification.c
 * @brief Upload verification implementation
 */

#include <stdio.h>
#include <curl/curl.h>
#include "verification.h"
#include "rdkv_cdl_log_wrapper.h"

/**
 * @brief Verify upload result based on HTTP and curl response codes
 * 
 * Aligns with uploadSTBLogs.sh script behavior:
 * - Success: HTTP 200 AND curl success
 * - Failure: Any other HTTP code OR curl failure
 * - Special handling for HTTP 000 (network failure)
 * 
 * @param session Session state containing response codes
 * @return UploadResult indicating success, failure, or retry needed
 */
UploadResult verify_upload(const SessionState* session)
{
    if (!session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "verify_upload: NULL session\n");
        return UPLOAD_FAILED;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "Verifying upload: HTTP=%d, Curl=%d\n", 
            session->http_code, session->curl_code);

    // Check curl-level success first
    if (!is_curl_success(session->curl_code)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "Upload failed at curl level: %s\n", 
                get_curl_error_desc(session->curl_code));
        return UPLOAD_FAILED;
    }

    // Script considers only HTTP 200 as success
    if (session->http_code == 200) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "Upload successful: HTTP %d\n", 
                session->http_code);
        return UPLOAD_SUCCESS;
    }

    // All other HTTP codes are failures
    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "Upload failed: HTTP %d\n", 
            session->http_code);
    return UPLOAD_FAILED;
}

/**
 * @brief Check if HTTP status code indicates success
 * 
 * Based on uploadSTBLogs.sh script: only 200 is considered success
 * Script checks: if [ "$http_code" = "200" ]
 * 
 * @param http_code HTTP response code
 * @return true if success, false otherwise
 */
bool is_http_success(int http_code)
{
    // Script only considers HTTP 200 as success
    return (http_code == 200);
}

/**
 * @brief Check if HTTP status code indicates terminal failure (no retry)
 * 
 * Based on uploadSTBLogs.sh script behavior:
 * - 404: Terminal failure (script breaks immediately, no retries)
 * - 000: Special case (network failure, may trigger fallback but no retry)
 * - All other codes: Retryable failures
 * 
 * @param http_code HTTP response code
 * @return true if terminal failure, false if retryable
 */
bool is_terminal_failure(int http_code)
{
    // Based on script analysis, only 404 is treated as terminal for retry logic
    // Script breaks immediately on 404 with "Retry logic not needed" message
    return (http_code == 404);
}

/**
 * @brief Check if curl code indicates success
 * 
 * @param curl_code Curl response code
 * @return true if success (CURLE_OK), false otherwise
 */
bool is_curl_success(int curl_code)
{
    return (curl_code == CURLE_OK);
}

/**
 * @brief Get human-readable description for curl error code
 * 
 * @param curl_code Curl error code
 * @return String description of the error
 */
const char* get_curl_error_desc(int curl_code)
{
    // Use libcurl's built-in error string function
    return curl_easy_strerror((CURLcode)curl_code);
}
