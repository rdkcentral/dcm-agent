/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file archive_manager.c
 * @brief Archive management implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <archive.h>
#include <archive_entry.h>
#include "archive_manager.h"
#include "log_collector.h"
#include "file_operations.h"
#include "system_utils.h"
#include "strategy_handler.h"
#include "rdk_debug.h"

/**
 * @brief Generate archive filename with MAC and timestamp (script format)
 * @param buffer Buffer to store filename
 * @param buffer_size Size of buffer
 * @param mac_address Device MAC address
 * @param prefix Filename prefix ("Logs" or "DRI_Logs")
 * @return true on success, false on failure
 * 
 * Format: <MAC>_<prefix>_<MM-DD-YY-HH-MMAM/PM>.tgz
 * Example: AA-BB-CC-DD-EE-FF_Logs_11-25-25-02-30PM.tgz
 *          AA-BB-CC-DD-EE-FF_DRI_Logs_11-25-25-02-30PM.tgz
 */
static bool generate_archive_name(char* buffer, size_t buffer_size, 
                                   const char* mac_address, const char* prefix)
{
    if (!buffer || !mac_address || !prefix || buffer_size < 64) {
        return false;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    if (!tm_info) {
        return false;
    }

    char timestamp[32];
    // Format: MM-DD-YY-HH-MMAM/PM (matches script: date "+%m-%d-%y-%I-%M%p")
    strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p", tm_info);
    
    // Format: <MAC>_<prefix>_<timestamp>.tgz (matches script format)
    snprintf(buffer, buffer_size, "%s_%s_%s.tgz", mac_address, prefix, timestamp);
    return true;
}

/**
 * @brief Add a file to archive using libarchive
 * @param archive Archive handle
 * @param filepath Full path to file
 * @param archive_path Path within archive
 * @return 0 on success, -1 on failure
 */
static int add_file_to_archive(struct archive* archive, const char* filepath, const char* archive_path)
{
    struct archive_entry* entry;
    struct stat st;
    char buff[16384];
    int fd;
    ssize_t len;

    if (stat(filepath, &st) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to stat file: %s\n", __FUNCTION__, __LINE__, filepath);
        return -1;
    }

    // Skip directories, sockets, etc. - only regular files
    if (!S_ISREG(st.st_mode)) {
        return 0;
    }

    entry = archive_entry_new();
    archive_entry_set_pathname(entry, archive_path);
    archive_entry_set_size(entry, st.st_size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_mtime(entry, st.st_mtime, 0);

    if (archive_write_header(archive, entry) != ARCHIVE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to write archive header: %s\n", 
                __FUNCTION__, __LINE__, archive_error_string(archive));
        archive_entry_free(entry);
        return -1;
    }

    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open file: %s\n", __FUNCTION__, __LINE__, filepath);
        archive_entry_free(entry);
        return -1;
    }

    while ((len = read(fd, buff, sizeof(buff))) > 0) {
        if (archive_write_data(archive, buff, len) < 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to write data: %s\n", 
                    __FUNCTION__, __LINE__, archive_error_string(archive));
            close(fd);
            archive_entry_free(entry);
            return -1;
        }
    }

    close(fd);
    archive_entry_free(entry);
    return 0;
}

/**
 * @brief Add directory contents to archive recursively
 * @param archive Archive handle
 * @param dirpath Directory to add
 * @param base_path Base path to strip from archive paths
 * @return 0 on success, -1 on failure
 */
static int add_directory_to_archive(struct archive* archive, const char* dirpath, const char* base_path)
{
    DIR* dir = opendir(dirpath);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open directory: %s\n", __FUNCTION__, __LINE__, dirpath);
        return -1;
    }

    struct dirent* entry;
    int base_len = strlen(base_path);
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fullpath[MAX_PATH_LENGTH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            continue;
        }

        // Calculate relative path for archive
        const char* archive_path = fullpath + base_len;
        if (archive_path[0] == '/') {
            archive_path++; // Skip leading slash
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively add subdirectory
            if (add_directory_to_archive(archive, fullpath, base_path) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Add regular file
            if (add_file_to_archive(archive, fullpath, archive_path) != 0) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                        "[%s:%d] Failed to add file to archive: %s\n", 
                        __FUNCTION__, __LINE__, fullpath);
                // Continue with other files
            }
        }
    }

    closedir(dir);
    return 0;
}

bool prepare_archive(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Preparing archive using strategy handler\n", __FUNCTION__, __LINE__);

    // Use strategy handler pattern to execute complete workflow
    int ret = execute_strategy_workflow(ctx, session);
    
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Strategy workflow failed\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Archive preparation completed successfully\n", __FUNCTION__, __LINE__);
    
    return true;
}

bool prepare_rrd_archive(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    // RRD strategy: Upload single RRD log file directly (no collection phase)
    // Note: RRD filename is provided via command line argument (RRD_UPLOADLOG_FILE)
    const char* rrd_file = ctx->paths.rrd_file;
    
    if (strlen(rrd_file) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] RRD log file path not configured\n", __FUNCTION__, __LINE__);
        return false;
    }

    if (!file_exists(rrd_file)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] RRD log file does not exist: %s\n", 
                __FUNCTION__, __LINE__, rrd_file);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Preparing RRD archive from: %s\n", 
            __FUNCTION__, __LINE__, rrd_file);

    // For RRD, the archive path is the rrd_file itself (already a tar.gz from command line)
    // Just validate and store the filename
    const char* filename = strrchr(rrd_file, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = rrd_file;
    }

    // RRD file is already provided as archive (tar.gz) via command line
    long size = get_archive_size(rrd_file);
    if (size > 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] RRD archive ready for upload, size: %ld bytes\n", 
                __FUNCTION__, __LINE__, size);
        
        // Store archive filename in session
        strncpy(session->archive_file, filename, sizeof(session->archive_file) - 1);
        session->archive_file[sizeof(session->archive_file) - 1] = '\0';
        return true;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] RRD archive file invalid or empty\n", __FUNCTION__, __LINE__);
        return false;
    }
}

long get_archive_size(const char* archive_path)
{
    if (!archive_path) {
        return -1;
    }

    struct stat st;
    if (stat(archive_path, &st) == 0) {
        return st.st_size;
    }

    return -1;
}

/**
 * @brief Create tar.gz archive from directory
 * @param ctx Runtime context
 * @param session Session state
 * @param source_dir Source directory to archive
 * @return 0 on success, -1 on failure
 */
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir)
{
    if (!ctx || !session || !source_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dir_exists(source_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Source directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, source_dir);
        return -1;
    }

    // Generate archive filename with MAC and timestamp (script format)
    char archive_filename[MAX_FILENAME_LENGTH];
    if (!generate_archive_name(archive_filename, sizeof(archive_filename), 
                               ctx->device.mac_address, "Logs")) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to generate archive filename\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Archive created in source directory
    char archive_path[MAX_PATH_LENGTH];
    snprintf(archive_path, sizeof(archive_path), "%s/%s", source_dir, archive_filename);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Creating archive: %s from %s\n", 
            __FUNCTION__, __LINE__, archive_path, source_dir);

    // Create tar.gz archive using libarchive
    struct archive* a = archive_write_new();
    if (!a) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive handle\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Set format to ustar tar and gzip compression
    archive_write_add_filter_gzip(a);
    archive_write_set_format_ustar(a);

    // Open the archive file
    if (archive_write_open_filename(a, archive_path) != ARCHIVE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open archive file: %s\n", 
                __FUNCTION__, __LINE__, archive_error_string(a));
        archive_write_free(a);
        return -1;
    }

    // Add all files from source directory
    int ret = add_directory_to_archive(a, source_dir, source_dir);
    
    // Close the archive
    if (archive_write_close(a) != ARCHIVE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to close archive: %s\n", 
                __FUNCTION__, __LINE__, archive_error_string(a));
        archive_write_free(a);
        return -1;
    }
    
    archive_write_free(a);
    
    if (ret == 0 && file_exists(archive_path)) {
        long size = get_archive_size(archive_path);
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Archive created successfully, size: %ld bytes\n", 
                __FUNCTION__, __LINE__, size);
        
        // Store archive filename in session
        strncpy(session->archive_file, archive_filename, sizeof(session->archive_file) - 1);
        session->archive_file[sizeof(session->archive_file) - 1] = '\0';
        return 0;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive, return code: %d\n", 
                __FUNCTION__, __LINE__, ret);
        return -1;
    }
}

/**
 * @brief Create DRI logs archive
 * @param ctx Runtime context
 * @param archive_path Output archive file path
 * @return 0 on success, -1 on failure
 */
int create_dri_archive(RuntimeContext* ctx, const char* archive_path)
{
    if (!ctx || !archive_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (strlen(ctx->paths.dri_log_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] DRI log path not configured\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dir_exists(ctx->paths.dri_log_path)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] DRI log directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.dri_log_path);
        return -1;
    }

    // Generate DRI archive filename with MAC and timestamp (script format)
    char dri_filename[MAX_FILENAME_LENGTH];
    if (!generate_archive_name(dri_filename, sizeof(dri_filename), 
                               ctx->device.mac_address, "DRI_Logs")) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to generate DRI archive filename\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Use provided archive_path or construct in temp dir
    char final_archive_path[MAX_PATH_LENGTH];
    if (archive_path && strlen(archive_path) > 0) {
        // Extract directory from provided path and use generated filename
        const char* last_slash = strrchr(archive_path, '/');
        if (last_slash) {
            size_t dir_len = last_slash - archive_path;
            snprintf(final_archive_path, sizeof(final_archive_path), "%.*s/%s", 
                     (int)dir_len, archive_path, dri_filename);
        } else {
            snprintf(final_archive_path, sizeof(final_archive_path), "%s", dri_filename);
        }
    } else {
        snprintf(final_archive_path, sizeof(final_archive_path), "/tmp/%s", dri_filename);
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Creating DRI archive: %s from %s\n", 
            __FUNCTION__, __LINE__, final_archive_path, ctx->paths.dri_log_path);

    // Create tar.gz archive using libarchive
    struct archive* a = archive_write_new();
    if (!a) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create archive handle\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Set format to ustar tar and gzip compression
    archive_write_add_filter_gzip(a);
    archive_write_set_format_ustar(a);

    // Open the archive file
    if (archive_write_open_filename(a, final_archive_path) != ARCHIVE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open archive file: %s\n", 
                __FUNCTION__, __LINE__, archive_error_string(a));
        archive_write_free(a);
        return -1;
    }

    // Add all files from DRI log directory
    int ret = add_directory_to_archive(a, ctx->paths.dri_log_path, ctx->paths.dri_log_path);
    
    // Close the archive
    if (archive_write_close(a) != ARCHIVE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to close archive: %s\n", 
                __FUNCTION__, __LINE__, archive_error_string(a));
        archive_write_free(a);
        return -1;
    }
    
    archive_write_free(a);
    
    if (ret == 0 && file_exists(final_archive_path)) {
        long size = get_archive_size(final_archive_path);
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] DRI archive created successfully, size: %ld bytes\n", 
                __FUNCTION__, __LINE__, size);
        return 0;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create DRI archive, return code: %d\n", 
                __FUNCTION__, __LINE__, ret);
        return -1;
    }
}

