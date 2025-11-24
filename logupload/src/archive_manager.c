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
#include <zlib.h>
#include "archive_manager.h"
#include "log_collector.h"
#include "file_operations.h"
#include "system_utils.h"
#include "rdk_debug.h"

/**
 * @brief Generate timestamp string for archive naming
 * @param buffer Buffer to store timestamp
 * @param buffer_size Size of buffer
 * @return true on success, false on failure
 */
static bool generate_timestamp(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 20) {
        return false;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    if (!tm_info) {
        return false;
    }

    // Format: MM-DD-YY-HH-MM[AM|PM]
    strftime(buffer, buffer_size, "%m-%d-%y-%I-%M%p", tm_info);
    
    return true;
}

/**
 * @brief Generate archive filename with MAC and timestamp
 * @param ctx Runtime context
 * @param filename_buf Buffer to store filename
 * @param buf_size Size of buffer
 * @return true on success, false on failure
 */
static bool generate_archive_filename(const RuntimeContext* ctx, char* filename_buf, size_t buf_size)
{
    if (!ctx || !filename_buf || buf_size == 0) {
        return false;
    }

    char timestamp[32];
    if (!generate_timestamp(timestamp, sizeof(timestamp))) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to generate timestamp\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    // Remove colons from MAC address for filename
    // Convert 02:42:AC:12:00:02 to 0242AC120002
    char mac_no_colons[MAX_MAC_LENGTH];
    int j = 0;
    for (int i = 0; ctx->device.mac_address[i] != '\0' && j < (int)(sizeof(mac_no_colons) - 1); i++) {
        if (ctx->device.mac_address[i] != ':') {
            mac_no_colons[j++] = ctx->device.mac_address[i];
        }
    }
    mac_no_colons[j] = '\0';

    // Format: MAC_Logs_MM-DD-YY-HH-MMAM.tgz (without colons in MAC)
    snprintf(filename_buf, buf_size, "%s_Logs_%s.tgz", mac_no_colons, timestamp);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Generated archive filename: %s\n", 
            __FUNCTION__, __LINE__, filename_buf);
    
    return true;
}

bool insert_timestamp(const char* log_dir)
{
    if (!log_dir || !dir_exists(log_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid or non-existent log directory\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Inserting timestamp into filenames in: %s\n", 
            __FUNCTION__, __LINE__, log_dir);

    char timestamp[32];
    if (!generate_timestamp(timestamp, sizeof(timestamp))) {
        return false;
    }

    // Add trailing dash to timestamp prefix
    strcat(timestamp, "-");

    DIR* dir = opendir(log_dir);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, log_dir);
        return false;
    }

    struct dirent* entry;
    int renamed_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and special entries
        if (entry->d_type == DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Skip files that already have timestamp prefix (pattern: MM-DD-YY-HH-MMAM-)
        // Check if filename matches timestamp pattern
        if (strstr(entry->d_name, "AM-") || strstr(entry->d_name, "PM-")) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Skipping already timestamped file: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }

        // Construct old and new paths
        char old_path[2048];
        char new_path[2048];
        int ret1 = snprintf(old_path, sizeof(old_path), "%s/%s", log_dir, entry->d_name);
        int ret2 = snprintf(new_path, sizeof(new_path), "%s/%s%s", log_dir, timestamp, entry->d_name);
        
        if (ret1 < 0 || ret1 >= (int)sizeof(old_path) || ret2 < 0 || ret2 >= (int)sizeof(new_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Path too long, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }

        // Rename file with timestamp prefix
        if (rename(old_path, new_path) == 0) {
            renamed_count++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Renamed: %s -> %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name, new_path);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to rename: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Renamed %d files with timestamp prefix\n", 
            __FUNCTION__, __LINE__, renamed_count);

    return true;
}

bool reverse_timestamp(const char* log_dir)
{
    if (!log_dir || !dir_exists(log_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid or non-existent log directory\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Removing timestamp prefix from filenames in: %s\n", 
            __FUNCTION__, __LINE__, log_dir);

    DIR* dir = opendir(log_dir);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open directory: %s\n", 
                __FUNCTION__, __LINE__, log_dir);
        return false;
    }

    struct dirent* entry;
    int renamed_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and special entries
        if (entry->d_type == DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if filename has timestamp prefix (pattern: MM-DD-YY-HH-MM[AM|PM]-)
        // Timestamp format is: 12-25-24-11-30AM- (18 chars)
        if (strlen(entry->d_name) < 18) {
            continue;
        }

        // Look for AM- or PM- pattern indicating timestamp
        const char* am_pos = strstr(entry->d_name, "AM-");
        const char* pm_pos = strstr(entry->d_name, "PM-");
        
        if (!am_pos && !pm_pos) {
            continue;
        }

        // Find the position after the timestamp
        const char* original_name = NULL;
        if (am_pos && (am_pos - entry->d_name) < 20) {
            original_name = am_pos + 3; // Skip "AM-"
        } else if (pm_pos && (pm_pos - entry->d_name) < 20) {
            original_name = pm_pos + 3; // Skip "PM-"
        }

        if (!original_name || strlen(original_name) == 0) {
            continue;
        }

        // Construct old and new paths
        char old_path[2048];
        char new_path[2048];
        int ret1 = snprintf(old_path, sizeof(old_path), "%s/%s", log_dir, entry->d_name);
        int ret2 = snprintf(new_path, sizeof(new_path), "%s/%s", log_dir, original_name);
        
        if (ret1 < 0 || ret1 >= (int)sizeof(old_path) || ret2 < 0 || ret2 >= (int)sizeof(new_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Path too long, skipping: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
            continue;
        }

        // Rename file to remove timestamp prefix
        if (rename(old_path, new_path) == 0) {
            renamed_count++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Restored: %s -> %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name, original_name);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to restore: %s\n", 
                    __FUNCTION__, __LINE__, entry->d_name);
        }
    }

    closedir(dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Restored %d files to original names\n", 
            __FUNCTION__, __LINE__, renamed_count);

    return true;
}

/**
 * @brief Write tar header for a file
 * @param fp File pointer
 * @param filename Filename in archive
 * @param filesize File size in bytes
 * @return true on success, false on failure
 */
static bool write_tar_header(FILE* fp, const char* filename, long filesize)
{
    char header[512];
    memset(header, 0, sizeof(header));

    // Basic tar header format (POSIX ustar)
    // File name (100 bytes)
    strncpy(header, filename, 100);
    
    // File mode (8 bytes) - 0644 in octal
    snprintf(header + 100, 8, "0000644");
    
    // Owner ID (8 bytes)
    snprintf(header + 108, 8, "0000000");
    
    // Group ID (8 bytes)
    snprintf(header + 116, 8, "0000000");
    
    // File size in octal (12 bytes)
    snprintf(header + 124, 12, "%011lo", filesize);
    
    // Modification time in octal (12 bytes)
    snprintf(header + 136, 12, "%011lo", (long)time(NULL));
    
    // Type flag - regular file
    header[156] = '0';
    
    // ustar magic
    strncpy(header + 257, "ustar", 6);
    header[263] = '0';
    header[264] = '0';
    
    // Calculate checksum
    unsigned int checksum = 0;
    // Initially set checksum field to spaces
    memset(header + 148, ' ', 8);
    
    for (int i = 0; i < 512; i++) {
        checksum += (unsigned char)header[i];
    }
    
    // Write checksum in octal
    snprintf(header + 148, 8, "%06o", checksum);
    header[154] = '\0';
    header[155] = ' ';
    
    // Write header
    if (fwrite(header, 1, 512, fp) != 512) {
        return false;
    }
    
    return true;
}

/**
 * @brief Add a file to tar archive
 * @param tar_fp Tar file pointer
 * @param filepath Path to file to add
 * @param arcname Name in archive
 * @return true on success, false on failure
 */
static bool add_file_to_tar(FILE* tar_fp, const char* filepath, const char* arcname)
{
    FILE* input_fp = fopen(filepath, "rb");
    if (!input_fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open file: %s\n", 
                __FUNCTION__, __LINE__, filepath);
        return false;
    }

    // Get file size
    fseek(input_fp, 0, SEEK_END);
    long filesize = ftell(input_fp);
    fseek(input_fp, 0, SEEK_SET);

    // Write tar header
    if (!write_tar_header(tar_fp, arcname, filesize)) {
        fclose(input_fp);
        return false;
    }

    // Write file content
    char buffer[512];
    size_t bytes_read;
    long total_written = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input_fp)) > 0) {
        if (fwrite(buffer, 1, bytes_read, tar_fp) != bytes_read) {
            fclose(input_fp);
            return false;
        }
        total_written += bytes_read;
    }

    fclose(input_fp);

    // Pad to 512-byte boundary
    long padding = (512 - (filesize % 512)) % 512;
    if (padding > 0) {
        char pad[512] = {0};
        if (fwrite(pad, 1, padding, tar_fp) != (size_t)padding) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Recursively add directory contents to tar
 * @param tar_fp Tar file pointer
 * @param dir_path Directory path
 * @param base_path Base path for relative names
 * @return true on success, false on failure
 */
static bool add_directory_to_tar(FILE* tar_fp, const char* dir_path, const char* base_path)
{
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[2048];
        int ret = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        if (ret < 0 || ret >= (int)sizeof(full_path)) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Path too long, skipping: %s/%s\n", 
                    __FUNCTION__, __LINE__, dir_path, entry->d_name);
            continue;
        }

        // Calculate relative path for archive
        char arc_name[1024];
        const char* rel_start = full_path + strlen(base_path);
        if (*rel_start == '/') {
            rel_start++;
        }
        strncpy(arc_name, rel_start, sizeof(arc_name) - 1);
        arc_name[sizeof(arc_name) - 1] = '\0';

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively add subdirectory
            if (!add_directory_to_tar(tar_fp, full_path, base_path)) {
                closedir(dir);
                return false;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Add regular file
            if (!add_file_to_tar(tar_fp, full_path, arc_name)) {
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to add file: %s\n", 
                        __FUNCTION__, __LINE__, full_path);
            }
        }
    }

    closedir(dir);
    return true;
}

/**
 * @brief Create uncompressed tar archive
 * @param source_dir Source directory
 * @param tar_path Output tar path
 * @return true on success, false on failure
 */
static bool create_tar(const char* source_dir, const char* tar_path)
{
    FILE* tar_fp = fopen(tar_path, "wb");
    if (!tar_fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to create tar file: %s\n", 
                __FUNCTION__, __LINE__, tar_path);
        return false;
    }

    bool success = add_directory_to_tar(tar_fp, source_dir, source_dir);

    if (success) {
        // Write two 512-byte blocks of zeros as EOF marker
        char eof[1024] = {0};
        fwrite(eof, 1, 1024, tar_fp);
    }

    fclose(tar_fp);
    return success;
}

/**
 * @brief Compress tar file with gzip
 * @param tar_path Input tar file
 * @param gz_path Output gzip file
 * @return true on success, false on failure
 */
static bool gzip_file(const char* tar_path, const char* gz_path)
{
    FILE* input = fopen(tar_path, "rb");
    if (!input) {
        return false;
    }

    gzFile output = gzopen(gz_path, "wb9"); // compression level 9
    if (!output) {
        fclose(input);
        return false;
    }

    char buffer[8192];
    size_t bytes_read;
    bool success = true;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (gzwrite(output, buffer, bytes_read) != (int)bytes_read) {
            success = false;
            break;
        }
    }

    fclose(input);
    gzclose(output);

    return success;
}

bool create_tarball(const char* source_dir, const char* archive_path)
{
    if (!source_dir || !archive_path) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    if (!dir_exists(source_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Source directory does not exist: %s\n", 
                __FUNCTION__, __LINE__, source_dir);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Creating tarball: %s from %s\n", 
            __FUNCTION__, __LINE__, archive_path, source_dir);

    // Create temporary tar file (uncompressed) with larger buffer
    char temp_tar[2048];
    int ret = snprintf(temp_tar, sizeof(temp_tar), "%s.tmp.tar", archive_path);
    
    if (ret < 0 || ret >= (int)sizeof(temp_tar)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Temp tar path too long\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    // Create tar archive
    if (!create_tar(source_dir, temp_tar)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to create tar archive\n", 
                __FUNCTION__, __LINE__);
        remove_file(temp_tar);
        return false;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Tar created, compressing with gzip\n", 
            __FUNCTION__, __LINE__);

    // Compress with gzip
    if (!gzip_file(temp_tar, archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to compress tar file\n", 
                __FUNCTION__, __LINE__);
        remove_file(temp_tar);
        return false;
    }

    // Remove temporary tar file
    remove_file(temp_tar);

    // Verify archive was created
    if (!file_exists(archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Archive file was not created: %s\n", 
                __FUNCTION__, __LINE__, archive_path);
        return false;
    }

    long size = get_archive_size(archive_path);
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Archive created successfully, size: %ld bytes\n", 
            __FUNCTION__, __LINE__, size);

    return true;
}

long get_archive_size(const char* archive_path)
{
    if (!archive_path) {
        return -1;
    }

    return get_file_size(archive_path);
}

bool prepare_archive(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Preparing archive for strategy: %d\n", 
            __FUNCTION__, __LINE__, session->strategy);

    // Create temporary collection directory with larger buffer
    char temp_collection_dir[1024];
    int ret = snprintf(temp_collection_dir, sizeof(temp_collection_dir), "%s/logcollection_%ld", 
                       ctx->paths.temp_dir, (long)time(NULL));
    
    if (ret < 0 || ret >= (int)sizeof(temp_collection_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Temp directory path too long\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    if (!create_directory(temp_collection_dir)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to create temp collection directory\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Created temp collection directory: %s\n", 
            __FUNCTION__, __LINE__, temp_collection_dir);

    // Collect logs into temporary directory
    int file_count = collect_logs(ctx, session, temp_collection_dir);
    if (file_count <= 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] No log files collected\n", __FUNCTION__, __LINE__);
        remove_directory(temp_collection_dir);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Collected %d log files\n", 
            __FUNCTION__, __LINE__, file_count);

    // Insert timestamp for non-OnDemand/Privacy strategies
    if (session->strategy != STRAT_ONDEMAND && session->strategy != STRAT_PRIVACY_ABORT) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Inserting timestamp into filenames\n", 
                __FUNCTION__, __LINE__);
        insert_timestamp(temp_collection_dir);
    }

    // Generate archive filename
    char archive_filename[256];
    if (!generate_archive_filename(ctx, archive_filename, sizeof(archive_filename))) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to generate archive filename\n", 
                __FUNCTION__, __LINE__);
        remove_directory(temp_collection_dir);
        return false;
    }

    // Construct full archive path with larger buffer
    char archive_path[2048];
    ret = snprintf(archive_path, sizeof(archive_path), "%s/%s", ctx->paths.temp_dir, archive_filename);
    
    if (ret < 0 || ret >= (int)sizeof(archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Archive path too long\n", 
                __FUNCTION__, __LINE__);
        remove_directory(temp_collection_dir);
        return false;
    }

    // Create tarball
    if (!create_tarball(temp_collection_dir, archive_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to create tarball\n", 
                __FUNCTION__, __LINE__);
        remove_directory(temp_collection_dir);
        return false;
    }

    // Store archive filename in session state
    strncpy(session->archive_file, archive_path, sizeof(session->archive_file) - 1);
    session->archive_file[sizeof(session->archive_file) - 1] = '\0';

    // Clean up temporary collection directory
    remove_directory(temp_collection_dir);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Archive prepared successfully: %s\n", 
            __FUNCTION__, __LINE__, archive_path);

    return true;
}

bool prepare_rrd_archive(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Preparing RRD archive\n", __FUNCTION__, __LINE__);

    // For RRD strategy, the tar file is already provided as rrd_file
    // No archiving needed - just verify the file exists and use it directly
    if (!file_exists(ctx->paths.rrd_file)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] RRD log file does not exist: %s\n", 
                __FUNCTION__, __LINE__, ctx->paths.rrd_file);
        return false;
    }

    // Store the RRD file path directly in session state
    // The file is already in tar.gz format and ready for upload
    strncpy(session->archive_file, ctx->paths.rrd_file, sizeof(session->archive_file) - 1);
    session->archive_file[sizeof(session->archive_file) - 1] = '\0';

    long size = get_archive_size(ctx->paths.rrd_file);
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] RRD archive ready for upload: %s (size: %ld bytes)\n", 
            __FUNCTION__, __LINE__, ctx->paths.rrd_file, size);

    return true;
}
