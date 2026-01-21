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
 * This module handles:
 * - Log file collection and filtering
 * - Archive creation (tar.gz format)
 * - Timestamp management based on upload strategy
 * 
 * Combines functionality from archive_manager and log_collector
 */

#ifndef ARCHIVE_MANAGER_H
#define ARCHIVE_MANAGER_H

#include "uploadstblogs_types.h"

/* ==========================
   Log Collection
   ========================== */

/**
 * @brief Collect log files for archiving
 * @param ctx Runtime context
 * @param session Session state
 * @param dest_dir Destination directory for collected logs
 * @return Number of files collected, or -1 on error
 *
 * Collects .log and .txt files, optionally PCAP and DRI logs
 * based on strategy and configuration.
 */
int collect_logs(const RuntimeContext* ctx, const SessionState* session, const char* dest_dir);

/**
 * @brief Collect previous logs
 * @param src_dir Source directory (PreviousLogs)
 * @param dest_dir Destination directory
 * @return Number of files copied, or -1 on error
 */
int collect_previous_logs(const char* src_dir, const char* dest_dir);

/**
 * @brief Collect PCAP files if enabled
 * @param ctx Runtime context
 * @param dest_dir Destination directory
 * @return Number of files collected, or -1 on error
 */
int collect_pcap_logs(const RuntimeContext* ctx, const char* dest_dir);

/**
 * @brief Collect DRI logs if enabled
 * @param ctx Runtime context
 * @param dest_dir Destination directory
 * @return Number of files collected, or -1 on error
 */
int collect_dri_logs(const RuntimeContext* ctx, const char* dest_dir);

/**
 * @brief Check if file should be included based on extension
 * @param filename File name to check
 * @return true if file should be collected, false otherwise
 */
bool should_collect_file(const char* filename);

/* ==========================
   Archive Management
   ========================== */

/**
 * @brief Create tar.gz archive from directory
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

/**
 * @brief Generate archive filename with MAC and timestamp
 * @param buffer Buffer to store filename
 * @param buffer_size Size of buffer
 * @param mac_address Device MAC address
 * @param prefix Filename prefix ("Logs" or "DRI_Logs")
 * @return true on success, false on failure
 */
bool generate_archive_name(char* buffer, size_t buffer_size, 
                           const char* mac_address, const char* prefix);

#endif /* ARCHIVE_MANAGER_H */
