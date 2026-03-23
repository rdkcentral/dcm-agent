/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#ifndef BACKUP_ENGINE_H
#define BACKUP_ENGINE_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Execute HDD-enabled backup strategy
 * 
 * @param config Backup configuration
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_execute_hdd_enabled_strategy(const backup_config_t* config);

/**
 * @brief Execute HDD-disabled backup strategy with rotation
 * 
 * @param config Backup configuration
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_execute_hdd_disabled_strategy(const backup_config_t* config);

/**
 * @brief Execute common backup operations (special files, version files, notifications)
 * 
 * @param config Backup configuration
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_execute_common_operations(const backup_config_t* config);

/**
 * @brief Backup and recover logs with specified operation
 * 
 * @param source Source path
 * @param dest Destination path
 * @param op Backup operation type (move, copy, delete)
 * @param s_ext Source file extension filter
 * @param d_ext Destination file extension
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_and_recover_logs(const char* source, const char* dest, 
                           backup_operation_type_t op, const char* s_ext, 
                           const char* d_ext);

/**
 * @brief Move log files matching patterns (.txt, .log, bootlog)
 * 
 * @param source_dir Source directory path
 * @param dest_dir Destination directory path
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int move_log_files_by_pattern(const char* source_dir, const char* dest_dir);

#ifdef __cplusplus
}
#endif

#endif /* BACKUP_ENGINE_H */
