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
 * @file log_collector.h
 * @brief Log file collection and filtering
 *
 * This module handles collection of log files from various directories
 * with filtering based on file type and strategy requirements.
 */

#ifndef LOG_COLLECTOR_H
#define LOG_COLLECTOR_H

#include "uploadstblogs_types.h"

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

#endif /* LOG_COLLECTOR_H */
