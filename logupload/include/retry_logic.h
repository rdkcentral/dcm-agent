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
 * @file retry_logic.h
 * @brief Upload retry logic and delay handling
 *
 * This module implements controlled retry loops with appropriate delays
 * for different upload paths.
 */

#ifndef RETRY_LOGIC_H
#define RETRY_LOGIC_H

#include "uploadstblogs_types.h"

/**
 * @brief Execute retry loop for upload path
 * @param ctx Runtime context
 * @param session Session state
 * @param path Upload path to retry
 * @param attempt_func Function pointer to attempt upload
 * @return UploadResult code
 *
 * Implements retry logic with delays:
 * - Direct: up to N attempts with 60s delay
 * - CodeBig: up to M attempts with 10s delay
 */
UploadResult retry_upload(RuntimeContext* ctx, SessionState* session, 
                         UploadPath path,
                         UploadResult (*attempt_func)(RuntimeContext*, SessionState*, UploadPath));

/**
 * @brief Check if retry should continue
 * @param ctx Runtime context
 * @param session Session state
 * @param path Current upload path
 * @param result Last upload result
 * @return true if should retry, false otherwise
 */
bool should_retry(const RuntimeContext* ctx, const SessionState* session, UploadPath path, UploadResult result);

/**
 * @brief Increment attempt counter for path
 * @param session Session state
 * @param path Upload path
 */
void increment_attempts(SessionState* session, UploadPath path);

#endif /* RETRY_LOGIC_H */
