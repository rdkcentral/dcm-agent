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
 * @file usb_log_main.h
 * @brief Main control module for USB log upload functionality
 *
 * This module provides the main entry point and high-level workflow
 * orchestration for USB log upload operations.
 */

#ifndef USB_LOG_MAIN_H
#define USB_LOG_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exit codes */
#define USB_LOG_SUCCESS              0
#define USB_LOG_ERROR_GENERAL        1
#define USB_LOG_ERROR_USB_NOT_MOUNTED 2
#define USB_LOG_ERROR_WRITE_ERROR    3
#define USB_LOG_ERROR_INVALID_USAGE  4

/**
 * @brief Execute USB log upload operation
 * 
 * @param usb_mount_point Path to USB mount point
 * @return int Exit code (0 on success, error code on failure)
 */
int usb_log_upload_execute(const char *usb_mount_point);

/**
 * @brief Print usage information
 * 
 * @param program_name Name of the program
 */
void print_usage(const char *program_name);

#ifdef __cplusplus
}
#endif

#endif /* USB_LOG_MAIN_H */