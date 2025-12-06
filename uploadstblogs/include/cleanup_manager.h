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
 * @file cleanup_manager.h
 * @brief Log cleanup and housekeeping utilities
 */

#ifndef CLEANUP_MANAGER_H
#define CLEANUP_MANAGER_H

#include <stdbool.h>

/**
 * @brief Clean up old log backup folders
 * 
 * Removes timestamped log backup folders older than 3 days
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
 * Removes .tgz files from log directory
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

#endif /* CLEANUP_MANAGER_H */
