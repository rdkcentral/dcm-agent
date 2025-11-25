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
 * @file archive_manager.h
 * @brief Log archive creation and management
 *
 * This module handles log collection, archive creation, and timestamp
 * management based on the selected upload strategy.
 */

#ifndef ARCHIVE_MANAGER_H
#define ARCHIVE_MANAGER_H

#include "uploadstblogs_types.h"

/**
 * @brief Prepare standard log archive
 * @param ctx Runtime context
 * @param session Session state
 * @return true on success, false on failure
 *
 * Creates .tgz archive with collected logs, applies timestamp
 * insertion for non-OnDemand/Privacy strategies.
 */
bool prepare_archive(RuntimeContext* ctx, SessionState* session);

/**
 * @brief Prepare RRD (Remote Debug) archive
 * @param ctx Runtime context
 * @param session Session state
 * @return true on success, false on failure
 *
 * Creates archive containing only RRD log file.
 */
bool prepare_rrd_archive(RuntimeContext* ctx, SessionState* session);

/**
 * @brief Get size of archive file
 * @param archive_path Path to archive file
 * @return Size in bytes, or -1 on error
 */
long get_archive_size(const char* archive_path);

/**
 * @brief Create tar.gz archive from directory
 * @param ctx Runtime context
 * @param session Session state
 * @param source_dir Source directory to archive
 * @return 0 on success, -1 on failure
 *
 * Creates archive named logs.tar.gz in source_dir
 */
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir);

/**
 * @brief Create DRI logs archive
 * @param ctx Runtime context
 * @param archive_path Output archive file path
 * @return 0 on success, -1 on failure
 *
 * Creates tar.gz archive containing DRI logs from DRI_LOG_PATH
 */
int create_dri_archive(RuntimeContext* ctx, const char* archive_path);

#endif /* ARCHIVE_MANAGER_H */
