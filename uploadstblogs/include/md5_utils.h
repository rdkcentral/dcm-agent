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
 * @file md5_utils.h
 * @brief MD5 hash calculation utilities for file integrity
 */

#ifndef MD5_UTILS_H
#define MD5_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Calculate MD5 hash of a file and encode as base64
 * 
 * Matches script behavior: openssl md5 -binary < file | openssl enc -base64
 * 
 * @param filepath Path to file to hash
 * @param md5_base64 Output buffer for base64-encoded MD5 (min 25 bytes)
 * @param output_size Size of output buffer
 * @return true on success, false on failure
 */
bool calculate_file_md5(const char *filepath, char *md5_base64, size_t output_size);

#endif /* MD5_UTILS_H */
