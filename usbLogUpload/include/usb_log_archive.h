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
 * @file usb_log_archive.h
 * @brief Archive management module for USB log upload operations
 *
 * This module handles log file compression, archiving, and archive
 * naming convention implementation.
 */

#ifndef USB_LOG_ARCHIVE_H
#define USB_LOG_ARCHIVE_H

#include <stddef.h>

/* Use shared archive functionality from uploadstblogs */
#include "archive_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create compressed archive for USB log upload
 * 
 * Wrapper around uploadstblogs create_archive() function for USB-specific requirements
 * 
 * @param source_dir Directory containing files to archive
 * @param archive_path Full path to output archive file
 * @param mac_address Device MAC address for filename generation
 * @return int 0 on success, negative error code on failure
 */
int create_usb_log_archive(const char *source_dir, const char *archive_path, const char *mac_address);

/* Note: The following functions are available from uploadstblogs/archive_manager.h:
 * - generate_archive_name() for filename generation with MAC and timestamp
 * - create_archive() for tar.gz archive creation
 * - get_archive_size() for archive size validation
 */

#ifdef __cplusplus
}
#endif

#endif /* USB_LOG_ARCHIVE_H */