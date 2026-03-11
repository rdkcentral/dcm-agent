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

#ifndef BACKUP_TYPES_H
#define BACKUP_TYPES_H

#include <stdbool.h>
#include <limits.h>
#include <sys/param.h>

#ifdef RDK_LOGGER_EXT
#define RDK_LOGGER_ENABLED
#endif

#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "rdk_logger.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Constants and Defines */
#define MAX_SPECIAL_FILES 32
#define MAX_ERROR_MESSAGE_LEN 256
#define MAX_FUNCTION_NAME_LEN 64
#define MAX_DESCRIPTION_LEN 128
#define MAX_CONDITIONAL_LEN 64

/* RDK Logging component name for Backup Logs */
#define LOG_BACKUP_LOGS "LOG.RDK.BACKUPLOGS"

/* Main backup configuration structure */
typedef struct {
    char log_path[PATH_MAX];
    char prev_log_path[PATH_MAX];
    char prev_log_backup_path[PATH_MAX];
    char persistent_path[PATH_MAX];
    bool hdd_enabled;
} backup_config_t;

/* Backup operation types */
typedef enum {
    BACKUP_OP_MOVE,
    BACKUP_OP_COPY,
    BACKUP_OP_DELETE
} backup_operation_type_t;

/* Special file operation types */
typedef enum {
    SPECIAL_FILE_COPY = 0,  // cp operation
    SPECIAL_FILE_MOVE = 1   // mv operation
} special_file_operation_t;

/* Special file entry structure */
typedef struct {
    char source_path[PATH_MAX];
    char destination_path[PATH_MAX];
    special_file_operation_t operation;
    char conditional_check[MAX_CONDITIONAL_LEN];  // Optional condition variable name
} special_file_entry_t;

/* Special files configuration container */
typedef struct {
    special_file_entry_t entries[MAX_SPECIAL_FILES];
    size_t count;
    bool config_loaded;
} special_files_config_t;

/* Backup operation structure */
typedef struct {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    backup_operation_type_t operation;
    char source_extension[32];
    char dest_extension[32];
} backup_operation_t;

/* Error information structure */
typedef struct {
    int error_code;
    char error_message[MAX_ERROR_MESSAGE_LEN];
    char function_name[MAX_FUNCTION_NAME_LEN];
    int line_number;
} error_info_t;

/* Return codes */
typedef enum {
    BACKUP_SUCCESS = 0,
    BACKUP_ERROR_CONFIG = -1,
    BACKUP_ERROR_FILESYSTEM = -2,
    BACKUP_ERROR_PERMISSIONS = -3,
    BACKUP_ERROR_MEMORY = -4,
    BACKUP_ERROR_INVALID_PARAM = -5,
    BACKUP_ERROR_NOT_FOUND = -6,
    BACKUP_ERROR_DISK_FULL = -7,
    BACKUP_ERROR_SYSTEM = -8
} backup_result_t;

/* Configuration flags */
typedef struct {
    bool debug_enabled;
    bool force_rotation;
    bool skip_disk_check;
    bool cleanup_enabled;
} backup_flags_t;

#ifdef __cplusplus
}
#endif

#endif /* BACKUP_TYPES_H */
