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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include "file_operations.h"
#include "system_utils.h"
#include "rdk_debug.h"
#include "uploadstblogs_types.h"

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

bool join_path(char* buffer, size_t buffer_size, const char* dir, const char* filename)
{
    if (!buffer || !dir || !filename) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(filename);
    
    // Check if directory path ends with a slash
    bool has_trailing_slash = (dir_len > 0 && dir[dir_len - 1] == '/');
    bool needs_separator = !has_trailing_slash;
    
    // Calculate required size: dir + separator (if needed) + filename + null terminator
    size_t required = dir_len + (needs_separator ? 1 : 0) + file_len + 1;
    
    if (required > buffer_size) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Path too long: %zu > %zu\n", 
                __FUNCTION__, __LINE__, required, buffer_size);
        return false;
    }
    
    // Build the path
    strcpy(buffer, dir);
    if (needs_separator) {
        strcat(buffer, "/");
    }
    strcat(buffer, filename);
    
    return true;
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

bool has_log_files(const char* dirpath)
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
    bool found = false;

    // Script checks specifically for *.txt and *.log files
    // uploadLogOnDemand line 741: ret=`ls $LOG_PATH/*.txt`
    // uploadLogOnReboot line 805: ret=`ls $PREV_LOG_PATH/*.txt`
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if file ends with .txt or .log (matches script behavior)
        const char* name = entry->d_name;
        size_t len = strlen(name);
        
        if (len > 4 && (strcmp(name + len - 4, ".txt") == 0 || strcmp(name + len - 4, ".log") == 0)) {
            found = true;
            break; // Found at least one .txt or .log file
        }
    }

    closedir(dir);
    return found;
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

/**
 * @brief Add timestamp prefix to all files in directory
 * @param dir_path Directory containing files to rename
 * @return 0 on success, -1 on failure
 */
// Global to store timestamp prefix for removal
static char g_timestamp_prefix[32] = {0};

int add_timestamp_to_files(const char* dir_path)
{
    if (!dir_path || !dir_exists(dir_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid or non-existent directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path ? dir_path : "NULL");
        return -1;
    }

    // Get current timestamp in script format: MM-DD-YY-HH-MMAM/PM-
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p-", tm_info);
    
    // Store timestamp prefix globally for removal later (matches script behavior)
    strncpy(g_timestamp_prefix, timestamp, sizeof(g_timestamp_prefix) - 1);

    DIR* dir = opendir(dir_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path);
        return -1;
    }

    int success_count = 0;
    int error_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and special entries
        if (entry->d_name[0] == '.' || 
            strcmp(entry->d_name, "..") == 0 ||
            strncmp(entry->d_name, timestamp, strlen(timestamp)) == 0) {
            continue;
        }

        char old_path[MAX_PATH_LENGTH];
        char new_path[MAX_PATH_LENGTH];
        
        int old_ret = snprintf(old_path, sizeof(old_path), "%s/%s", dir_path, entry->d_name);
        int new_ret = snprintf(new_path, sizeof(new_path), "%s/%s%s", dir_path, timestamp, entry->d_name);
        
        // Check for snprintf truncation
        if (old_ret < 0 || old_ret >= (int)sizeof(old_path) ||
            new_ret < 0 || new_ret >= (int)sizeof(new_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Path too long, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }

        // Rename directly without pre-check to avoid TOCTOU issue
        if (rename(old_path, new_path) == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] Renamed: %s -> %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name, new_path);
            success_count++;
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to rename %s: %s\n", 
                    __FUNCTION__, __LINE__, old_path, strerror(errno));
            error_count++;
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Timestamp added to %d files, %d errors\n", 
            __FUNCTION__, __LINE__, success_count, error_count);

    return (error_count > 0) ? -1 : 0;
}

/**
 * @brief Remove timestamp prefix from all files in directory
 * @param dir_path Directory containing files to rename
 * @return 0 on success, -1 on failure
 */
int remove_timestamp_from_files(const char* dir_path)
{
    if (!dir_path || !dir_exists(dir_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid or non-existent directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path ? dir_path : "NULL");
        return -1;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path);
        return -1;
    }

    // Get stored timestamp prefix length (matches script behavior: cut -c$len-)
    size_t prefix_len = strlen(g_timestamp_prefix);
    
    if (prefix_len == 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] No timestamp prefix stored, attempting pattern detection\n", 
                __FUNCTION__, __LINE__);
    }

    int success_count = 0;
    int error_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and special entries
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Look for files with timestamp prefix matching script pattern
        // Pattern: MM-DD-YY-HH-MMAM/PM- (matches script modifyTimestampPrefixWithOriginalName)
        int has_timestamp = 0;
        size_t cut_pos = prefix_len;
        
        if (prefix_len > 0 && strlen(entry->d_name) > prefix_len) {
            // Use stored prefix length (matches script: cut -c$len-)
            has_timestamp = (strncmp(entry->d_name, g_timestamp_prefix, prefix_len) == 0);
        } else if (strlen(entry->d_name) > 19) {
            // Fallback pattern detection: XX-XX-XX-XX-XXAM/PM- or XX-XX-XX-XX-XXPM-
            has_timestamp = (entry->d_name[2] == '-' && entry->d_name[5] == '-' && 
                            entry->d_name[8] == '-' && entry->d_name[11] == '-');
            if (has_timestamp) {
                // Find the end of timestamp (look for AM- or PM-)
                const char* am_pos = strstr(entry->d_name, "AM-");
                const char* pm_pos = strstr(entry->d_name, "PM-");
                if (am_pos) {
                    cut_pos = (am_pos - entry->d_name) + 3;
                } else if (pm_pos) {
                    cut_pos = (pm_pos - entry->d_name) + 3;
                } else {
                    has_timestamp = 0;
                }
            }
        }

        if (has_timestamp && cut_pos > 0 && strlen(entry->d_name) > cut_pos) {
            char old_path[MAX_PATH_LENGTH];
            char new_path[MAX_PATH_LENGTH];
            
            int old_ret = snprintf(old_path, sizeof(old_path), "%s/%s", dir_path, entry->d_name);
            int new_ret = snprintf(new_path, sizeof(new_path), "%s/%s", dir_path, entry->d_name + cut_pos);
            
            // Check for snprintf truncation
            if (old_ret < 0 || old_ret >= (int)sizeof(old_path) ||
                new_ret < 0 || new_ret >= (int)sizeof(new_path)) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                        "[%s:%d] Path too long, skipping: %s\n", 
                        __FUNCTION__, __LINE__, entry->d_name);
                continue;
            }

            if (rename(old_path, new_path) == 0) {
                RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                        "[%s:%d] Removed timestamp: %s -> %s\n", 
                        __FUNCTION__, __LINE__, entry->d_name, entry->d_name + cut_pos);
                success_count++;
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                        "[%s:%d] Failed to rename %s: %s\n", 
                        __FUNCTION__, __LINE__, old_path, strerror(errno));
                error_count++;
            }
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Timestamp removed from %d files, %d errors\n", 
            __FUNCTION__, __LINE__, success_count, error_count);

    return (error_count > 0) ? -1 : 0;
}

/**
 * @brief Add timestamp prefix to files with UploadLogsNow-specific exclusions
 * @param dir_path Directory containing files to rename
 * @return 0 on success, -1 on failure
 * 
 * This function implements the same logic as the shell script's modifyFileWithTimestamp()
 * function, including exclusions for files that already have timestamps or special log types.
 */
int add_timestamp_to_files_uploadlogsnow(const char* dir_path)
{
    if (!dir_path || !dir_exists(dir_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid or non-existent directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path ? dir_path : "NULL");
        return -1;
    }

    // Get current timestamp in script format: MM-DD-YY-HH-MMAM/PM-
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p-", tm_info);
    
    // Store timestamp prefix globally for removal later (matches script behavior)
    strncpy(g_timestamp_prefix, timestamp, sizeof(g_timestamp_prefix) - 1);

    DIR* dir = opendir(dir_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path);
        return -1;
    }

    int success_count = 0;
    int error_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and special entries
        if (entry->d_name[0] == '.' || 
            strcmp(entry->d_name, "..") == 0 ||
            strncmp(entry->d_name, timestamp, strlen(timestamp)) == 0) {
            continue;
        }

        // Check conditions that should skip timestamp modification (matches shell script logic)
        int should_skip = 0;
        const char* filename = entry->d_name;
        size_t filename_len = strlen(filename);
        
        // Check for existing AM/PM timestamp pattern: .*-[0-9][0-9][AP]M-.* (combined check)
        if (filename_len > 6) {
            for (size_t i = 0; i < filename_len - 6; i++) {
                if (filename[i] == '-' && 
                    isdigit(filename[i+1]) && isdigit(filename[i+2]) && 
                    (filename[i+3] == 'A' || filename[i+3] == 'P') && 
                    filename[i+4] == 'M' && filename[i+5] == '-') {
                    should_skip = 1;
                    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                            "[%s:%d] Processing file...%s\n", 
                            __FUNCTION__, __LINE__, filename);
                    break;
                }
            }
        }
        
        // Check for reboot log pattern: reboot.log
        if (!should_skip && strcmp(filename, "reboot.log") == 0) {
            should_skip = 1;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Processing file...%s\n", 
                    __FUNCTION__, __LINE__, filename);
        }
        
        // Check for abl reason log pattern: ABLReason.txt
        if (!should_skip && strcmp(filename, "ABLReason.txt") == 0) {
            should_skip = 1;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Processing file...%s\n", 
                    __FUNCTION__, __LINE__, filename);
        }
        
        if (should_skip) {
            continue;
        }

        char old_path[MAX_PATH_LENGTH];
        char new_path[MAX_PATH_LENGTH];
        
        int old_ret = snprintf(old_path, sizeof(old_path), "%s/%s", dir_path, entry->d_name);
        int new_ret = snprintf(new_path, sizeof(new_path), "%s/%s%s", dir_path, timestamp, entry->d_name);
        
        // Check for snprintf truncation
        if (old_ret < 0 || old_ret >= (int)sizeof(old_path) ||
            new_ret < 0 || new_ret >= (int)sizeof(new_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Path too long, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }

        // Rename directly without pre-check to avoid TOCTOU issue
        if (rename(old_path, new_path) == 0) {
            success_count++;
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to rename %s: %s\n", 
                    __FUNCTION__, __LINE__, old_path, strerror(errno));
            error_count++;
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Timestamp added to %d files, %d errors\n", 
            __FUNCTION__, __LINE__, success_count, error_count);

    return (error_count > 0) ? -1 : 0;
}

/**
 * @brief Move all contents from source directory to destination directory
 * @param src_dir Source directory
 * @param dest_dir Destination directory
 * @return 0 on success, -1 on failure
 */
int move_directory_contents(const char* src_dir, const char* dest_dir)
{
    if (!src_dir || !dest_dir || !dir_exists(src_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters or source directory does not exist\n", 
                __FUNCTION__, __LINE__);
        return -1;
    }

    // Create destination directory if it doesn't exist
    if (!dir_exists(dest_dir)) {
        if (!create_directory(dest_dir)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to create destination directory: %s\n", 
                    __FUNCTION__, __LINE__, dest_dir);
            return -1;
        }
    }

    DIR* dir = opendir(src_dir);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open source directory: %s\n", 
                __FUNCTION__, __LINE__, src_dir);
        return -1;
    }

    int success_count = 0;
    int error_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char src_path[MAX_PATH_LENGTH];
        char dest_path[MAX_PATH_LENGTH];
        
        int src_ret = snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        int dest_ret = snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);
        
        // Check for snprintf truncation
        if (src_ret < 0 || src_ret >= (int)sizeof(src_path) ||
            dest_ret < 0 || dest_ret >= (int)sizeof(dest_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Path too long, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }

        if (rename(src_path, dest_path) == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] Moved: %s -> %s\n", 
                    __FUNCTION__, __LINE__, src_path, dest_path);
            success_count++;
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Failed to move %s: %s\n", 
                    __FUNCTION__, __LINE__, src_path, strerror(errno));
            error_count++;
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Moved %d items, %d errors\n", 
            __FUNCTION__, __LINE__, success_count, error_count);

    return (error_count > 0) ? -1 : 0;
}

/**
 * @brief Clean directory by removing all its contents
 * @param dir_path Directory to clean
 * @return 0 on success, -1 on failure
 */
int clean_directory(const char* dir_path)
{
    if (!dir_path || !dir_exists(dir_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid or non-existent directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path ? dir_path : "NULL");
        return -1;
    }

    // Use emptyFolder from common_utilities
    if (emptyFolder((char*)dir_path) != RDK_API_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to clean directory: %s\n", 
                __FUNCTION__, __LINE__, dir_path);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Directory cleaned: %s\n", 
            __FUNCTION__, __LINE__, dir_path);

    return 0;
}

/**
 * @brief Clear old packet capture files from log directory
 * @param log_path Log directory path
 * @return 0 on success, -1 on failure
 */
int clear_old_packet_captures(const char* log_path)
{
    if (!log_path || !dir_exists(log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid or non-existent directory: %s\n", 
                __FUNCTION__, __LINE__, log_path ? log_path : "NULL");
        return -1;
    }

    DIR* dir = opendir(log_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, log_path);
        return -1;
    }

    int removed_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Look for .pcap files
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".pcap") == 0) {
            char file_path[MAX_PATH_LENGTH];
            int path_ret = snprintf(file_path, sizeof(file_path), "%s/%s", log_path, entry->d_name);
            
            // Check for snprintf truncation
            if (path_ret < 0 || path_ret >= (int)sizeof(file_path)) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                        "[%s:%d] Path too long, skipping: %s\n", 
                        __FUNCTION__, __LINE__, entry->d_name);
                continue;
            }

            if (remove_file(file_path)) {
                RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                        "[%s:%d] Removed PCAP file: %s\n", 
                        __FUNCTION__, __LINE__, entry->d_name);
                removed_count++;
            } else {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                        "[%s:%d] Failed to remove PCAP file: %s\n", 
                        __FUNCTION__, __LINE__, file_path);
            }
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Removed %d PCAP files from %s\n", 
            __FUNCTION__, __LINE__, removed_count, log_path);

    return 0;
}

/**
 * @brief Remove old directories matching pattern and older than specified days
 * @param base_path Base directory to search in
 * @param pattern Directory name pattern to match
 * @param days_old Minimum age in days for removal
 * @return 0 on success, -1 on failure
 */
int remove_old_directories(const char* base_path, const char* pattern, int days_old)
{
    if (!base_path || !pattern || days_old < 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dir_exists(base_path)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] Base directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, base_path);
        return 0; // Not an error if base doesn't exist
    }

    time_t now = time(NULL);
    time_t cutoff_time = now - (days_old * 24 * 60 * 60);

    DIR* dir = opendir(base_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, base_path);
        return -1;
    }

    int removed_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if name matches pattern (simple substring match)
        if (strstr(entry->d_name, pattern) != NULL) {
            char dir_path[MAX_PATH_LENGTH];
            int path_ret = snprintf(dir_path, sizeof(dir_path), "%s/%s", base_path, entry->d_name);
            
            // Check for snprintf truncation
            if (path_ret < 0 || path_ret >= (int)sizeof(dir_path)) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                        "[%s:%d] Path too long, skipping: %s\n", 
                        __FUNCTION__, __LINE__, entry->d_name);
                continue;
            }

            struct stat st;
            if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                // Check if directory is old enough
                if (st.st_mtime < cutoff_time) {
                    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                            "[%s:%d] Removing old directory: %s (age: %ld days)\n", 
                            __FUNCTION__, __LINE__, entry->d_name, 
                            (now - st.st_mtime) / (24 * 60 * 60));

                    if (remove_directory(dir_path)) {
                        removed_count++;
                    } else {
                        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                                "[%s:%d] Failed to remove directory: %s\n", 
                                __FUNCTION__, __LINE__, dir_path);
                    }
                }
            }
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Removed %d old directories matching pattern '%s'\n", 
            __FUNCTION__, __LINE__, removed_count, pattern);

    return 0;
}
