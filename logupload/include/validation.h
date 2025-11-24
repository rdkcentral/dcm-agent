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
 * @file validation.h
 * @brief System validation and prerequisite checks
 *
 * This module validates system prerequisites including directories,
 * binaries, and configuration before upload operations.
 */

#ifndef VALIDATION_H
#define VALIDATION_H

#include "uploadstblogs_types.h"

/**
 * @brief Validate system prerequisites
 * @param ctx Runtime context
 * @return true if system is valid, false otherwise
 *
 * Checks for required directories, binaries, and configuration files.
 */
bool validate_system(const RuntimeContext* ctx);

/**
 * @brief Check if required directories exist
 * @param ctx Runtime context
 * @return true if all directories exist, false otherwise
 */
bool validate_directories(const RuntimeContext* ctx);

/**
 * @brief Check if required binaries are available
 * @return true if all binaries exist, false otherwise
 */
bool validate_binaries(void);

/**
 * @brief Check if required configuration files exist
 * @return true if all config files exist, false otherwise
 */
bool validate_configuration(void);

#endif /* VALIDATION_H */
