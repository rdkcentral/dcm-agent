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

#ifndef SPECIAL_FILES_H
#define SPECIAL_FILES_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize special files manager
 * 
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int special_files_init(void);

/**
 * @brief Cleanup special files manager
 */
void special_files_cleanup(void);

/**
 * @brief Load special files configuration from file
 * 
 * @param config Special files configuration structure
 * @param config_file Path to configuration file (one filename per line)
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int special_files_load_config(special_files_config_t* config, const char* config_file);

/**
 * @brief Validate special files configuration entry
 * 
 * @param entry Entry to validate
 * @return int BACKUP_SUCCESS if valid, error code if invalid
 */
int special_files_validate_entry(const special_file_entry_t* entry);

/**
 * @brief Execute single special file operation
 * 
 * @param entry Special file entry to process
 * @param backup_config Backup configuration for destination path
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int special_files_execute_entry(const special_file_entry_t* entry, 
                               const backup_config_t* backup_config);

/**
 * @brief Execute all special file operations from config
 * 
 * @param config Special files configuration
 * @param backup_config Backup configuration for destination path
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int special_files_execute_all(const special_files_config_t* config, 
                             const backup_config_t* backup_config);

#ifdef __cplusplus
}
#endif

#endif /* SPECIAL_FILES_H */
