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

#endif /* FILE_OPERATIONS_H */
