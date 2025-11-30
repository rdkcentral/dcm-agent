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
 * @file log_collector.c
 * @brief Log collection implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "log_collector.h"
#include "file_operations.h"
#include "system_utils.h"
#include "rdk_debug.h"

/**
 * @brief Check if filename has a valid log extension
 * @param filename File name to check
 * @return true if file should be collected
 */
bool should_collect_file(const char* filename)
{
    if (!filename || filename[0] == '\0') {
        return false;
    }

    // Skip . and .. directories
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }

    // Collect files with .log or .txt extensions (including rotated logs like .log.0, .txt.1)
    // Shell script uses: *.txt* and *.log* patterns
    if (strstr(filename, ".log") != NULL || strstr(filename, ".txt") != NULL) {
        return true;
    }

    return false;
}

/**
 * @brief Copy a single file to destination directory
 * @param src_path Source file path
 * @param dest_dir Destination directory
 * @return true on success, false on failure
 */
static bool copy_log_file(const char* src_path, const char* dest_dir)
{
    if (!src_path || !dest_dir) {
        return false;
    }

    // Extract filename from source path
    const char* filename = strrchr(src_path, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = src_path;
    }

    // Construct destination path with larger buffer to avoid truncation
    char dest_path[2048];
    int ret = snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, filename);
    
    if (ret < 0 || ret >= (int)sizeof(dest_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Destination path too long: %s/%s\n", 
                __FUNCTION__, __LINE__, dest_dir, filename);
        return false;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Copying %s to %s\n", 
            __FUNCTION__, __LINE__, src_path, dest_path);

    return copy_file(src_path, dest_path);
}

/**
 * @brief Collect files from a directory matching filter
 * @param src_dir Source directory
 * @param dest_dir Destination directory
 * @param filter_func Filter function (NULL = collect all)
 * @return Number of files collected, or -1 on error
 */
static int collect_files_from_dir(const char* src_dir, const char* dest_dir, 
                                   bool (*filter_func)(const char*))
{
    if (!src_dir || !dest_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dir_exists(src_dir)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Source directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, src_dir);
        return 0;
    }

    DIR* dir = opendir(src_dir);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, src_dir);
        return -1;
    }

    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip directories
        if (entry->d_type == DT_DIR) {
            continue;
        }

        // Apply filter if provided
        if (filter_func && !filter_func(entry->d_name)) {
            continue;
        }

        // Construct full source path with larger buffer
        char src_path[2048];
        int ret = snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        
        if (ret < 0 || ret >= (int)sizeof(src_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Source path too long, skipping: %s/%s\n", 
                    __FUNCTION__, __LINE__, src_dir, entry->d_name);
            continue;
        }

        // Copy file to destination
        if (copy_log_file(src_path, dest_dir)) {
            count++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Collected: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to copy: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected %d files from %s\n", 
            __FUNCTION__, __LINE__, count, src_dir);

    return count;
}

int collect_logs(const RuntimeContext* ctx, const SessionState* session, const char* dest_dir)
{
    if (!ctx || !session || !dest_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // This function is used ONLY by ONDEMAND strategy to copy files from LOG_PATH to temp directory
    // Other strategies (REBOOT/DCM) work directly in their source directories and don't call this
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collecting log files from LOG_PATH to: %s\n", 
            __FUNCTION__, __LINE__, dest_dir);

    if (strlen(ctx->paths.log_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] LOG_PATH is not set\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Collect *.txt* and *.log* files from LOG_PATH
    int count = collect_files_from_dir(ctx->paths.log_path, dest_dir, should_collect_file);
    
    if (count <= 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] No log files collected\n", __FUNCTION__, __LINE__);
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected %d log files\n", 
                __FUNCTION__, __LINE__, count);
    }

    return count;
}

int collect_previous_logs(const char* src_dir, const char* dest_dir)
{
    if (!src_dir || !dest_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dir_exists(src_dir)) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Previous logs directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, src_dir);
        return 0;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collecting previous logs from: %s\n", 
            __FUNCTION__, __LINE__, src_dir);

    // Collect .log and .txt files from previous logs directory
    int count = collect_files_from_dir(src_dir, dest_dir, should_collect_file);

    if (count > 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected %d previous log files\n", 
                __FUNCTION__, __LINE__, count);
    }

    return count;
}

/**
 * @brief Check if file is a PCAP file
 * @param filename File name to check
 * @return true if PCAP file
 */
static bool is_pcap_file(const char* filename)
{
    if (!filename) {
        return false;
    }

    // Skip . and ..
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }

    // Check for .pcap extension
    const char* ext = strrchr(filename, '.');
    if (ext && strcasecmp(ext, ".pcap") == 0) {
        return true;
    }

    // Also check for .pcap.* (backup files)
    if (strstr(filename, ".pcap") != NULL) {
        return true;
    }

    return false;
}

int collect_pcap_logs(const RuntimeContext* ctx, const char* dest_dir)
{
    if (!ctx || !dest_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!ctx->settings.include_pcap) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] PCAP collection not enabled\n", __FUNCTION__, __LINE__);
        return 0;
    }

    // Shell script behavior: Only collect LAST (most recent) pcap file if device is mediaclient
    // Script: lastPcapCapture=`ls -lst $LOG_PATH/*.pcap | head -n 1`
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collecting most recent PCAP file from: %s\n", 
            __FUNCTION__, __LINE__, ctx->paths.log_path);

    DIR* dir = opendir(ctx->paths.log_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to open LOG_PATH: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.log_path);
        return 0;
    }

    struct dirent* entry;
    time_t newest_time = 0;
    char newest_pcap[1024] = {0};
    
    // Find the most recent .pcap file (specifically looking for -moca.pcap pattern)
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            continue;
        }

        // Check for .pcap extension
        if (!strstr(entry->d_name, ".pcap")) {
            continue;
        }

        char full_path[2048];
        int ret = snprintf(full_path, sizeof(full_path), "%s/%s", ctx->paths.log_path, entry->d_name);
        
        if (ret < 0 || ret >= (int)sizeof(full_path)) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (st.st_mtime > newest_time) {
                newest_time = st.st_mtime;
                strncpy(newest_pcap, full_path, sizeof(newest_pcap) - 1);
                newest_pcap[sizeof(newest_pcap) - 1] = '\0';
            }
        }
    }

    closedir(dir);

    // Copy the most recent PCAP file if found
    if (newest_time > 0 && strlen(newest_pcap) > 0) {
        if (copy_log_file(newest_pcap, dest_dir)) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected most recent PCAP file: %s\n", 
                    __FUNCTION__, __LINE__, newest_pcap);
            return 1;
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to copy PCAP file: %s\n", 
                    __FUNCTION__, __LINE__, newest_pcap);
        }
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] No PCAP files found\n", __FUNCTION__, __LINE__);
    }

    return 0;
}

int collect_dri_logs(const RuntimeContext* ctx, const char* dest_dir)
{
    if (!ctx || !dest_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!ctx->settings.include_dri) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DRI log collection not enabled\n", __FUNCTION__, __LINE__);
        return 0;
    }

    if (strlen(ctx->paths.dri_log_path) == 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] DRI log path not configured\n", __FUNCTION__, __LINE__);
        return 0;
    }

    if (!dir_exists(ctx->paths.dri_log_path)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] DRI log directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.dri_log_path);
        return 0;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collecting DRI logs from: %s\n", 
            __FUNCTION__, __LINE__, ctx->paths.dri_log_path);

    // Collect all files from DRI log directory (no filter)
    int count = collect_files_from_dir(ctx->paths.dri_log_path, dest_dir, NULL);

    if (count > 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected %d DRI log files\n", 
                __FUNCTION__, __LINE__, count);
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] No DRI log files found\n", __FUNCTION__, __LINE__);
    }

    return count;
}
