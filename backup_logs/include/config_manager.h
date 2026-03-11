/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2024 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load backup configuration from system files
 * 
 * @param config Backup configuration structure to populate
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int config_load(backup_config_t* config);

/**
 * @brief Validate backup configuration
 * 
 * @param config Backup configuration to validate
 * @return int BACKUP_SUCCESS if valid, error code if invalid
 */
int config_validate(const backup_config_t* config);

/**
 * @brief Get log path from configuration
 * 
 * @return const char* Log path string or NULL if not set
 */
const char* config_get_log_path(void);

/**
 * @brief Check if HDD is enabled
 * 
 * @return true if HDD enabled, false otherwise
 */
bool config_is_hdd_enabled(void);

/**
 * @brief Load special files configuration
 * 
 * @param config Special files configuration structure
 * @param config_file Path to configuration file
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int special_files_config_load(special_files_config_t* config, const char* config_file);

/**
 * @brief Validate special files configuration
 * 
 * @param config Special files configuration to validate
 * @return int BACKUP_SUCCESS if valid, error code if invalid
 */
int special_files_config_validate(const special_files_config_t* config);

/**
 * @brief Free special files configuration resources
 * 
 * @param config Special files configuration to free
 */
void special_files_config_free(special_files_config_t* config);

/**
 * @brief Execute special files operations
 * 
 * @param config Special files configuration
 * @param backup_config Main backup configuration for variable substitution
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int special_files_execute_operations(const special_files_config_t* config, 
                                   const backup_config_t* backup_config);

/**
 * @brief Parse environment variables and paths
 * 
 * @param config Backup configuration to update with parsed values
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int config_parse_environment(backup_config_t* config);

/**
 * @brief Load device properties
 * 
 * @param config Backup configuration to update
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int config_load_device_properties(backup_config_t* config);

/**
 * @brief Load include properties
 * 
 * @param config Backup configuration to update
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int config_load_include_properties(backup_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_MANAGER_H */
