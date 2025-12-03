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
 * @file tls_handler.h
 * @brief TLS/MTLS configuration and handling
 *
 * This module provides TLS configuration including version enforcement,
 * cipher suite selection, and security options.
 */

#ifndef TLS_HANDLER_H
#define TLS_HANDLER_H

#include "uploadstblogs_types.h"

/**
 * @brief Configure TLS options for curl
 * @param ctx Runtime context
 * @param curl_args Output buffer for TLS curl arguments
 * @param args_size Size of buffer
 * @return true on success, false on failure
 *
 * Configures:
 * - TLS version (enforce TLSv1.2+)
 * - Cipher suites
 * - Certificate verification
 */
bool configure_tls(const RuntimeContext* ctx, char* curl_args, size_t args_size);

/**
 * @brief Get recommended TLS version string
 * @return TLS version string for curl
 */
const char* get_tls_version(void);

/**
 * @brief Get recommended cipher suites
 * @return Cipher suite string for curl
 */
const char* get_cipher_suites(void);

#endif /* TLS_HANDLER_H */
