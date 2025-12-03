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
 * @file oauth_handler.h
 * @brief OAuth authentication handling for CodeBig path
 *
 * This module handles OAuth-based authentication including authorization
 * header generation for CodeBig upload path.
 */

#ifndef OAUTH_HANDLER_H
#define OAUTH_HANDLER_H

#include "uploadstblogs_types.h"

/**
 * @brief Get OAuth authorization header
 * @param ctx Runtime context
 * @param auth_header Output buffer for authorization header
 * @param header_size Size of buffer
 * @return true on success, false on failure
 *
 * Generates OAuth header from signed service URL.
 */
bool get_oauth_header(const RuntimeContext* ctx, char* auth_header, size_t header_size);

/**
 * @brief Configure curl for OAuth authentication
 * @param ctx Runtime context
 * @param curl_args Output buffer for curl arguments
 * @param args_size Size of buffer
 * @return true on success, false on failure
 *
 * Adds OAuth header to curl command.
 */
bool configure_oauth(const RuntimeContext* ctx, char* curl_args, size_t args_size);

/**
 * @brief Sign URL for OAuth authentication
 * @param url URL to sign
 * @param signed_url Output buffer for signed URL
 * @param url_size Size of buffer
 * @return true on success, false on failure
 */
bool sign_url(const char* url, char* signed_url, size_t url_size);

#endif /* OAUTH_HANDLER_H */
