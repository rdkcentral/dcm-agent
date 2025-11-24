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
 * @file file_operations.c
 * @brief File operations implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include "file_operations.h"
#include "system_utils.h"
#include "rdk_debug.h"

bool file_exists(const char* filepath)
{
    if (!filepath || filepath[0] == '\0') {
        return false;
    }
    // Use filePresentCheck from common_utilities
    return (filePresentCheck(filepath) == RDK_API_SUCCESS);
}

bool dir_exists(const char* dirpath)
{
    if (!dirpath || dirpath[0] == '\0') {
        return false;
    }
    // Use folderCheck from common_utilities
    return (folderCheck((char*)dirpath) == 1);
}

bool create_directory(const char* dirpath)
{
    if (!dirpath || dirpath[0] == '\0') {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid directory path\n", __FUNCTION__, __LINE__);
        return false;
    }

    // If directory already exists, return success
    if (dir_exists(dirpath)) {
        return true;
    }

    // Create a mutable copy of the path for createDir
    char path_copy[512];
    strncpy(path_copy, dirpath, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // Remove trailing slashes
    size_t len = strlen(path_copy);
    while (len > 1 && path_copy[len - 1] == '/') {
        path_copy[--len] = '\0';
    }

    // For recursive directory creation, we need to handle parent dirs
    char* p = path_copy;
    if (*p == '/') {
        p++; // Skip leading slash
    }

    for (; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!dir_exists(path_copy)) {
                // Use createDir from common_utilities
                if (createDir(path_copy) != RDK_API_SUCCESS) {
                    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to create directory %s\n", 
                            __FUNCTION__, __LINE__, path_copy);
                    return false;
                }
            }
            *p = '/';
        }
    }

    // Create the final directory
    if (!dir_exists(path_copy)) {
        // Use createDir from common_utilities
        if (createDir(path_copy) != RDK_API_SUCCESS) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to create directory %s\n", 
                    __FUNCTION__, __LINE__, path_copy);
            return false;
        }
    }

    return true;
}

bool remove_file(const char* filepath)
{
    if (!filepath || filepath[0] == '\0') {
        return false;
    }

    if (!file_exists(filepath)) {
        return true; // Already removed
    }

    // Use removeFile from common_utilities
    return (removeFile((char*)filepath) == RDK_API_SUCCESS);
}

bool remove_directory(const char* dirpath)
{
    if (!dirpath || dirpath[0] == '\0') {
        return false;
    }

    if (!dir_exists(dirpath)) {
        return true; // Already removed
    }

    // Use emptyFolder from common_utilities to remove contents
    if (emptyFolder((char*)dirpath) != RDK_API_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to empty directory %s\n", 
                __FUNCTION__, __LINE__, dirpath);
        return false;
    }

    // Remove the directory itself
    if (rmdir(dirpath) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to remove directory %s: %s\n", 
                __FUNCTION__, __LINE__, dirpath, strerror(errno));
        return false;
    }

    return true;
}

bool copy_file(const char* src, const char* dest)
{
    if (!src || !dest || src[0] == '\0' || dest[0] == '\0') {
        return false;
    }

    // Use copyFiles from common_utilities
    return (copyFiles((char*)src, (char*)dest) == RDK_API_SUCCESS);
}

long get_file_size(const char* filepath)
{
    if (!filepath || filepath[0] == '\0') {
        return -1;
    }

    // Use getFileSize from common_utilities
    int size = getFileSize(filepath);
    return (size >= 0) ? (long)size : -1L;
}

bool is_directory_empty(const char* dirpath)
{
    if (!dirpath || dirpath[0] == '\0') {
        return false;
    }

    if (!dir_exists(dirpath)) {
        return false;
    }

    DIR* dir = opendir(dirpath);
    if (!dir) {
        return false;
    }

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        count++;
        break; // Found at least one entry
    }

    closedir(dir);
    return (count == 0);
}

bool write_file(const char* filepath, const char* content)
{
    if (!filepath || filepath[0] == '\0' || !content) {
        return false;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open file %s for writing: %s\n", 
                __FUNCTION__, __LINE__, filepath, strerror(errno));
        return false;
    }

    size_t content_len = strlen(content);
    size_t written = fwrite(content, 1, content_len, file);
    fclose(file);

    if (written != content_len) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to write complete content to %s\n", 
                __FUNCTION__, __LINE__, filepath);
        return false;
    }

    return true;
}

int read_file(const char* filepath, char* buffer, size_t buffer_size)
{
    if (!filepath || filepath[0] == '\0' || !buffer || buffer_size == 0) {
        return -1;
    }

    FILE* file = fopen(filepath, "r");
    if (!file) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open file %s for reading: %s\n", 
                __FUNCTION__, __LINE__, filepath, strerror(errno));
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, buffer_size - 1, file);
    fclose(file);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Null terminate
    }

    return (int)bytes_read;
}
