/**
 * Copyright 2020 RDK Management
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
 * @file usb_log_file_manager.h
 * @brief File management module for USB log upload operations
 *
 * This module handles log file discovery, directory operations,
 * and file movement operations.
 */

#ifndef USB_LOG_FILE_MANAGER_H
#define USB_LOG_FILE_MANAGER_H

/* Use shared file operations from uploadstblogs */
#include "../uploadstblogs/include/file_operations.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create USB log directory on the USB device
 * 
 * @param usb_path Base USB mount path
 * @return int 0 on success, negative error code on failure
 */
int create_usb_log_directory(const char *usb_path);

/**
 * @brief Create temporary directory for log processing
 * 
 * @param file_name Base filename for temporary directory
 * @param temp_dir_path Buffer to store created temporary directory path
 * @param buffer_size Size of temp_dir_path buffer
 * @return int 0 on success, negative error code on failure
 */
int create_temporary_directory(const char *file_name, char *temp_dir_path, size_t buffer_size);

/* Note: The following functions are available from uploadstblogs/file_operations.h:
 * - move_directory_contents() for moving log files
 * - remove_directory() for cleanup
 * - create_directory() for ensuring directories exist
 * - file_exists(), dir_exists() for validation
 */

#ifdef __cplusplus
}
#endif

#endif /* USB_LOG_FILE_MANAGER_H */