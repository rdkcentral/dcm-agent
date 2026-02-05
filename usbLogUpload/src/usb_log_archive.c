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
#include <errno.h>

/**
 * @brief Create compressed archive for USB log upload
 * 
 * Uses uploadstblogs archive API to create tar.gz archive, then moves it
 * to the specified destination path.
 * 
 * @param source_dir Directory containing files to archive
 * @param archive_path Full path to output archive file
 * @param mac_address Device MAC address for filename generation
 * @return int 0 on success, negative error code on failure
 */
int create_usb_log_archive(const char *source_dir, const char *archive_path, const char *mac_address)
{
    char timestamp_buf[32];
    char temp_archive_path[512];
    char archive_filename[256];
    
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

    /* Get timestamp for logging */
    if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
        strncpy(timestamp_buf, "00/00/00-00:00:00", sizeof(timestamp_buf) - 1);
    }

    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] %s ARCHIVE AND COMPRESS TO %s\n", 
            __FUNCTION__, __LINE__, timestamp_buf, archive_path);

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

    /* Use uploadstblogs create_archive function - creates archive in source_dir */
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Creating archive from %s with MAC %s\n", 
            __FUNCTION__, __LINE__, source_dir, mac_address);

    int result = create_archive(&ctx, &session, source_dir);
    if (result != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] %s USB WRITING ERROR - Failed to create archive (error: %d)\n", 
                __FUNCTION__, __LINE__, timestamp_buf, result);
        return 3; /* Exit code 3 matches original script: "Writing Error" */
    }

    /* Archive was created in source_dir with auto-generated name
     * session.archive_file contains the filename (stored by create_archive)
     * We need to move it to the desired USB destination path
     */
    if (session.archive_file[0] != '\0') {
        /* Archive filename was stored in session */
        snprintf(temp_archive_path, sizeof(temp_archive_path), "%s/%s", 
                 source_dir, session.archive_file);
    } else {
        /* Fallback: generate the expected filename */
        if (!generate_archive_name(archive_filename, sizeof(archive_filename), 
                                   mac_address, "Logs")) {
            RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                    "[%s:%d] Failed to determine archive filename\n", __FUNCTION__, __LINE__);
            return 3;
        }
        snprintf(temp_archive_path, sizeof(temp_archive_path), "%s/%s", 
                 source_dir, archive_filename);
    }

    /* Move the archive from source_dir to the USB destination */
    if (rename(temp_archive_path, archive_path) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] %s USB WRITING ERROR - Failed to move archive to %s: %s\n", 
                __FUNCTION__, __LINE__, timestamp_buf, archive_path, strerror(errno));
        return 3; /* Exit code 3: Writing Error */
    }

    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] Successfully created archive: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    return 0;
}