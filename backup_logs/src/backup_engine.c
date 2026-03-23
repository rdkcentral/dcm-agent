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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



#include "backup_engine.h"
#include "system_utils.h"
#include "sys_integration.h"
#include "special_files.h"
#include "backup_types.h"

/* RDK Logging component name for Backup Logs */


/* Helper function to move log files matching patterns */
int move_log_files_by_pattern(const char* source_dir, const char* dest_dir) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Moving log files from %s to %s\n", source_dir, dest_dir);
    
    DIR* dir = opendir(source_dir);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to open source directory: %s\n", source_dir);
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    struct dirent* entry;
    int moved_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char source_file[PATH_MAX];
        snprintf(source_file, sizeof(source_file), "%s/%s", source_dir, entry->d_name);
        
        /* Check if it's a regular file */
        if (filePresentCheck(source_file) != 0) {
            continue;
        }
        
        /* Check if filename matches patterns: *.txt*, *.log*, bootlog */
        const char* name = entry->d_name;
        
        /* Exclude backup_logs.log from processing to prevent moving active log file */
        if (strcmp(name, "backup_logs.log.0") == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Skipping active log file: %s\n", name);
            continue;
        }
        
        bool matches = (strcmp(name, "bootlog") == 0) || 
                      (strstr(name, ".txt") != NULL) || 
                      (strstr(name, ".log") != NULL);
        
        if (matches) {
            char dest_file[PATH_MAX];
            snprintf(dest_file, sizeof(dest_file), "%s/%s", dest_dir, entry->d_name);
            
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Moving log file: %s -> %s\n", source_file, dest_file);
            
            if (copyFiles(source_file, dest_file) == 0) {
                if (remove(source_file) != 0) { /* Move operation: copy + delete */
                    RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to remove source file after copy: %s\n", source_file);
                }
                moved_count++;
                RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Successfully moved: %s\n", entry->d_name);
            } else {
                RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to move: %s\n", entry->d_name);
            }
        }
    }
    
    closedir(dir);
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Pattern-based file move completed. Files moved: %d\n", moved_count);
    return moved_count > 0 ? BACKUP_SUCCESS : BACKUP_ERROR_FILESYSTEM;
}

/* Execute HDD-enabled backup strategy */
int backup_execute_hdd_enabled_strategy(const backup_config_t* config) {
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing HDD-enabled backup strategy\n");
    
    const char* sysLog = "messages.txt";
    char syslog_path[PATH_MAX];
    
    /* Check path length to avoid truncation */
    if (strlen(config->prev_log_path) + strlen("/") + strlen(sysLog) >= PATH_MAX) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Path too long: %s\n", config->prev_log_path);
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    strcpy(syslog_path, config->prev_log_path);
    strcat(syslog_path, "/");
    strcat(syslog_path, sysLog);
    
    if (filePresentCheck(syslog_path) != 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "First time backup - moving logs to %s\n", config->prev_log_path);
        /* First time - move logs directly to PREV_LOG_PATH */
        move_log_files_by_pattern(config->log_path, config->prev_log_path);
        
        /* Touch last_reboot */
        char last_reboot_path[PATH_MAX];
        
        /* Check path length */
        if (strlen(config->prev_log_path) + 13 >= PATH_MAX) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Path too long for last_reboot\n");
            return BACKUP_ERROR_FILESYSTEM;
        }
        
        strcpy(last_reboot_path, config->prev_log_path);
        strcat(last_reboot_path, "/last_reboot");
        FILE *fp = fopen(last_reboot_path, "a");
        if (fp) {
            fclose(fp);
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Created last_reboot marker: %s\n", last_reboot_path);
        }
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Subsequent backup - creating timestamped directory\n");
        /* Remove existing last_reboot markers */
        DIR* dir = opendir(config->prev_log_path);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, "last_reboot") == 0) {
                    char marker_path[PATH_MAX];
                    
                    /* Check path length */
                    if (strlen(config->prev_log_path) + strlen("/") + strlen(entry->d_name) >= PATH_MAX) {
                        continue; /* Skip this file if path would be too long */
                    }
                    
                    strcpy(marker_path, config->prev_log_path);
                    strcat(marker_path, "/");
                    strcat(marker_path, entry->d_name);
                    if (remove(marker_path) != 0) {
                        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to remove last_reboot marker: %s\n", marker_path);
                    }
                }
            }
            closedir(dir);
        }
        
        /* Create timestamped directory */
        time_t rawtime;
        struct tm *timeinfo;
        char timestamp[32];
        char timestamped_path[PATH_MAX];
        
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timestamp, sizeof(timestamp), "%m-%d-%y-%I-%M-%S%p", timeinfo);
        
        /* Check path length */
        if (strlen(config->prev_log_path) + strlen("/logbackup-") + strlen(timestamp) >= PATH_MAX) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Timestamped path would be too long\n");
            return BACKUP_ERROR_FILESYSTEM;
        }
        
        strcpy(timestamped_path, config->prev_log_path);
        strcat(timestamped_path, "/logbackup-");
        strcat(timestamped_path, timestamp);
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Creating timestamped backup directory: %s\n", timestamped_path);
        
        /* Create timestamped directory */
        if (createDir(timestamped_path) != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to create timestamped directory: %s\n", timestamped_path);
            return BACKUP_ERROR_FILESYSTEM;
        }
        
        /* Move files to timestamped directory */
        move_log_files_by_pattern(config->log_path, timestamped_path);
        
        /* Touch last_reboot in timestamped directory */
        char last_reboot_path[PATH_MAX];
        
        /* Check path length */
        if (strlen(timestamped_path) + 13 >= PATH_MAX) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Path too long for timestamped last_reboot\n");
            return BACKUP_ERROR_FILESYSTEM;
        }
        
        strcpy(last_reboot_path, timestamped_path);
        strcat(last_reboot_path, "/last_reboot");
        FILE *fp = fopen(last_reboot_path, "a");
        if (fp) {
            fclose(fp);
        }
    }
    
    return BACKUP_SUCCESS;
}

/* Execute HDD-disabled backup strategy with rotation */
int backup_execute_hdd_disabled_strategy(const backup_config_t* config) {
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing HDD-disabled backup strategy with rotation\n");
    /* Define log file names like shell script does */
    const char* sysLog = "messages.txt";
    const char* sysLogBAK1 = "bak1_messages.txt";
    const char* sysLogBAK2 = "bak2_messages.txt";
    const char* sysLogBAK3 = "bak3_messages.txt";
    
    /* Build file paths for checking */
    char syslog_path[PATH_MAX], bak1_path[PATH_MAX], bak2_path[PATH_MAX], bak3_path[PATH_MAX];
    
    /* Check base path length */
    size_t base_len = strlen(config->prev_log_path);
    if (base_len + 19 >= PATH_MAX) { /* 19 = strlen("/bak1_messages.txt") + 1 */
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Base path too long: %s\n", config->prev_log_path);
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    strcpy(syslog_path, config->prev_log_path); strcat(syslog_path, "/"); strcat(syslog_path, sysLog);
    strcpy(bak1_path, config->prev_log_path); strcat(bak1_path, "/"); strcat(bak1_path, sysLogBAK1);
    strcpy(bak2_path, config->prev_log_path); strcat(bak2_path, "/"); strcat(bak2_path, sysLogBAK2);
    strcpy(bak3_path, config->prev_log_path); strcat(bak3_path, "/"); strcat(bak3_path, sysLogBAK3);
    
    /* Ensure paths end with slash for backup_and_recover_logs */
    char log_path_slash[PATH_MAX], prev_log_path_slash[PATH_MAX];
    
    /* Check lengths */
    if (strlen(config->log_path) + 2 >= PATH_MAX || strlen(config->prev_log_path) + 2 >= PATH_MAX) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Path too long for slash addition\n");
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    strcpy(log_path_slash, config->log_path); strcat(log_path_slash, "/");
    strcpy(prev_log_path_slash, config->prev_log_path); strcat(prev_log_path_slash, "/");
    
    /* HDD disabled backup rotation logic */
    if (filePresentCheck(syslog_path) != 0) {
        /* First time - move all logs directly */
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "First time HDD-disabled backup - moving all logs\n");
        backup_and_recover_logs(log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "", "");
    } else if (filePresentCheck(bak1_path) != 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Moving logs to bak1_ prefix\n");
        backup_and_recover_logs(log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "", "bak1_");
    } else if (filePresentCheck(bak2_path) != 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Moving logs to bak2_ prefix\n");
        backup_and_recover_logs(log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "", "bak2_");
    } else if (filePresentCheck(bak3_path) != 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Moving logs to bak3_ prefix\n");
        backup_and_recover_logs(log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "", "bak3_");
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Performing full rotation cycle\n");
        /* Full rotation: bak1->current, bak2->bak1, bak3->bak2, new->bak3 */
        backup_and_recover_logs(prev_log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "bak1_", "");
        backup_and_recover_logs(prev_log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "bak2_", "bak1_");
        backup_and_recover_logs(prev_log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "bak3_", "bak2_");
        backup_and_recover_logs(log_path_slash, prev_log_path_slash, BACKUP_OP_MOVE, "", "bak3_");
    }
    
    /* Touch last_reboot file */
    char last_reboot_path[PATH_MAX];
    
    /* Check path length */
    if (strlen(config->prev_log_path) + 13 >= PATH_MAX) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Path too long for last_reboot\n");
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    strcpy(last_reboot_path, config->prev_log_path);
    strcat(last_reboot_path, "/last_reboot");
    FILE *fp = fopen(last_reboot_path, "a");
    if (fp) {
        fclose(fp);
    }
    
    /* Cleanup LOG_PATH like shell script does: rm -rf $LOG_PATH slash asterisk dot asterisk */
    DIR* dir = opendir(config->log_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char file_path[PATH_MAX];
            
            /* Check path length */
            if (strlen(config->log_path) + strlen("/") + strlen(entry->d_name) >= PATH_MAX) {
                continue; /* Skip if path would be too long */
            }
            
            strcpy(file_path, config->log_path);
            strcat(file_path, "/");
            strcat(file_path, entry->d_name);
            if (remove(file_path) != 0) {
                RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to remove file during log cleanup: %s\n", file_path);
            }
        }
        closedir(dir);
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "HDD-disabled backup strategy completed successfully\n");
    return BACKUP_SUCCESS;
}

/* Backup and recover logs with specified operation */
int backup_and_recover_logs(const char* source, const char* dest, 
                           backup_operation_type_t op, const char* s_ext, 
                           const char* d_ext) {
    if (!source || !dest) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "backup_and_recover_logs: NULL source or dest parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "backup_and_recover_logs: %s -> %s, op=%d, s_ext='%s', d_ext='%s'\n",
            source, dest, op, s_ext ? s_ext : "(none)", d_ext ? d_ext : "(none)");
    char source_file[PATH_MAX];
    char dest_file[PATH_MAX];
    char combined_prefix[PATH_MAX];
    
    int file_count = 0;
    int success_count = 0;
    
    /* Build combined prefix for path removal: source + s_ext */
    snprintf(combined_prefix, sizeof(combined_prefix), "%s%s", 
             source, s_ext ? s_ext : "");
    
    /* Open source directory */
    DIR* dir = opendir(source);
    if (!dir) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to open source directory: %s\n", source);
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    struct dirent* entry;
    
    /* Process each file in directory */
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. entries */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Exclude backup_logs.log from processing to prevent moving active log file */
        if (strcmp(entry->d_name, "backup_logs.log.0") == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Skipping active log file: %s\n", entry->d_name);
            continue;
        }
        
        /* Build full source file path */
        snprintf(source_file, sizeof(source_file), "%s%s", source, entry->d_name);
        
        /* Check if it's a regular file (match shell script -type f).
         * Use open(O_NOFOLLOW) + fstat() to eliminate TOCTOU (CWE-367):
         * opening with O_NOFOLLOW refuses symlinks, and fstat() on the
         * resulting fd operates on the same inode already held open,
         * so no race window exists between the check and the use. */
        struct stat file_stat;
        int check_fd = open(source_file, O_RDONLY | O_NOFOLLOW);
        if (check_fd < 0) {
            /* Skip if file cannot be opened (e.g. symlink or permission denied) */
            continue;
        }
        if (fstat(check_fd, &file_stat) != 0) {
            close(check_fd);
            continue;
        }
        close(check_fd);
        if (S_ISDIR(file_stat.st_mode)) {
            /* Skip directories - we don't want to backup directories to PreviousLogs */
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Skipping directory: %s\n", source_file);
            continue;
        }
        if (!S_ISREG(file_stat.st_mode)) {
            /* Skip non-regular files (symlinks, devices, etc.) */
            continue;
        }
        
        /* Apply pattern matching like shell script: find -name "$s_ext*" */
        if (s_ext && strlen(s_ext) > 0) {
            /* Only process files that start with s_ext */
            if (strncmp(entry->d_name, s_ext, strlen(s_ext)) != 0) {
                continue;
            }
        }
        /* If s_ext is empty/NULL, process all files (matches shell behavior) */
        
        file_count++;
        
        /* Build destination filename using shell script logic:
         * $operation "$file" "$destn$d_extn${file/$source$s_extn/}"
         * This removes the combined source+s_ext prefix from full path */
        const char* remaining_path;
        if (strlen(combined_prefix) > 0 && strncmp(source_file, combined_prefix, strlen(combined_prefix)) == 0) {
            /* Remove combined prefix from full source path */
            remaining_path = source_file + strlen(combined_prefix);
        } else {
            /* Fallback: just use the filename if prefix doesn't match */
            remaining_path = entry->d_name;
        }
        
        /* Build final destination: dest + d_ext + remaining_path */
        snprintf(dest_file, sizeof(dest_file), "%s%s%s", 
                dest, 
                d_ext ? d_ext : "", 
                remaining_path);
        
        /* Perform the operation */
        int result;
        if (op == BACKUP_OP_MOVE) {
            /* Use copyFiles followed by remove for move operation */
            result = copyFiles(source_file, dest_file);
            if (result == 0) {
                /* Remove source file only if copy succeeded */
                if (remove(source_file) != 0) {
                    result = -1;
                }
            }
        } else if (op == BACKUP_OP_COPY) {
            result = copyFiles(source_file, dest_file);
        } else {
            result = -1;
        }
        
        if (result == 0) {
            success_count++;
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Successfully processed: %s -> %s\n", source_file, dest_file);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to process: %s -> %s\n", source_file, dest_file);
        }
    }
    
    closedir(dir);
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "backup_and_recover_logs completed: %d/%d files processed successfully\n", 
            success_count, file_count);
    
    /* Return success if we processed files successfully, or if no files were found */
    return (file_count == 0 || success_count > 0) ? BACKUP_SUCCESS : BACKUP_ERROR_FILESYSTEM;
}

/* Execute common backup operations (special files, version files, notifications) */
int backup_execute_common_operations(const backup_config_t* config) {
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing common backup operations\n");

    /* Declared static to avoid large stack frame (~264KB) - CWE-400 / STACK_USE.
     * Placed in BSS segment instead of the stack. Reset before each use. */
    static special_files_config_t special_config;
    memset(&special_config, 0, sizeof(special_config));

    /* Initialize special files manager */
    special_files_init();
    
    /* Load configuration from file */
    int result = special_files_load_config(&special_config, "/etc/backup_logs/special_files.conf");
    if (result == BACKUP_SUCCESS && special_config.count > 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Processing %zu special files\n", special_config.count);
        /* Execute all special file operations */
        result = special_files_execute_all(&special_config, config);
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "No special files configuration found or empty config\n");
    }
    /* If config file doesn't exist or is empty, skip special files processing */

    /* Send systemd notification like shell script does */
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Sending systemd notification\n");
    sys_send_systemd_notification("Logs Backup Done..!");
    
    /* Cleanup special files manager */
    special_files_cleanup();
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Common backup operations completed\n");
    return BACKUP_SUCCESS;
}
