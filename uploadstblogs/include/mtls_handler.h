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
 * @file mtls_handler.h
 * @brief mTLS authentication handling for Direct path
 *
 * This module handles mutual TLS authentication including certificate
 * management and TLS configuration for Direct upload path.
 */

#ifndef MTLS_HANDLER_H
#define MTLS_HANDLER_H

#include "uploadstblogs_types.h"

/**
 * @brief Configure curl for mTLS authentication
 * @param ctx Runtime context
 * @param curl_args Output buffer for curl arguments
 * @param args_size Size of buffer
 * @return true on success, false on failure
 *
 * Adds mTLS certificate options to curl command:
 * --cert, --key, --cacert, TLS version, OCSP stapling
 */
bool configure_mtls(const RuntimeContext* ctx, char* curl_args, size_t args_size);

/**
 * @brief Validate mTLS certificates exist and are readable
 * @param ctx Runtime context
 * @return true if certificates valid, false otherwise
 */
bool validate_mtls_certs(const RuntimeContext* ctx);

/**
 * @brief Check if OCSP stapling should be enabled
 * @param ctx Runtime context
 * @return true if OCSP enabled, false otherwise
 */
bool is_ocsp_enabled(const RuntimeContext* ctx);

#endif /* MTLS_HANDLER_H */
