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
 * @file cleanup_handler.h
 * @brief Cleanup and finalization operations
 *
 * This module handles:
 * - Post-upload cleanup (archive removal, block markers, state restoration)
 * - Log housekeeping (old backups, archive cleanup)
 * - Privacy enforcement (log truncation)
 * 
 * Combines functionality from cleanup_handler and cleanup_manager
 */

#ifndef CLEANUP_HANDLER_H
#define CLEANUP_HANDLER_H

#include <stdbool.h>
#include "uploadstblogs_types.h"

/* ==========================
   Upload Finalization
   ========================== */

/**
 * @brief Finalize upload operation
 * @param ctx Runtime context
 * @param session Session state
 *
 * Performs:
 * - Archive deletion
 * - Block marker updates
 * - Temporary directory cleanup
 * - Event emission
 * - Telemetry reporting
 */
void finalize(RuntimeContext* ctx, SessionState* session);

/**
 * @brief Enforce privacy mode (truncate logs)
 * @param log_path Path to logs directory
 */
void enforce_privacy(const char* log_path);

/**
 * @brief Update block markers after upload
 * @param ctx Runtime context
 * @param session Session state
 *
 * Rules:
 * - Success on CodeBig → block Direct for 24h
 * - Failure on CodeBig → block CodeBig for 30m
 */
void update_block_markers(const RuntimeContext* ctx, const SessionState* session);

/**
 * @brief Remove archive file
 * @param archive_path Path to archive file
 * @return true on success, false on failure
 */
bool remove_archive(const char* archive_path);

/**
 * @brief Clean temporary directories
 * @param ctx Runtime context
 * @return true on success, false on failure
 */
bool cleanup_temp_dirs(const RuntimeContext* ctx, const SessionState* session);

/**
 * @brief Create block marker file
 * @param path Upload path to block
 * @param duration_seconds Block duration in seconds
 * @return true on success, false on failure
 */
bool create_block_marker(UploadPath path, int duration_seconds);

/* ==========================
   Log Housekeeping
   ========================== */

/**
 * @brief Clean up old log backup folders
 * 
 * Removes timestamped log backup folders older than max_age_days.
 * Matches script behavior: find /opt/logs -name "*-*-*-*-*M-*" -mtime +3
 * 
 * @param log_path Base log directory path
 * @param max_age_days Maximum age in days (typically 3)
 * @return Number of folders removed
 */
int cleanup_old_log_backups(const char *log_path, int max_age_days);

/**
 * @brief Remove old tar.gz archive files
 * 
 * Removes .tgz files from log directory.
 * Matches script: find $LOG_PATH -name "*.tgz" -exec rm -rf {} \;
 * 
 * @param log_path Log directory path
 * @return Number of files removed
 */
int cleanup_old_archives(const char *log_path);

/**
 * @brief Check if path matches timestamped backup pattern
 * 
 * Patterns: *-*-*-*-*M- or *-*-*-*-*M-logbackup
 * Example: 11-30-25-03-45PM-logbackup
 * 
 * @param filename Filename or path to check
 * @return true if matches pattern, false otherwise
 */
bool is_timestamped_backup(const char *filename);

#endif /* CLEANUP_HANDLER_H */
