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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "special_files.h"
#include "system_utils.h"

/* Initialize special files manager */
int special_files_init(void) {
    return BACKUP_SUCCESS;
}

/* Cleanup special files manager */
void special_files_cleanup(void) {
    /* Nothing to cleanup */
}

/* Load special files configuration from config file */
int special_files_load_config(special_files_config_t* config, const char* config_file) {
    FILE* fp;
    char line[512];
    
    if (!config || !config_file) {
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    /* Initialize config */
    config->count = 0;
    config->config_loaded = false;
    
    /* Try to open config file */
    fp = fopen(config_file, "r");
    if (!fp) {
        /* Config file not found - return with empty config */
        config->config_loaded = false;
        return BACKUP_ERROR_CONFIG;
    }
    
    /* Read lines from config file */
    while (fgets(line, sizeof(line), fp) && config->count < MAX_SPECIAL_FILES) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        /* Remove trailing newline */
        char* newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }
        newline = strchr(line, '\r');
        if (newline) {
            *newline = '\0';
        }
        
        /* Skip empty lines after trimming */
        if (strlen(line) == 0) {
            continue;
        }
        
        /* Process filename */
        if (strlen(line) > 0) {
            special_file_entry_t* entry = &config->entries[config->count];
            
            /* Copy source path directly */
            strncpy(entry->source_path, line, sizeof(entry->source_path) - 1);
            entry->source_path[sizeof(entry->source_path) - 1] = '\0';
            
            /* Determine destination filename from source path */
            const char* filename = strrchr(line, '/');
            if (filename) {
                filename++; /* Skip the '/' */
            } else {
                filename = line; /* No path separator, use entire string */
            }
            
            strncpy(entry->destination_path, filename, sizeof(entry->destination_path) - 1);
            entry->destination_path[sizeof(entry->destination_path) - 1] = '\0';
            
            /* All operations will be determined manually in execute function */
            entry->operation = SPECIAL_FILE_COPY; /* Default, will be overridden */
            entry->conditional_check[0] = '\0';  /* No conditions */
            
            config->count++;
        }
    }
    
    fclose(fp);
    config->config_loaded = true;
    
    return BACKUP_SUCCESS;
}

/* Simple validation for special file entry */
int special_files_validate_entry(const special_file_entry_t* entry) {
    if (!entry) {
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    if (strlen(entry->source_path) == 0 || strlen(entry->destination_path) == 0) {
        return BACKUP_ERROR_CONFIG;
    }
    
    return BACKUP_SUCCESS;
}

/* Execute single special file operation */
int special_files_execute_entry(const special_file_entry_t* entry, 
                               const backup_config_t* backup_config) {
    if (!entry) {
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    /* Validate entry */
    int result = special_files_validate_entry(entry);
    if (result != BACKUP_SUCCESS) {
        return result;
    }
    
    /* Build full destination path using backup config */
    char full_dest_path[PATH_MAX];
    if (backup_config && backup_config->log_path) {
        int ret = snprintf(full_dest_path, sizeof(full_dest_path), "%s/%s", 
                backup_config->log_path, entry->destination_path);
        if (ret >= (int)sizeof(full_dest_path)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "full_dest_path truncated: required %d bytes, available %zu\n", 
                    ret, sizeof(full_dest_path));
            return BACKUP_ERROR_CONFIG;
        }
    } else {
        strncpy(full_dest_path, entry->destination_path, sizeof(full_dest_path) - 1);
        full_dest_path[sizeof(full_dest_path) - 1] = '\0';
    }
    
    /* Check if source file exists */
    if (filePresentCheck(entry->source_path) != 0) {
        return BACKUP_SUCCESS;  /* File doesn't exist - not an error */
    }
    
    /* Determine operation manually based on specific files like original script */
    bool should_move = false;
    if (strcmp(entry->source_path, "/tmp/disk_cleanup.log") == 0 ||
        strcmp(entry->source_path, "/tmp/mount_log.txt") == 0 ||
        strcmp(entry->source_path, "/tmp/mount-ta_log.txt") == 0) {
        should_move = true;
    }
    
    /* Execute operation */
    if (should_move) {
        /* Move operation: copy + delete */
        result = copyFiles((char*)entry->source_path, full_dest_path);
        if (result == 0) {
            if (remove(entry->source_path) != 0) {
                result = -1;
            }
        }
    } else {
        /* Copy operation for version files */
        result = copyFiles((char*)entry->source_path, full_dest_path);
    }
    
    return (result == 0) ? BACKUP_SUCCESS : BACKUP_ERROR_FILESYSTEM;
}

/* Execute all special file operations from config */
int special_files_execute_all(const special_files_config_t* config, 
                             const backup_config_t* backup_config) {
    if (!config) {
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    int success_count = 0;
    
    /* Process all entries in config */
    for (size_t i = 0; i < config->count; i++) {
        int result = special_files_execute_entry(&config->entries[i], backup_config);
        if (result == BACKUP_SUCCESS) {
            success_count++;
        }
        /* Continue processing even if individual operations fail */
    }
    
    return BACKUP_SUCCESS;
}

