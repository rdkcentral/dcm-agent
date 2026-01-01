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
 * @brief Archive management and log collection implementation
 * 
 * Combines archive_manager and log_collector functionality:
 * - Log file collection and filtering
 * - TAR.GZ archive creation
 * - Archive naming and timestamp management
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
#include <zlib.h>
#include "archive_manager.h"
#include "file_operations.h"
#ifndef GTEST_ENABLE
#include "system_utils.h"
#endif
#include "strategy_handler.h"
#include "rdk_debug.h"

/* ==========================
   Log Collection Functions
   ========================== */

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

    if (strlen(ctx->log_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] LOG_PATH is not set\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Collect *.txt* and *.log* files from LOG_PATH
    int count = collect_files_from_dir(ctx->log_path, dest_dir, should_collect_file);
    
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

int collect_pcap_logs(const RuntimeContext* ctx, const char* dest_dir)
{
    if (!ctx || !dest_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!ctx->include_pcap) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] PCAP collection not enabled\n", __FUNCTION__, __LINE__);
        return 0;
    }

    // Shell script behavior: Only collect LAST (most recent) pcap file if device is mediaclient
    // Script: lastPcapCapture=`ls -lst $LOG_PATH/*.pcap | head -n 1`
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collecting most recent PCAP file from: %s\n", 
            __FUNCTION__, __LINE__, ctx->log_path);

    DIR* dir = opendir(ctx->log_path);
    if (!dir) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to open LOG_PATH: %s\n", 
                __FUNCTION__, __LINE__, ctx->log_path);
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
        int ret = snprintf(full_path, sizeof(full_path), "%s/%s", ctx->log_path, entry->d_name);
        
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

    if (!ctx->include_dri) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DRI log collection not enabled\n", __FUNCTION__, __LINE__);
        return 0;
    }

    if (strlen(ctx->dri_log_path) == 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] DRI log path not configured\n", __FUNCTION__, __LINE__);
        return 0;
    }

    if (!dir_exists(ctx->dri_log_path)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] DRI log directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->dri_log_path);
        return 0;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collecting DRI logs from: %s\n", 
            __FUNCTION__, __LINE__, ctx->dri_log_path);

    // Collect all files from DRI log directory (no filter)
    int count = collect_files_from_dir(ctx->dri_log_path, dest_dir, NULL);

    if (count > 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected %d DRI log files\n", 
                __FUNCTION__, __LINE__, count);
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] No DRI log files found\n", __FUNCTION__, __LINE__);
    }

    return count;
}

/* ==========================
   Archive Creation Functions
   ========================== */

/* TAR header structure (POSIX ustar format) */
struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

#define TAR_BLOCK_SIZE 512

/* Forward declarations */
static int create_archive_with_options(RuntimeContext* ctx, SessionState* session, 
                                       const char* source_dir, const char* output_dir,
                                       const char* prefix);

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
bool generate_archive_name(char* buffer, size_t buffer_size, 
                          const char* mac_address, const char* prefix)
{
    if (!buffer || !prefix || buffer_size < 64) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid parameters: buffer=%p, prefix=%p, buffer_size=%zu\n",
                __FUNCTION__, __LINE__, (void*)buffer, (void*)prefix, buffer_size);
        return false;
    }

    if (!mac_address || strlen(mac_address) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] MAC address is NULL or empty\n", __FUNCTION__, __LINE__);
        return false;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    if (!tm_info) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to get local time\n", __FUNCTION__, __LINE__);
        return false;
    }

    char timestamp[32];
    // Format: MM-DD-YY-HH-MMAM/PM (matches script: date "+%m-%d-%y-%I-%M%p")
    strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M%p", tm_info);
    
    // Remove colons from MAC address for filename (A8:4A:63 -> A84A63)
    char mac_clean[32];
    const char* src = mac_address;
    char* dst = mac_clean;
    while (*src && (dst - mac_clean) < sizeof(mac_clean) - 1) {
        if (*src != ':') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    // Format: <MAC>_<prefix>_<timestamp>.tgz (matches script format)
    snprintf(buffer, buffer_size, "%s_%s_%s.tgz", mac_clean, prefix, timestamp);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Generated archive name: %s (MAC=%s, prefix=%s)\n",
            __FUNCTION__, __LINE__, buffer, mac_address, prefix);
    
    return true;
}

/**
 * @brief Calculate TAR checksum
 */
static unsigned int calculate_tar_checksum(struct tar_header* header)
{
    unsigned int sum = 0;
    unsigned char* ptr = (unsigned char*)header;
    
    // Initialize checksum field with spaces
    memset(header->checksum, ' ', 8);
    
    // Calculate checksum
    for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
        sum += ptr[i];
    }
    
    return sum;
}

/**
 * @brief Write TAR header for a file
 */
static int write_tar_header(gzFile gz, const char* filename, struct stat* st)
{
    struct tar_header header;
    memset(&header, 0, sizeof(header));
    
    // Filename (strip leading path for archive)
    strncpy(header.name, filename, sizeof(header.name) - 1);
    
    // File mode
    snprintf(header.mode, sizeof(header.mode), "%07o", (unsigned int)st->st_mode & 0777);
    
    // UID and GID
    snprintf(header.uid, sizeof(header.uid), "%07o", 0);
    snprintf(header.gid, sizeof(header.gid), "%07o", 0);
    
    // File size
    snprintf(header.size, sizeof(header.size), "%011lo", (unsigned long)st->st_size);
    
    // Modification time
    snprintf(header.mtime, sizeof(header.mtime), "%011lo", (unsigned long)st->st_mtime);
    
    // Type flag (regular file)
    header.typeflag = '0';
    
    // Magic and version (ustar)
    memcpy(header.magic, "ustar", 5);
    header.magic[5] = '\0';
    memcpy(header.version, "00", 2);
    
    // Calculate and write checksum
    unsigned int checksum = calculate_tar_checksum(&header);
    snprintf(header.checksum, sizeof(header.checksum), "%06o", checksum);
    
    // Write header to gzip file
    if (gzwrite(gz, &header, sizeof(header)) != sizeof(header)) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Add file content to TAR archive
 */
static int add_file_to_tar(gzFile gz, const char* filepath, const char* arcname)
{
    struct stat st;
    
    // Open file first with O_NOFOLLOW to prevent symlink attacks (TOCTOU fix)
    int fd = open(filepath, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) {
        if (errno != ELOOP) {  // ELOOP = symlink detected
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Failed to open file: %s (errno=%d)\n", 
                    __FUNCTION__, __LINE__, filepath, errno);
        }
        return -1;
    }
    
    // Use fstat on the open file descriptor to avoid TOCTOU race condition
    if (fstat(fd, &st) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to fstat file: %s\n", __FUNCTION__, __LINE__, filepath);
        close(fd);
        return -1;
    }
    
    // Skip non-regular files
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return 0;
    }
    
    // Write TAR header
    if (write_tar_header(gz, arcname, &st) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to write TAR header\n", __FUNCTION__, __LINE__);
        close(fd);
        return -1;
    }
    
    // Convert file descriptor to FILE* for reading
    FILE* fp = fdopen(fd, "rb");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to fdopen file: %s\n", __FUNCTION__, __LINE__, filepath);
        close(fd);
        return -1;
    }
    
    char buffer[8192];
    size_t bytes_read;
    size_t total_written = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (gzwrite(gz, buffer, bytes_read) != (int)bytes_read) {
            fclose(fp);
            return -1;
        }
        total_written += bytes_read;
    }
    
    fclose(fp);
    
    // Pad to 512-byte boundary
    size_t padding = (TAR_BLOCK_SIZE - (total_written % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
    if (padding > 0) {
        char pad[TAR_BLOCK_SIZE] = {0};
        if (gzwrite(gz, pad, padding) != (int)padding) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief Recursively add directory to TAR archive
 */
static int add_directory_to_tar(gzFile gz, const char* dirpath, const char* base_path, const char* exclude_file)
{
    DIR* dir = opendir(dirpath);
    if (!dir) {
        return -1;
    }
    
    struct dirent* entry;
    int base_len = strlen(base_path);
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char fullpath[MAX_PATH_LENGTH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        
        // Skip excluded file
        if (exclude_file && strcmp(fullpath, exclude_file) == 0) {
            continue;
        }
        
        struct stat st;
        if (stat(fullpath, &st) != 0) {
            continue;
        }
        
        // Calculate archive path (relative path)
        const char* arcname = fullpath + base_len;
        if (arcname[0] == '/') {
            arcname++;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Recursively process subdirectory
            if (add_directory_to_tar(gz, fullpath, base_path, exclude_file) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Add file
            if (add_file_to_tar(gz, fullpath, arcname) != 0) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB,
                        "[%s:%d] Failed to add file: %s\n", __FUNCTION__, __LINE__, fullpath);
            }
        }
    }
    
    closedir(dir);
    return 0;
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
 * @brief Create tar.gz archive from directory using zlib
 * @param ctx Runtime context
 * @param session Session state (optional, can be NULL for DRI archives)
 * @param source_dir Source directory to archive
 * @param output_dir Output directory for archive (NULL = use source_dir)
 * @param prefix Archive name prefix ("Logs" or "DRI_Logs")
 * @return 0 on success, -1 on failure
 */
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir)
{
    if (!ctx || !session || !source_dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }
    return create_archive_with_options(ctx, session, source_dir, NULL, "Logs");
}

/**
 * @brief Create archive with custom options
 */
static int create_archive_with_options(RuntimeContext* ctx, SessionState* session, 
                                       const char* source_dir, const char* output_dir,
                                       const char* prefix)
{
    if (!ctx || !session || !source_dir || !prefix) {
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
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Creating archive with MAC='%s', prefix='%s'\n",
            __FUNCTION__, __LINE__, 
            ctx->device.mac_address ? ctx->device.mac_address : "(NULL)", 
            prefix);
    
    char archive_filename[MAX_FILENAME_LENGTH];
    if (!generate_archive_name(archive_filename, sizeof(archive_filename), 
                               ctx->device.mac_address, prefix)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to generate archive filename\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Determine output directory
    const char* target_dir = output_dir ? output_dir : source_dir;
    
    // Archive path
    char archive_path[MAX_PATH_LENGTH];
    snprintf(archive_path, sizeof(archive_path), "%s/%s", target_dir, archive_filename);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Creating archive: %s from %s\n", 
            __FUNCTION__, __LINE__, archive_path, source_dir);

    // Create gzip file
    gzFile gz = gzopen(archive_path, "wb9");
    if (!gz) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create gzip file\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Add all files from directory
    int ret = add_directory_to_tar(gz, source_dir, source_dir, archive_path);
    
    // Write two 512-byte blocks of zeros (TAR EOF marker)
    char eof_blocks[TAR_BLOCK_SIZE * 2];
    memset(eof_blocks, 0, sizeof(eof_blocks));
    gzwrite(gz, eof_blocks, sizeof(eof_blocks));
    
    // Close gzip file
    gzclose(gz);

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
                "[%s:%d] Failed to create archive\n", __FUNCTION__, __LINE__);
        return -1;
    }
}

/**
 * @brief Create DRI logs archive
 * @param ctx Runtime context
 * @param archive_path Output archive file path (directory portion used)
 * @return 0 on success, -1 on failure
 */
int create_dri_archive(RuntimeContext* ctx, const char* archive_path)
{
    if (!ctx || !archive_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (strlen(ctx->dri_log_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] DRI log path not configured\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (!dir_exists(ctx->dri_log_path)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] DRI log directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->dri_log_path);
        return -1;
    }

    // Extract output directory from archive_path
    char output_dir[MAX_PATH_LENGTH];
    const char* last_slash = strrchr(archive_path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - archive_path;
        snprintf(output_dir, sizeof(output_dir), "%.*s", (int)dir_len, archive_path);
    } else {
        strcpy(output_dir, "/tmp");
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Creating DRI archive from %s to %s\n", 
            __FUNCTION__, __LINE__, ctx->dri_log_path, output_dir);

    // Use the common archive creation with DRI_Logs prefix
    return create_archive_with_options(ctx, NULL, ctx->dri_log_path, output_dir, "DRI_Logs");
}
