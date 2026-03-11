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

#ifndef BACKUP_LOGS_H
#define BACKUP_LOGS_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Main entry point for backup_logs system
 * 
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return int Return code (0 for success, negative for error)
 */
int backup_logs_main(int argc, char *argv[]);

/**
 * @brief Initialize backup system
 * 
 * @param config Backup configuration structure
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_logs_init(backup_config_t *config);

/**
 * @brief Execute complete backup process
 * 
 * @param config Backup configuration
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_logs_execute(const backup_config_t *config);

/**
 * @brief Cleanup and shutdown backup system
 * 
 * @param config Backup configuration
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int backup_logs_cleanup(backup_config_t *config);

/**
 * @brief Print version information
 */
void backup_logs_print_version(void);

/**
 * @brief Print usage information
 * 
 * @param program_name Name of the program
 */
void backup_logs_print_usage(const char *program_name);

#ifdef __cplusplus
}
#endif

#endif /* BACKUP_LOGS_H */
