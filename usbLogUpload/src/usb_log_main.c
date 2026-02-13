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
 * @file usb_log_main.c
 * @brief Main control module implementation for USB log upload
 *
 * This file contains the main entry point and high-level workflow
 * orchestration for USB log upload operations.
 */

#include "usb_log_main.h"
#include "usb_log_validation.h"
#include "usb_log_file_manager.h"
#include "usb_log_archive.h"
#include "usb_log_utils.h"
#include "context_manager.h"
#include "archive_manager.h"
#include <time.h>
#include <string.h>

/**
 * @brief Main application entry point
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit code
 */
int main(int argc, char *argv[])
{
    int ret;
    
    /* Initialize logging system */
    if (usb_log_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize logging system\n");
        return USB_LOG_ERROR_GENERAL;
    }
    
    /* Validate input parameters */
    ret = validate_input_parameters(argc, argv);
    if (ret != 0) {
        return ret; /* Returns exit code 4 for invalid usage */
    }
    
    /* Validate device compatibility */
    ret = validate_device_compatibility();
    if (ret != 0) {
        return ret; /* Returns exit code 4 for unsupported device */
    }
    
    /* Execute USB log upload operation */
    ret = usb_log_upload_execute(argv[1]);
    
    return ret;
}

/**
 * @brief Execute USB log upload operation
 * 
 * @param usb_mount_point Path to USB mount point
 * @return int Exit code (0 on success, error code on failure)
 */
int usb_log_upload_execute(const char *usb_mount_point)
{
    char usb_log_dir[512];
    char mac_address[32];
    char file_name[256];
    char log_file[256];
    char temp_dir[512];
    char archive_path[1024];
    char log_path[256];
    char timestamp_buf[32];
    int ret;
    
    /* Validate USB mount point */
    ret = validate_usb_mount_point(usb_mount_point);
    if (ret != 0) {
        return ret; /* Returns exit code 2 for USB not mounted */
    }
    
    /* Get LOG_PATH from properties */
    memset(log_path, 0, sizeof(log_path));
    if (getIncludePropertyData("LOG_PATH", log_path, sizeof(log_path)) != UTILS_SUCCESS) {
        strncpy(log_path, "/opt/logs", sizeof(log_path) - 1);
    }
    
    /* Build USB Log directory path: $USB_MNTP/Log */
    snprintf(usb_log_dir, sizeof(usb_log_dir), "%s/Log", usb_mount_point);
    
    /* Create USB log directory if it doesn't exist */
    ret = create_usb_log_directory(usb_log_dir);
    if (ret != 0) {
        return ret;
    }
    
    /* Get timestamp for logging */
    if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
        strncpy(timestamp_buf, "00/00/00-00:00:00", sizeof(timestamp_buf) - 1);
    }
    
    /* Log start message */
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] %s STARTING USB LOG UPLOAD\n", 
            __FUNCTION__, __LINE__, timestamp_buf);
    
    /* Get MAC address */
    if (!get_mac_address(mac_address, sizeof(mac_address))) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to get MAC address\n", __FUNCTION__, __LINE__);
        return USB_LOG_ERROR_GENERAL;
    }
    
    /* Generate archive filename using uploadstblogs function */
    if (!generate_archive_name(log_file, sizeof(log_file), mac_address, "Logs")) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to generate archive filename\n", __FUNCTION__, __LINE__);
        return USB_LOG_ERROR_GENERAL;
    }
    
    /* Extract base filename without .tgz extension for temp directory name */
    strncpy(file_name, log_file, sizeof(file_name) - 1);
    file_name[sizeof(file_name) - 1] = '\0';
    char *ext = strstr(file_name, ".tgz");
    if (ext) {
        *ext = '\0';  /* Remove .tgz extension */
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] %s Folder: %s\n", 
            __FUNCTION__, __LINE__, timestamp_buf, usb_log_dir);
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] %s File: %s\n", 
            __FUNCTION__, __LINE__, timestamp_buf, file_name);
    
    /* Create temporary directory: /opt/tmpusb/$FILE_NAME */
    ret = create_temporary_directory(file_name, temp_dir, sizeof(temp_dir));
    if (ret != 0) {
        return ret; /* Returns exit code 3 for writing error */
    }
    
    /* Move log files from LOG_PATH to temp directory */
    ret = move_log_files(log_path, temp_dir);
    if (ret != 0) {
        cleanup_temporary_files(temp_dir);
        return ret;
    }
    
    /* Send SIGHUP to reload syslog-ng if enabled */
    reload_syslog_service();
    
    /* Build full archive path: $USB_LOG/$LOG_FILE */
    snprintf(archive_path, sizeof(archive_path), "%s/%s", usb_log_dir, log_file);
    
    /* Create compressed archive */
    ret = create_usb_log_archive(temp_dir, archive_path, mac_address);
    if (ret != 0) {
        cleanup_temporary_files(temp_dir);
        perform_filesystem_sync();
        return ret; /* Returns exit code 3 for writing error */
    }
    
    /* Output archive path (matches shell script: echo $USB_LOG_FILE) */
    printf("%s\n", archive_path);
    
    /* Cleanup temporary directory */
    cleanup_temporary_files(temp_dir);
    
    /* Sync USB drive to flush everything to external storage */
    perform_filesystem_sync();
    
    /* Get timestamp for completion log */
    if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
        strncpy(timestamp_buf, "00/00/00-00:00:00", sizeof(timestamp_buf) - 1);
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] %s COMPLETED USB LOG UPLOAD\n", 
            __FUNCTION__, __LINE__, timestamp_buf);
    
    return USB_LOG_SUCCESS;
}