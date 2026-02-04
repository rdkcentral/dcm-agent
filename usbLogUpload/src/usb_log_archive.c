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
 * @file usb_log_archive.c
 * @brief Archive management module implementation for USB log upload
 *
 * This file contains the implementation of log file compression,
 * archiving, and archive naming convention.
 */

#include "usb_log_archive.h"
#include "usb_log_utils.h"
#include "context_manager.h"
#include "archive_manager.h"
#include "uploadstblogs_types.h"
#include <sys/stat.h>
#include <string.h>

/**
 * @brief Create compressed archive for USB log upload
 * 
 * Uses uploadstblogs archive API to create tar.gz archive
 * 
 * @param source_dir Directory containing files to archive
 * @param archive_path Full path to output archive file
 * @param mac_address Device MAC address for filename generation
 * @return int 0 on success, negative error code on failure
 */
int create_usb_log_archive(const char *source_dir, const char *archive_path, const char *mac_address)
{
    if (!source_dir || !archive_path || !mac_address) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    /* Check if source directory exists */
    struct stat st;
    if (stat(source_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Source directory does not exist: %s\n", __FUNCTION__, __LINE__, source_dir);
        return -2;
    }

    /* Initialize minimal runtime context for archive creation */
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    /* Set essential context fields */
    strncpy(ctx.mac_address, mac_address, sizeof(ctx.mac_address) - 1);
    ctx.mac_address[sizeof(ctx.mac_address) - 1] = '\0';
    
    /* Initialize minimal session state */
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.trigger = TRIGGER_MANUAL;  /* USB upload is manual trigger */

    /* Use uploadstblogs create_archive function */
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Creating archive from %s with MAC %s\n", 
            __FUNCTION__, __LINE__, source_dir, mac_address);

    int result = create_archive(&ctx, &session, source_dir);
    if (result != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to create archive from %s (error: %d)\n", 
                __FUNCTION__, __LINE__, source_dir, result);
        return -3;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] Successfully created archive from %s\n", 
            __FUNCTION__, __LINE__, source_dir);

    return 0;
}

/**
 * @brief Generate archive filename following naming convention
 * 
 * @param filename_buffer Buffer to store generated filename
 * @param buffer_size Size of filename_buffer
 * @return int 0 on success, negative error code on failure
 */
int generate_archive_filename(char *filename_buffer, size_t buffer_size)
{
    if (!filename_buffer || buffer_size < 64) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Invalid parameters or buffer too small\n", __FUNCTION__, __LINE__);
        return -1;
    }

    /* Get MAC address using uploadstblogs function */
    char mac_address[32];
    if (!get_mac_address(mac_address, sizeof(mac_address))) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to get MAC address\n", __FUNCTION__, __LINE__);
        return -2;
    }

    /* Use uploadstblogs generate_archive_name function with "Logs" prefix */
    if (!generate_archive_name(filename_buffer, buffer_size, mac_address, "Logs")) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to generate archive name\n", __FUNCTION__, __LINE__);
        return -3;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Generated archive filename: %s\n", __FUNCTION__, __LINE__, filename_buffer);

    return 0;
}