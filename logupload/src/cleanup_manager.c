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
 * @file cleanup_manager.c
 * @brief Log cleanup and housekeeping implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <regex.h>
#include "cleanup_manager.h"
#include "uploadstblogs_types.h"
#include "rdk_debug.h"

/**
 * @brief Recursively remove directory and contents
 */
static int remove_directory_recursive(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
        return remove(path);
    }
    
    struct dirent *entry;
    char filepath[512];
    int result = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                result = remove_directory_recursive(filepath);
            } else {
                result = remove(filepath);
            }
            
            if (result != 0) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                        "[%s:%d] Failed to remove: %s\n", 
                        __FUNCTION__, __LINE__, filepath);
            }
        }
    }
    
    closedir(dir);
    return rmdir(path);
}

bool is_timestamped_backup(const char *filename)
{
    if (!filename) {
        return false;
    }
    
    // Pattern 1: *-*-*-*-*M- (matches: 11-30-25-03-45PM-)
    // Pattern 2: *-*-*-*-*M-logbackup (matches: 11-30-25-03-45PM-logbackup)
    regex_t regex;
    int ret;
    
    // Regex pattern for: digits-digits-digits-digits-digits[AP]M- or [AP]M-logbackup
    const char *pattern = "[0-9]+-[0-9]+-[0-9]+-[0-9]+-[0-9]+[AP]M(-logbackup)?$";
    
    ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to compile regex\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    ret = regexec(&regex, filename, 0, NULL, 0);
    regfree(&regex);
    
    return (ret == 0);
}

int cleanup_old_log_backups(const char *log_path, int max_age_days)
{
    if (!log_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid log path\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    DIR *dir = opendir(log_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, log_path);
        return -1;
    }
    
    time_t now = time(NULL);
    time_t cutoff = now - (max_age_days * 24 * 60 * 60);
    int removed_count = 0;
    
    struct dirent *entry;
    char fullpath[512];
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if matches timestamped backup pattern
        if (!is_timestamped_backup(entry->d_name)) {
            continue;
        }
        
        snprintf(fullpath, sizeof(fullpath), "%s/%s", log_path, entry->d_name);
        
        struct stat st;
        if (stat(fullpath, &st) != 0) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                    "[%s:%d] Failed to stat: %s\n", 
                    __FUNCTION__, __LINE__, fullpath);
            continue;
        }
        
        // Check if older than max_age_days (matches script: -mtime +3)
        if (st.st_mtime < cutoff) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                    "[%s:%d] Removing old backup (age: %d days): %s\n",
                    __FUNCTION__, __LINE__, 
                    (int)((now - st.st_mtime) / (24 * 60 * 60)), fullpath);
            
            if (S_ISDIR(st.st_mode)) {
                if (remove_directory_recursive(fullpath) == 0) {
                    removed_count++;
                }
            } else {
                if (remove(fullpath) == 0) {
                    removed_count++;
                }
            }
        }
    }
    
    closedir(dir);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Cleanup complete: removed %d old backups from %s\n",
            __FUNCTION__, __LINE__, removed_count, log_path);
    
    return removed_count;
}

int cleanup_old_archives(const char *log_path)
{
    if (!log_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid log path\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    DIR *dir = opendir(log_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, log_path);
        return -1;
    }
    
    int removed_count = 0;
    struct dirent *entry;
    char fullpath[512];
    
    while ((entry = readdir(dir)) != NULL) {
        // Check if file ends with .tgz
        size_t len = strlen(entry->d_name);
        if (len < 5 || strcmp(entry->d_name + len - 4, ".tgz") != 0) {
            continue;
        }
        
        snprintf(fullpath, sizeof(fullpath), "%s/%s", log_path, entry->d_name);
        
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
                "[%s:%d] Removing old archive: %s\n",
                __FUNCTION__, __LINE__, fullpath);
        
        if (remove(fullpath) == 0) {
            removed_count++;
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                    "[%s:%d] Failed to remove: %s\n", 
                    __FUNCTION__, __LINE__, fullpath);
        }
    }
    
    closedir(dir);
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB,
            "[%s:%d] Archive cleanup complete: removed %d .tgz files from %s\n",
            __FUNCTION__, __LINE__, removed_count, log_path);
    
    return removed_count;
}
