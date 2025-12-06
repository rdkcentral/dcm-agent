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
 * @file uploadstblogs.h
 * @brief Main header for uploadSTBLogs application
 *
 * This file contains the main entry point declarations and high-level
 * application interfaces.
 */

#ifndef UPLOADSTBLOGS_H
#define UPLOADSTBLOGS_H

#include "uploadstblogs_types.h"

/**
 * @brief Parse command-line arguments
 * @param argc Argument count
 * @param argv Argument vector
 * @param ctx Runtime context to populate
 * @return true on success, false on failure
 */
bool parse_args(int argc, char** argv, RuntimeContext* ctx);

/**
 * @brief Acquire file lock to ensure single instance
 * @param lock_path Path to lock file
 * @return true if lock acquired, false otherwise
 */
bool acquire_lock(const char* lock_path);

/**
 * @brief Release previously acquired lock
 */
void release_lock(void);

/**
 * @brief Main application entry point
 */
int main(int argc, char** argv);

#endif /* UPLOADSTBLOGS_H */
