/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "special_files.h"
#include "system_utils.h"

/* Initialize special files manager */
int special_files_init(void) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting special files manager initialization\n");
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Special files manager initialization completed successfully\n");
    return BACKUP_SUCCESS;
}

/* Cleanup special files manager */
void special_files_cleanup(void) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting special files manager cleanup\n");
    
    /* Nothing to cleanup */
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Special files manager cleanup completed\n");
}

/* Load special files configuration from config file */
int special_files_load_config(special_files_config_t* config, const char* config_file) {
    FILE* fp;
    char line[512];
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting special files configuration loading from: %s\n", 
            config_file ? config_file : "(null)");
    
    if (!config || !config_file) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration loading failed: NULL parameter (config=%p, config_file=%p)\n", 
                (void*)config, (void*)config_file);
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    /* Initialize config */
    config->count = 0;
    config->config_loaded = false;
    
    /* Try to open config file */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Attempting to open config file: %s\n", config_file);
    fp = fopen(config_file, "r");
    if (!fp) {
        /* Config file not found - return with empty config */
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Config file not found: %s (errno: %d - %s)\n", 
                config_file, errno, strerror(errno));
        config->config_loaded = false;
        return BACKUP_ERROR_CONFIG;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Config file opened successfully: %s\n", config_file);
    
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
            
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Loaded special file entry %zu: source=%s, dest=%s\n", 
                    config->count, entry->source_path, entry->destination_path);
            config->count++;
        }
    }
    
    fclose(fp);
    config->config_loaded = true;
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Special files configuration loading completed successfully - loaded %zu entries\n", config->count);
    return BACKUP_SUCCESS;
}

/* Simple validation for special file entry */
int special_files_validate_entry(const special_file_entry_t* entry) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Validating special file entry\n");
    
    if (!entry) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Entry validation failed: NULL entry parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    if (strlen(entry->source_path) == 0 || strlen(entry->destination_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Entry validation failed: empty paths (source='%s', dest='%s')\n", 
                entry->source_path, entry->destination_path);
        return BACKUP_ERROR_CONFIG;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Entry validation successful for: %s -> %s\n", 
            entry->source_path, entry->destination_path);
    return BACKUP_SUCCESS;
}

/* Execute single special file operation */
int special_files_execute_entry(const special_file_entry_t* entry, 
                               const backup_config_t* backup_config) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting execution of special file entry\n");
    
    if (!entry) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Entry execution failed: NULL entry parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    /* Validate entry */
    int result = special_files_validate_entry(entry);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Entry execution aborted due to validation failure\n");
        return result;
    }
    
    /* Build full destination path using backup config */
    char full_dest_path[PATH_MAX];
    if (backup_config && strlen(backup_config->log_path) > 0) {
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
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Full destination path resolved: %s\n", full_dest_path);
    
    /* Check if source file exists */
    if (filePresentCheck(entry->source_path) != 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Source file does not exist, skipping: %s\n", entry->source_path);
        return BACKUP_SUCCESS;  /* File doesn't exist - not an error */
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Source file exists, proceeding with operation: %s\n", entry->source_path);
    
    /* Determine operation manually based on specific files like original script */
    bool should_move = false;
    if (strcmp(entry->source_path, "/tmp/disk_cleanup.log") == 0 ||
        strcmp(entry->source_path, "/tmp/mount_log.txt") == 0 ||
        strcmp(entry->source_path, "/tmp/mount-ta_log.txt") == 0) {
        should_move = true;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing %s operation: %s -> %s\n", 
            should_move ? "MOVE" : "COPY", entry->source_path, full_dest_path);
    
    /* Execute operation */
    if (should_move) {
        /* Move operation: copy + delete */
        result = copyFiles((char*)entry->source_path, full_dest_path);
        if (result == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Copy completed, removing source file: %s\n", entry->source_path);
            if (remove(entry->source_path) != 0) {
                RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to remove source file: %s (errno: %d - %s)\n", 
                        entry->source_path, errno, strerror(errno));
                result = -1;
            } else {
                RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Source file removed successfully: %s\n", entry->source_path);
            }
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Copy operation failed for move: %s -> %s\n", 
                    entry->source_path, full_dest_path);
        }
    } else {
        /* Copy operation for version files */
        result = copyFiles((char*)entry->source_path, full_dest_path);
        if (result != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Copy operation failed: %s -> %s\n", 
                    entry->source_path, full_dest_path);
        } else {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Copy operation completed successfully: %s -> %s\n", 
                    entry->source_path, full_dest_path);
        }
    }
    
    int final_result = (result == 0) ? BACKUP_SUCCESS : BACKUP_ERROR_FILESYSTEM;
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Special file operation completed with result: %s\n", 
            final_result == BACKUP_SUCCESS ? "SUCCESS" : "FAILURE");
    
    return final_result;
}

/* Execute all special file operations from config */
int special_files_execute_all(const special_files_config_t* config, 
                             const backup_config_t* backup_config) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting execution of all special file operations\n");
    
    if (!config) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Execute all failed: NULL config parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Processing %zu special file entries\n", config->count);
    int success_count = 0;
    
    /* Process all entries in config */
    for (size_t i = 0; i < config->count; i++) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Processing special file entry %zu of %zu\n", i + 1, config->count);
        int result = special_files_execute_entry(&config->entries[i], backup_config);
        if (result == BACKUP_SUCCESS) {
            success_count++;
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Special file entry %zu failed, continuing with remaining entries\n", i + 1);
        }
        /* Continue processing even if individual operations fail */
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Special file operations completed: %d/%zu successful\n", 
            success_count, config->count);
    return BACKUP_SUCCESS;
}
