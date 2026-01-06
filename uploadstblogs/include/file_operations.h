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
 * @file file_operations.h
 * @brief Common file operations utilities
 *
 * This module provides common file system operations used throughout
 * the application.
 */

#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Check if file exists
 * @param filepath Path to file
 * @return true if exists, false otherwise
 */
bool file_exists(const char* filepath);

/**
 * @brief Check if directory exists
 * @param dirpath Path to directory
 * @return true if exists, false otherwise
 */
bool dir_exists(const char* dirpath);

/**
 * @brief Create directory recursively
 * @param dirpath Path to directory
 * @return true on success, false on failure
 */
bool create_directory(const char* dirpath);

/**
 * @brief Remove file
 * @param filepath Path to file
 * @return true on success, false on failure
 */
bool remove_file(const char* filepath);

/**
 * @brief Remove directory recursively
 * @param dirpath Path to directory
 * @return true on success, false on failure
 */
bool remove_directory(const char* dirpath);

/**
 * @brief Copy file
 * @param src Source file path
 * @param dest Destination file path
 * @return true on success, false on failure
 */
bool copy_file(const char* src, const char* dest);

/**
 * @brief Safely join directory path and filename, handling trailing slashes
 * @param buffer Output buffer for joined path
 * @param buffer_size Size of output buffer
 * @param dir Directory path (may have trailing slash)
 * @param filename Filename to append
 * @return true on success, false if path would exceed buffer size
 */
bool join_path(char* buffer, size_t buffer_size, const char* dir, const char* filename);

/**
 * @brief Get file size
 * @param filepath Path to file
 * @return File size in bytes, or -1 on error
 */
long get_file_size(const char* filepath);

/**
 * @brief Check if directory is empty
 * @param dirpath Path to directory
 * @return true if empty, false otherwise
 */
bool is_directory_empty(const char* dirpath);

/**
 * @brief Check if directory has .txt or .log files
 * @param dirpath Path to directory
 * @return true if has .txt or .log files, false otherwise
 */
bool has_log_files(const char* dirpath);

/**
 * @brief Write string to file
 * @param filepath Path to file
 * @param content Content to write
 * @return true on success, false on failure
 */
bool write_file(const char* filepath, const char* content);

/**
 * @brief Read file into buffer
 * @param filepath Path to file
 * @param buffer Output buffer
 * @param buffer_size Size of buffer
 * @return Number of bytes read, or -1 on error
 */
int read_file(const char* filepath, char* buffer, size_t buffer_size);

/**
 * @brief Add timestamp prefix to all files in directory
 * @param dir_path Directory containing files
 * @return 0 on success, -1 on failure
 *
 * Renames files with MM-DD-YY-HH-MMAM- prefix
 * Example: file.log -> 11-25-25-10-30AM-file.log
 */
int add_timestamp_to_files(const char* dir_path);

/**
 * @brief Add timestamp prefix to files with UploadLogsNow-specific exclusions
 * @param dir_path Directory containing files
 * @return 0 on success, -1 on failure
 *
 * Like add_timestamp_to_files() but skips files that already have AM/PM 
 * timestamps, reboot logs, and ABL reason logs (matches shell script logic)
 */
int add_timestamp_to_files_uploadlogsnow(const char* dir_path);

/**
 * @brief Remove timestamp prefix from all files in directory
 * @param dir_path Directory containing files
 * @return 0 on success, -1 on failure
 *
 * Restores original filenames by removing MM-DD-YY-HH-MMAM- prefix
 */
int remove_timestamp_from_files(const char* dir_path);

/**
 * @brief Move all contents from source to destination directory
 * @param src_dir Source directory
 * @param dest_dir Destination directory
 * @return 0 on success, -1 on failure
 */
int move_directory_contents(const char* src_dir, const char* dest_dir);

/**
 * @brief Remove all files and subdirectories from directory
 * @param dir_path Directory to clean
 * @return 0 on success, -1 on failure
 *
 * Note: Directory itself is not deleted, only its contents
 */
int clean_directory(const char* dir_path);

/**
 * @brief Clear old packet capture files, keeping only most recent 10
 * @param log_path Directory containing PCAP files
 * @return 0 on success, -1 on failure
 */
int clear_old_packet_captures(const char* log_path);

/**
 * @brief Remove old directories matching pattern and older than days
 * @param base_path Base directory to search
 * @param pattern Glob pattern to match (e.g., "*-*-*-*-*M-logbackup")
 * @param days_old Minimum age in days for removal
 * @return Number of directories removed, or -1 on error
 */
int remove_old_directories(const char* base_path, const char* pattern, int days_old);

#endif /* FILE_OPERATIONS_H */
