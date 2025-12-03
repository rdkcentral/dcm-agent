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
 * @file ocsp_validator.h
 * @brief OCSP validation support
 *
 * This module handles OCSP (Online Certificate Status Protocol) validation
 * and stapling configuration.
 */

#ifndef OCSP_VALIDATOR_H
#define OCSP_VALIDATOR_H

#include "uploadstblogs_types.h"

/**
 * @brief Check if OCSP validation is enabled
 * @param ctx Runtime context
 * @return true if OCSP enabled, false otherwise
 */
bool is_ocsp_validation_enabled(const RuntimeContext* ctx);

/**
 * @brief Configure OCSP stapling for curl
 * @param ctx Runtime context
 * @param curl_args Output buffer for OCSP curl arguments
 * @param args_size Size of buffer
 * @return true on success, false on failure
 */
bool configure_ocsp(const RuntimeContext* ctx, char* curl_args, size_t args_size);

/**
 * @brief Check for OCSP marker files
 * @return true if OCSP markers present, false otherwise
 */
bool check_ocsp_markers(void);

#endif /* OCSP_VALIDATOR_H */
