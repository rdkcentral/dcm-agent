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
 * @file context_manager.h
 * @brief Runtime context initialization and management
 *
 * This module handles initialization of the runtime context including
 * loading environment variables, TR-181 parameters, and RFC values.
 */

#ifndef CONTEXT_MANAGER_H
#define CONTEXT_MANAGER_H

#include "uploadstblogs_types.h"

/**
 * @brief Initialize runtime context
 * @param ctx Runtime context to initialize
 * @return true on success, false on failure
 *
 * Loads environment variables, device properties, TR-181 values,
 * and RFC settings into the runtime context.
 */
bool init_context(RuntimeContext* ctx);

/**
 * @brief Cleanup runtime context resources
 * 
 * Releases any resources held by the context (e.g., RBUS connection).
 * Call this when done using the context.
 */
void cleanup_context(void);

/**
 * @brief Load environment variables
 * @param ctx Runtime context
 * @return true on success, false on failure
 */
bool load_environment(RuntimeContext* ctx);

/**
 * @brief Load TR-181 parameters
 * @param ctx Runtime context
 * @return true on success, false on failure
 */
bool load_tr181_params(RuntimeContext* ctx);

/**
 * @brief Get device MAC address
 * @param mac_buf Buffer to store MAC address
 * @param buf_size Size of buffer
 * @return true on success, false on failure
 */
bool get_mac_address(char* mac_buf, size_t buf_size);

#endif /* CONTEXT_MANAGER_H */
