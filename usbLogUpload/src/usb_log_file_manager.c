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
 * @file usb_log_file_manager.c
 * @brief File management module implementation for USB log upload
 *
 * This file contains the implementation of log file discovery,
 * directory operations, and file movement operations.
 */

#define _DEFAULT_SOURCE
#include "usb_log_file_manager.h"
#include "usb_log_utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "../uploadstblogs/include/file_operations.h"

/**
 * @brief Create USB log directory on the USB device
 * 
 * @param usb_log_path Full path to USB log directory (e.g., /mnt/usb/Log)
 * @return int 0 on success, negative error code on failure
 */
int create_usb_log_directory(const char *usb_log_path)
{
    if (!usb_log_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Invalid parameter\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    /* Check if directory already exists */
    if (dir_exists(usb_log_path)) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
                "[%s:%d] USB log directory already exists: %s\n", 
                __FUNCTION__, __LINE__, usb_log_path);
        return 0;
    }
    
    /* Create directory (mkdir -p behavior) */
    if (!create_directory(usb_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to create USB log directory: %s\n", 
                __FUNCTION__, __LINE__, usb_log_path);
        return -2;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] Created USB log directory: %s\n", 
            __FUNCTION__, __LINE__, usb_log_path);
    
    return 0;
}

/**
 * @brief Move log files from source to destination
 * 
 * Moves all files from LOG_PATH to temp directory.
 * Matches shell script: mv $LOG_PATH/ * $USB_DIR/.
 * 
 * @param source_path Source directory path (LOG_PATH)
 * @param dest_path Destination directory path (temp directory)
 * @return int 0 on success, negative error code on failure
 */
int move_log_files(const char *source_path, const char *dest_path)
{
    DIR *dir;
    struct dirent *entry;
    char src_file[1024];
    char dst_file[1024];
    int file_count = 0;
    int moved_count = 0;
    
    if (!source_path || !dest_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Moving log files from %s to %s\n", 
            __FUNCTION__, __LINE__, source_path, dest_path);
    
    /* Open source directory */
    dir = opendir(source_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to open source directory: %s\n", 
                __FUNCTION__, __LINE__, source_path);
        return -2;
    }
    
    /* Iterate through all entries in directory */
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. directories */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Build source file path */
        snprintf(src_file, sizeof(src_file), "%s/%s", source_path, entry->d_name);
        
        /* Build destination file path */
        snprintf(dst_file, sizeof(dst_file), "%s/%s", dest_path, entry->d_name);
        
        /* Check if it's a regular file (skip directories) */
        struct stat st;
        if (stat(src_file, &st) == 0 && S_ISREG(st.st_mode)) {
            file_count++;
            
            /* Move file using rename() */
            if (rename(src_file, dst_file) == 0) {
                moved_count++;
                RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
                        "[%s:%d] Moved: %s\n", __FUNCTION__, __LINE__, entry->d_name);
            } else {
                /* Log warning but continue with other files */
                RDK_LOG(RDK_LOG_WARN, LOG_USB_UPLOAD, 
                        "[%s:%d] Failed to move %s: %s\n", 
                        __FUNCTION__, __LINE__, entry->d_name, strerror(errno));
            }
        }
    }
    
    closedir(dir);
    
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] Moved %d of %d files from %s to %s\n", 
            __FUNCTION__, __LINE__, moved_count, file_count, source_path, dest_path);
    
    return 0;
}

/**
 * @brief Cleanup temporary files and directories
 * 
 * Removes temporary directory and all its contents.
 * Matches shell script: rm -r $USB_DIR
 * 
 * @param temp_path Temporary directory path to cleanup
 * @return int 0 on success, negative error code on failure
 */
int cleanup_temporary_files(const char *temp_path)
{
    if (!temp_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Invalid parameter\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Cleaning up temporary directory: %s\n", 
            __FUNCTION__, __LINE__, temp_path);
    
    /* Remove directory recursively */
    if (!remove_directory(temp_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to remove temporary directory: %s\n", 
                __FUNCTION__, __LINE__, temp_path);
        return -2;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Temporary directory cleaned up successfully\n", __FUNCTION__, __LINE__);
    
    return 0;
}

/**
 * @brief Create temporary directory for log processing
 * 
 * @param file_name Base filename for temporary directory
 * @param temp_dir_path Buffer to store created temporary directory path
 * @param buffer_size Size of temp_dir_path buffer
 * @return int 0 on success, negative error code on failure
 */
int create_temporary_directory(const char *file_name, char *temp_dir_path, size_t buffer_size)
{
    char timestamp_buf[32];
    
    /* Build temporary directory path: /opt/tmpusb/<file_name> */
    if (snprintf(temp_dir_path, buffer_size, "/opt/tmpusb/%s", file_name) >= (int)buffer_size) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Temporary directory path too long\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    /* Create directory with parents (like mkdir -p) */
    if (!create_directory(temp_dir_path)) {
        /* Get timestamp for logging */
        if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
            strcpy(timestamp_buf, "00/00/00-00:00:00");
        }
        
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] %s ERROR! Failed to create %s\n", 
                __FUNCTION__, __LINE__, timestamp_buf, temp_dir_path);
        return 3; /* Exit code 3 matches original script: "Writing error" */
    }
    
    /* Perform sync to ensure directory is flushed to storage */
    sync();
    
    /* Verify directory was actually created */
    if (access(temp_dir_path, F_OK) != 0) {
        /* Get timestamp for logging */
        if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
            strcpy(timestamp_buf, "00/00/00-00:00:00");
        }
        
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] %s ERROR! Failed to create %s\n", 
                __FUNCTION__, __LINE__, timestamp_buf, temp_dir_path);
        return 3; /* Exit code 3 matches original script: "Writing error" */
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Created temporary directory: %s\n", 
            __FUNCTION__, __LINE__, temp_dir_path);
    
    return 0;
}