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
 * @file usb_log_utils.h
 * @brief Utility functions for USB log upload operations
 *
 * This module provides common utility functions including logging,
 * configuration management, and error handling.
 */

#ifndef USB_LOG_UTILS_H
#define USB_LOG_UTILS_H

#include <stddef.h>
#include "rdk_fwdl_utils.h"
#include "common_device_api.h"
#include "rdk_debug.h"

/* RDK utility constants */
#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 1
#endif
#ifndef UTILS_FAIL
#define UTILS_FAIL -1
#endif

/* RDK Logging component name for USB Log Upload */
#define LOG_USB_UPLOAD "LOG.RDK.USBLOGUPLOAD"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize logging system
 * 
 * @return int 0 on success, negative error code on failure
 */
int usb_log_init(void);

/**
 * @brief Send signal to reload syslog-ng service
 * 
 * @return int 0 on success, negative error code on failure
 */
int reload_syslog_service(void);

/**
 * @brief Perform filesystem sync operation
 * 
 * @return int 0 on success, negative error code on failure
 */
int perform_filesystem_sync(void);

/**
 * @brief Get current timestamp for logging
 * 
 * @param timestamp_buffer Buffer to store timestamp
 * @param buffer_size Size of timestamp_buffer
 * @return int 0 on success, negative error code on failure
 */
int get_current_timestamp(char *timestamp_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* USB_LOG_UTILS_H */