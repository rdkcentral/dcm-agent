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
 * @file config_manager.h
 * @brief Configuration file loading and parsing
 *
 * This module handles loading and parsing of configuration files
 * including device.properties, include.properties, and DCM configuration.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "uploadstblogs_types.h"

/**
 * @brief Load configuration from properties files
 * @param ctx Runtime context to populate
 * @return true on success, false on failure
 */
bool load_config(RuntimeContext* ctx);

/**
 * @brief Parse properties file
 * @param filepath Path to properties file
 * @param ctx Runtime context to update
 * @return true on success, false on failure
 */
bool parse_properties_file(const char* filepath, RuntimeContext* ctx);

/**
 * @brief Get property value from file
 * @param filepath Path to properties file
 * @param key Property key
 * @param value Output buffer for value
 * @param value_size Size of output buffer
 * @return true if found, false otherwise
 */
bool get_property(const char* filepath, const char* key, char* value, size_t value_size);

#endif /* CONFIG_MANAGER_H */
