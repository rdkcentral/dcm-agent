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
 * @file verification.h
 * @brief Upload verification and result interpretation
 *
 * This module verifies upload success by interpreting HTTP and curl
 * status codes.
 */

#ifndef VERIFICATION_H
#define VERIFICATION_H

#include "uploadstblogs_types.h"

/**
 * @brief Verify upload result
 * @param session Session state with http_code and curl_code
 * @return UploadResult code
 *
 * Verification logic:
 * - HTTP 200 + curl success → UPLOAD_SUCCESS
 * - HTTP 404 → UPLOAD_FAILED (terminal)
 * - Other → UPLOAD_RETRY or UPLOAD_FAILED
 */
UploadResult verify_upload(const SessionState* session);

/**
 * @brief Check if HTTP code indicates success
 * @param http_code HTTP response code
 * @return true if success, false otherwise
 */
bool is_http_success(int http_code);

/**
 * @brief Check if HTTP code indicates terminal failure
 * @param http_code HTTP response code
 * @return true if terminal (no retry), false otherwise
 */
bool is_terminal_failure(int http_code);

/**
 * @brief Check if curl code indicates success
 * @param curl_code Curl return code
 * @return true if success, false otherwise
 */
bool is_curl_success(int curl_code);

/**
 * @brief Get error description for curl code
 * @param curl_code Curl return code
 * @return Error description string
 */
const char* get_curl_error_desc(int curl_code);

#endif /* VERIFICATION_H */
