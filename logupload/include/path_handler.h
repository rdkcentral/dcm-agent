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
 * @file path_handler.h
 * @brief Direct and CodeBig upload path handling
 *
 * This module implements the Direct (mTLS) and CodeBig (OAuth) upload paths
 * including pre-sign requests and S3 uploads.
 */

#ifndef PATH_HANDLER_H
#define PATH_HANDLER_H

#include "uploadstblogs_types.h"

/**
 * @brief Execute Direct path upload (mTLS)
 * @param ctx Runtime context
 * @param session Session state
 * @return UploadResult code
 *
 * Steps:
 * 1. Pre-sign request with mTLS authentication
 * 2. S3 PUT with mTLS
 */
UploadResult execute_direct_path(RuntimeContext* ctx, SessionState* session);

/**
 * @brief Execute CodeBig path upload (OAuth)
 * @param ctx Runtime context
 * @param session Session state
 * @return UploadResult code
 *
 * Steps:
 * 1. Pre-sign request with OAuth header
 * 2. S3 PUT with standard TLS
 */
UploadResult execute_codebig_path(RuntimeContext* ctx, SessionState* session);

/**
 * @brief Perform pre-sign request (Direct path)
 * @param ctx Runtime context
 * @param presign_url_out Buffer to store pre-signed URL
 * @param url_size Size of buffer
 * @return true on success, false on failure
 */
bool presign_request_direct(RuntimeContext* ctx, char* presign_url_out, size_t url_size);

/**
 * @brief Perform pre-sign request (CodeBig path)
 * @param ctx Runtime context
 * @param presign_url_out Buffer to store pre-signed URL
 * @param url_size Size of buffer
 * @return true on success, false on failure
 */
bool presign_request_codebig(RuntimeContext* ctx, char* presign_url_out, size_t url_size);

/**
 * @brief Upload archive to S3 using pre-signed URL
 * @param ctx Runtime context
 * @param session Session state
 * @param presign_url Pre-signed URL
 * @param use_mtls Use mTLS for upload
 * @return true on success, false on failure
 */
bool upload_to_s3(RuntimeContext* ctx, SessionState* session, const char* presign_url, bool use_mtls);

#endif /* PATH_HANDLER_H */
