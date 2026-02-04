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
 * @file usb_log_validation.c
 * @brief Validation module implementation for USB log upload
 *
 * This file contains the implementation of device compatibility verification,
 * USB mount point validation, and input parameter validation.
 */

#include "usb_log_validation.h"
#include "usb_log_utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Validate device compatibility
 * 
 * @return int 0 if compatible, negative error code otherwise
 */
int validate_device_compatibility(void)
{
    char device_name[256];
    
    /* Get DEVICE_NAME from device.properties */
    memset(device_name, 0, sizeof(device_name));
    if (getDevicePropertyData("DEVICE_NAME", device_name, sizeof(device_name)) != UTILS_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] ERROR! Cannot access DEVICE_NAME property\n", 
                __FUNCTION__, __LINE__);
        return 4;
    }
    
    /* Check if device is PLATCO (only supported device) */
    if (strcmp(device_name, "PLATCO") != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] ERROR! USB Log download not available on this device.\n", 
                __FUNCTION__, __LINE__);
        return 4; /* Exit code 4 matches original script */
    }
    
    return 0;
}

/**
 * @brief Validate USB mount point
 * 
 * @param mount_point Path to USB mount point
 * @return int 0 if valid, negative error code otherwise
 */
int validate_usb_mount_point(const char *mount_point)
{
    /* Check if mount point parameter is valid */
    if (!mount_point || mount_point[0] == '\0') {
        return -1;
    }
    
    /* Check if USB mount point directory exists */
    if (access(mount_point, F_OK) != 0) {
        char timestamp_buf[32];
        
        /* Get timestamp for logging */
        if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
            strcpy(timestamp_buf, "00/00/00-00:00:00");
        }
        
        /* Log error using RDK logger (matches original script) */
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] %s ERROR! USB drive is not mounted at %s\n", 
                __FUNCTION__, __LINE__, timestamp_buf, mount_point);
        
        return 2; /* Exit code 2 matches original script: "No USB" */
    }
    
    return 0;
}

/**
 * @brief Validate input parameters
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return int 0 if parameters valid, negative error code otherwise
 */
int validate_input_parameters(int argc, char *argv[])
{
    /* Check argument count - should be exactly 2 (program name + USB mount point) */
    if (argc != 2) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] USAGE: %s <USB mount point>\n", 
                __FUNCTION__, __LINE__, argv[0]);
        return 4; /* Exit code 4 matches original script */
    }
    
    /* Check if USB mount point argument is valid */
    if (!argv[1] || argv[1][0] == '\0') {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] USAGE: %s <USB mount point>\n", 
                __FUNCTION__, __LINE__, argv[0]);
        return 4;
    }
    
    return 0;
}