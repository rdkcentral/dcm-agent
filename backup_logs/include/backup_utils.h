/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2024 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BACKUP_UTILS_H
#define BACKUP_UTILS_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate timestamp string
 * 
 * @param timestamp_str Buffer to store timestamp string
 * @param buffer_size Size of buffer
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_generate_timestamp(char* timestamp_str, size_t buffer_size);

/**
 * @brief Central backup logging function - equivalent to backupLog from original script
 * 
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void utils_backup_log(const char* format, ...);

/**
 * @brief Join path components
 * 
 * @param result Buffer to store joined path
 * @param result_size Size of result buffer
 * @param path1 First path component
 * @param path2 Second path component
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_join_path(char* result, size_t result_size, const char* path1, const char* path2);

/**
 * @brief Trim whitespace from string
 * 
 * @param str String to trim (modified in place)
 * @return char* Pointer to trimmed string
 */
char* utils_trim_whitespace(char* str);

/**
 * @brief Split string by delimiter
 * 
 * @param str String to split
 * @param delimiter Delimiter character
 * @param tokens Array to store token pointers
 * @param max_tokens Maximum number of tokens
 * @return int Number of tokens found
 */
int utils_split_string(char* str, char delimiter, char* tokens[], int max_tokens);

/**
 * @brief Check if string starts with prefix
 * 
 * @param str String to check
 * @param prefix Prefix to match
 * @return bool true if starts with prefix, false otherwise
 */
bool utils_starts_with(const char* str, const char* prefix);

/**
 * @brief Check if string ends with suffix
 * 
 * @param str String to check
 * @param suffix Suffix to match
 * @return bool true if ends with suffix, false otherwise
 */
bool utils_ends_with(const char* str, const char* suffix);

/**
 * @brief Safe string copy with bounds checking
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Size of destination buffer
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_safe_strcpy(char* dest, const char* src, size_t dest_size);

/**
 * @brief Safe string concatenation with bounds checking
 * 
 * @param dest Destination buffer
 * @param src Source string to append
 * @param dest_size Size of destination buffer
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_safe_strcat(char* dest, const char* src, size_t dest_size);

/**
 * @brief Case-insensitive string comparison
 * 
 * @param str1 First string
 * @param str2 Second string
 * @return int 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
int utils_strcasecmp(const char* str1, const char* str2);

/**
 * @brief Convert string to lowercase
 * 
 * @param str String to convert (modified in place)
 * @return char* Pointer to modified string
 */
char* utils_to_lowercase(char* str);

/**
 * @brief Convert string to uppercase
 * 
 * @param str String to convert (modified in place)
 * @return char* Pointer to modified string
 */
char* utils_to_uppercase(char* str);

/**
 * @brief Get basename from file path
 * 
 * @param path File path
 * @param basename Buffer to store basename
 * @param basename_size Size of basename buffer
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_get_basename(const char* path, char* basename, size_t basename_size);

/**
 * @brief Get dirname from file path
 * 
 * @param path File path
 * @param dirname Buffer to store dirname
 * @param dirname_size Size of dirname buffer
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_get_dirname(const char* path, char* dirname, size_t dirname_size);

/**
 * @brief Get file extension
 * 
 * @param filename File name
 * @param extension Buffer to store extension
 * @param extension_size Size of extension buffer
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int utils_get_extension(const char* filename, char* extension, size_t extension_size);

/**
 * @brief Pattern matching with wildcards
 * 
 * @param pattern Pattern with wildcards (* and ?)
 * @param text Text to match against pattern
 * @return bool true if pattern matches, false otherwise
 */
bool utils_pattern_match(const char* pattern, const char* text);

#ifdef __cplusplus
}
#endif

#endif /* BACKUP_UTILS_H */
