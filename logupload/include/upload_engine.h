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
 * @file upload_engine.h
 * @brief Upload execution engine orchestration
 *
 * This module orchestrates the upload execution including path selection,
 * retry logic, fallback handling, and upload verification.
 */

#ifndef UPLOAD_ENGINE_H
#define UPLOAD_ENGINE_H

#include "uploadstblogs_types.h"

/**
 * @brief Execute complete upload cycle with retry and fallback
 * @param ctx Runtime context
 * @param session Session state
 * @return true on successful upload, false on failure
 *
 * Orchestrates:
 * - Path selection (Direct vs CodeBig)
 * - Pre-sign request
 * - Retry logic
 * - Fallback handling
 * - S3 upload
 * - Verification
 */
bool execute_upload_cycle(RuntimeContext* ctx, SessionState* session);

/**
 * @brief Attempt upload on specified path
 * @param ctx Runtime context
 * @param session Session state
 * @param path Upload path to use
 * @return UploadResult code
 */
UploadResult attempt_upload(RuntimeContext* ctx, SessionState* session, UploadPath path);

/**
 * @brief Determine if fallback should be attempted
 * @param ctx Runtime context
 * @param session Session state
 * @param result Last upload result
 * @return true if fallback allowed, false otherwise
 */
bool should_fallback(const RuntimeContext* ctx, const SessionState* session, UploadResult result);

/**
 * @brief Switch to fallback path
 * @param session Session state
 */
void switch_to_fallback(SessionState* session);

/**
 * @brief Upload archive file to server
 * @param ctx Runtime context
 * @param session Session state
 * @param archive_path Path to archive file
 * @return 0 on success, -1 on failure
 *
 * Handles complete upload process including pre-signed URL request,
 * retry logic, and fallback handling
 */
int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path);

#endif /* UPLOAD_ENGINE_H */
