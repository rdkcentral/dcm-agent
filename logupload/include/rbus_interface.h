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
 * @file rbus_interface.h
 * @brief RBUS interface for TR-181 parameter access
 */

#ifndef RBUS_INTERFACE_H
#define RBUS_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize RBUS connection
 * @return true on success, false on failure
 */
bool rbus_init(void);

/**
 * @brief Close RBUS connection
 */
void rbus_cleanup(void);

/**
 * @brief Get TR-181 string parameter via RBUS
 * @param param_name TR-181 parameter name
 * @param value_buf Buffer to store the string value
 * @param buf_size Size of the value buffer
 * @return true on success, false on failure
 */
bool rbus_get_string_param(const char* param_name, char* value_buf, size_t buf_size);

/**
 * @brief Get TR-181 boolean parameter via RBUS
 * @param param_name TR-181 parameter name
 * @param value Pointer to store the boolean value
 * @return true on success, false on failure
 */
bool rbus_get_bool_param(const char* param_name, bool* value);

/**
 * @brief Get TR-181 integer parameter via RBUS
 * @param param_name TR-181 parameter name
 * @param value Pointer to store the integer value
 * @return true on success, false on failure
 */
bool rbus_get_int_param(const char* param_name, int* value);

#endif /* RBUS_INTERFACE_H */
