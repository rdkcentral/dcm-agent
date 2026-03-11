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

#ifndef SYS_INTEGRATION_H
#define SYS_INTEGRATION_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send systemd notification
 * 
 * @param message Notification message to send
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_send_systemd_notification(const char* message);

/**
 * @brief Execute external script
 * 
 * @param script_path Path to script to execute
 * @param args Arguments to pass to script
 * @param result_code Pointer to store script exit code
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_execute_script(const char* script_path, const char* args, int* result_code);

/**
 * @brief Get process status and resource usage
 * 
 * @param memory_usage Pointer to store memory usage in bytes
 * @param cpu_usage Pointer to store CPU usage percentage
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_get_process_status(long* memory_usage, float* cpu_usage);

/**
 * @brief Set signal handlers for graceful shutdown
 * 
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_setup_signal_handlers(void);

/**
 * @brief Check if system is in maintenance mode
 * 
 * @return bool true if in maintenance mode, false otherwise
 */
bool sys_is_maintenance_mode(void);

/**
 * @brief Lock process to prevent multiple instances
 * 
 * @param lock_file Path to lock file
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_acquire_process_lock(const char* lock_file);

/**
 * @brief Release process lock
 * 
 * @param lock_file Path to lock file
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_release_process_lock(const char* lock_file);

/**
 * @brief Get system uptime
 * 
 * @param uptime_seconds Pointer to store uptime in seconds
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_get_uptime(long* uptime_seconds);

/**
 * @brief Check if running as root/privileged user
 * 
 * @return bool true if privileged, false otherwise
 */
bool sys_is_privileged(void);

/**
 * @brief Initialize syslog for logging
 * 
 * @param program_name Program name for syslog
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_init_syslog(const char* program_name);

/**
 * @brief Close syslog
 */
void sys_close_syslog(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_INTEGRATION_H */
