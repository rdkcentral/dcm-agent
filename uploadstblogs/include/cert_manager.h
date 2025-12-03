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
 * @file cert_manager.h
 * @brief Certificate management for TLS/mTLS
 *
 * This module manages certificate paths and validation for secure
 * communications.
 */

#ifndef CERT_MANAGER_H
#define CERT_MANAGER_H

#include "uploadstblogs_types.h"

/**
 * @brief Load certificate paths from configuration
 * @param ctx Runtime context to populate with certificate paths
 * @return true on success, false on failure
 */
bool load_cert_paths(RuntimeContext* ctx);

/**
 * @brief Validate certificate files
 * @param cert_path Path to client certificate
 * @param key_path Path to private key
 * @param ca_path Path to CA certificate
 * @return true if all certificates valid, false otherwise
 */
bool validate_certificates(const char* cert_path, const char* key_path, const char* ca_path);

/**
 * @brief Check certificate expiration
 * @param cert_path Path to certificate
 * @return true if valid/not expired, false if expired or error
 */
bool check_cert_expiration(const char* cert_path);

#endif /* CERT_MANAGER_H */
