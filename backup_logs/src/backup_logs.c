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
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>



#include "backup_logs.h"
#include "backup_types.h"
#include "config_manager.h"
#include "backup_engine.h"
#include "sys_integration.h"
#include "special_files.h"
#include "system_utils.h"
#include <secure_wrapper.h>

#define BACKUP_LOGS_VERSION "1.0.0"
#define BACKUP_LOGS_BUILD_DATE __DATE__
#define DEBUG_INI_NAME "/etc/debug.ini"

/* Backup-specific symlink-aware file existence check 
 * Unlike dcmUtilsFilePresentCheck which uses stat() and follows symlinks,
 * this function uses lstat() to check the file/symlink itself regardless of target existence
 */
static int backup_filePresentCheck_symlink_aware(const char *file_name) {
    if (!file_name) {
        return -1;  // Invalid parameter
    }
    
    struct stat sfile;
    memset(&sfile, 0, sizeof(sfile));
    
    /* Use lstat() instead of stat() to check symlink itself, not its target */
    if (lstat(file_name, &sfile) != 0) {
        return -1;  // File/symlink doesn't exist
    }
    
    return 0;  // File/symlink exists (regardless of target validity)
}

/* Initialize backup system */
int backup_logs_init(backup_config_t *config) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting backup system initialization\n");
    
    if (!config) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Backup initialization failed: NULL config parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }
    /* Initialize RDK logging */
#ifdef RDK_LOGGER_EXT
    /* Extended RDK logger configuration with file output */
    rdk_LogOutput_File filelog;
    strncpy(filelog.fileName, "backup_logs.log", sizeof(filelog.fileName)-1);
    filelog.fileName[sizeof(filelog.fileName) - 1] = '\0';
    strncpy(filelog.fileLocation, "/tmp/", sizeof(filelog.fileLocation)-1);
    filelog.fileLocation[sizeof(filelog.fileLocation) - 1] = '\0';
    filelog.fileSizeMax = 51200;  /* 50KB max file size */
    filelog.fileCountMax = 5;     /* Keep 5 rotated files */

    rdk_logger_ext_config_t logger_config = {
        .pModuleName = "LOG.RDK.BACKUPLOGS",       /* Module name */
        .loglevel = RDK_LOG_INFO,                  /* Default log level */
        .output = RDKLOG_OUTPUT_FILE,              /* Output to FILE */
        .format = RDKLOG_FORMAT_WITH_TS,           /* Timestamped format */
        .pFilePolicy = &filelog                    /* Using file output */
    };
    
    if (rdk_logger_ext_init(&logger_config) != RDK_SUCCESS) {
        printf("BACKUP_LOGS : ERROR - Extended logger init failed\n");
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "RDK Logger initialized with file output: /opt/logs/backup_logs.log\n");
    }
#endif

#ifdef RDK_LOGGER_ENABLED
    if (0 == rdk_logger_init(DEBUG_INI_NAME)) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "RDK Logger initialized successfully\n");
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "RDK Logger initialization failed, logging may not work properly\n");
    }
#endif
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Initializing backup system configuration\n");
    
    /* Initializing backup system */
    
    /* Load configuration from properties files */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Loading backup configuration\n");
    int result = config_load(config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration loading failed with result: %d\n", result);
        return result;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Configuration loaded successfully - log_path: %s, hdd_enabled: %s\n", 
            config->log_path, config->hdd_enabled ? "true" : "false");
    
    /* Create log workspace if not there */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Creating log directory: %s\n", config->log_path);
    if (createDir((char*)config->log_path) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to create log directory: %s\n", config->log_path);
        return BACKUP_ERROR_FILESYSTEM;
    }
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Log directory created/verified: %s\n", config->log_path);
    
    /* Create intermediate log workspace if not there */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Creating previous logs directory: %s\n", config->prev_log_path);
    if (createDir((char*)config->prev_log_path) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to create previous logs directory: %s\n", config->prev_log_path);
        return BACKUP_ERROR_FILESYSTEM;
    }
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Previous logs directory created/verified: %s\n", config->prev_log_path);
    
    /* Create log backup workspace if not there, clean it if exists */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Creating/cleaning backup directory: %s\n", config->prev_log_backup_path);
    if (createDir((char*)config->prev_log_backup_path) != 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to create backup directory: %s\n", config->prev_log_backup_path);
        return BACKUP_ERROR_FILESYSTEM;
    } else {
        /* Clean the backup directory like shell script does: rm -rf $PREV_LOG_BACKUP_PATH/asterisk */
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Emptying backup directory: %s\n", config->prev_log_backup_path);
        if (emptyFolder((char*)config->prev_log_backup_path) != 0) {
            RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to empty backup directory: %s\n", config->prev_log_backup_path);
            /* Continue anyway - not critical */
        } else {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Backup directory emptied successfully: %s\n", config->prev_log_backup_path);
        }
    }
    
    /* Touch persistent file like shell script does */
    char persistent_file[PATH_MAX];
    
    /* Check path length to avoid truncation */
    size_t path_len = strlen(config->persistent_path);
    if (path_len + 15 >= PATH_MAX) { /* 15 = strlen("/logFileBackup") + 1 for null terminator */
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Persistent path too long: %s\n", config->persistent_path);
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    /* Safely construct the path */
    strcpy(persistent_file, config->persistent_path);
    strcat(persistent_file, "/logFileBackup");
    
    /* Create persistent directory if it doesn't exist */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Creating persistent directory: %s\n", config->persistent_path);
    if (createDir((char*)config->persistent_path) != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to create persistent directory: %s\n", config->persistent_path);
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Persistent directory created/verified: %s\n", config->persistent_path);
    }
    
    /* Touch the logFileBackup file */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Creating/touching persistent file: %s\n", persistent_file);
    FILE *fp = fopen(persistent_file, "a");
    if (fp) {
        fclose(fp);
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Persistent file created/touched successfully: %s\n", persistent_file);
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to create/touch persistent file: %s\n", persistent_file);
        /* Continue anyway - not critical */
    }
    
    /* Run disk threshold check if script exists */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Checking for disk threshold check script: /lib/rdk/disk_threshold_check.sh\n");
    if (filePresentCheck("/lib/rdk/disk_threshold_check.sh") == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing disk threshold check script with parameter 0 (bootup cleanup)\n");
        result = v_secure_system("/lib/rdk/disk_threshold_check.sh 0");
        if (result != 0) {
            RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Disk threshold check script failed with exit code: %d\n", result);
            /* Continue anyway - not critical */
        } else {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Disk threshold check script completed successfully\n");
        }
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Disk threshold check script not found, skipping\n");
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Backup system initialization completed successfully\n");
    return BACKUP_SUCCESS;
}

/* Execute complete backup process */
int backup_logs_execute(const backup_config_t *config) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting backup execution process\n");
    
    if (!config) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Backup execution failed: NULL config parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing backup with strategy: %s\n", 
            config->hdd_enabled ? "HDD-enabled" : "HDD-disabled");
    /* Find and remove last_reboot file like shell script does */
    char last_bootfile[PATH_MAX];
    
    /* Check path length to avoid truncation */
    size_t path_len = strlen(config->prev_log_path);
    if (path_len + 13 >= PATH_MAX) { /* 13 = strlen("/last_reboot") + 1 for null terminator */
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Previous log path too long: %s\n", config->prev_log_path);
        return BACKUP_ERROR_FILESYSTEM;
    }
    
    /* Safely construct the path */
    strcpy(last_bootfile, config->prev_log_path);
    strcat(last_bootfile, "/last_reboot");
    
    if (filePresentCheck(last_bootfile) == 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Found last_reboot file, removing: %s\n", last_bootfile);
        if (removeFile(last_bootfile) != 0) {
            RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to remove last_reboot file: %s\n", last_bootfile);
            /* Continue anyway - not critical */
        } else {
            RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Successfully removed last_reboot file: %s\n", last_bootfile);
        }
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "No last_reboot file found at: %s\n", last_bootfile);
    }
    
    /* Execute appropriate backup strategy based on HDD_ENABLED */
    int result;
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Executing backup strategy for HDD_ENABLED=%s\n", 
            config->hdd_enabled ? "true" : "false");
    if (config->hdd_enabled) {
        result = backup_execute_hdd_enabled_strategy(config);
    } else {
        result = backup_execute_hdd_disabled_strategy(config);
    }
    
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Backup strategy execution failed with result: %d\n", result);
        return result;
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Backup strategy execution completed successfully\n");
    
    /* Execute common operations (special files, version files, systemd notification) */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting common backup operations\n");
    result = backup_execute_common_operations(config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Common operations failed with result: %d, continuing\n", result);
        /* Continue anyway - not critical for main backup operation */
    } else {
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Common backup operations completed successfully\n");
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Backup execution process completed successfully\n");
    return BACKUP_SUCCESS;
}

/* Cleanup and shutdown backup system */
int backup_logs_cleanup(backup_config_t *config) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting backup system cleanup\n");
    
    /* Suppress unused parameter warning */
    (void)config;
    
    /* Cleanup special files manager */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Cleaning up special files manager\n");
    special_files_cleanup();
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Backup system cleanup completed successfully\n");
    return BACKUP_SUCCESS;
}

/* Main entry point */
int backup_logs_main(int argc, char *argv[]) {
    /* Start timing the program execution */
    struct timespec program_start, program_end;
    clock_gettime(CLOCK_MONOTONIC, &program_start);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting backup_logs main function with %d arguments\n", argc);
    
    /* Suppress unused parameter warnings */
    (void)argc;
    (void)argv;
    
    int result;
    /* Declared static to avoid large stack frame (~16KB) - CWE-400 / STACK_USE.
     * Placed in BSS segment instead of the stack. Reset before each use. */
    static backup_config_t config;
    memset(&config, 0, sizeof(config));

    /* Initialize backup system */
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Initializing backup system\n");
    result = backup_logs_init(&config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to initialize backup system with result: %d\n", result);
        return EXIT_FAILURE;
    }

    /* Execute backup process */
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Starting backup execution\n");
    result = backup_logs_execute(&config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Backup execution failed with result: %d\n", result);
        backup_logs_cleanup(&config);
        return EXIT_FAILURE;
    }

    /* Cleanup and exit */
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting cleanup and shutdown\n");
    result = backup_logs_cleanup(&config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Cleanup failed with result: %d\n", result);
        return EXIT_FAILURE;
    }

    /* Calculate and log total execution time */
    clock_gettime(CLOCK_MONOTONIC, &program_end);
    double total_time = (program_end.tv_sec - program_start.tv_sec) + (program_end.tv_nsec - program_start.tv_nsec) / 1000000000.0;
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Backup process completed successfully - Total runtime: %.3f seconds\n", total_time);
    return EXIT_SUCCESS;
}
#ifndef GTEST_ENABLE
/* Standard main function for executable */
int main(int argc, char *argv[]) {
    return backup_logs_main(argc, argv);
}
#endif
