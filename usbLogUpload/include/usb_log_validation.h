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
 * @file usb_log_validation.h
 * @brief Validation module for USB log upload operations
 *
 * This module provides device compatibility verification, USB mount point
 * validation, and input parameter validation.
 */

#ifndef USB_LOG_VALIDATION_H
#define USB_LOG_VALIDATION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate device compatibility
 * 
 * Checks if the current device supports USB log upload functionality.
 * Currently only PLATCO devices are supported.
 * 
 * @return int 0 if compatible, negative error code otherwise
 */
int validate_device_compatibility(void);

/**
 * @brief Validate USB mount point
 * 
 * Verifies that the provided USB mount point exists and is accessible.
 * 
 * @param mount_point Path to USB mount point
 * @return int 0 if valid, negative error code otherwise
 */
int validate_usb_mount_point(const char *mount_point);

/**
 * @brief Validate system prerequisites
 * 
 * Checks that all required system components and utilities are available.
 * 
 * @return int 0 if all prerequisites met, negative error code otherwise
 */
int validate_system_prerequisites(void);

/**
 * @brief Validate input parameters
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return int 0 if parameters valid, negative error code otherwise
 */
int validate_input_parameters(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* USB_LOG_VALIDATION_H */