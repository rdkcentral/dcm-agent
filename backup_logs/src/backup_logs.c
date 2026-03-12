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



#include "backup_logs.h"
#include "backup_types.h"
#include "config_manager.h"
#include "backup_engine.h"
#include "sys_integration.h"
#include "special_files.h"
#include "system_utils.h"

#define BACKUP_LOGS_VERSION "1.0.0"
#define BACKUP_LOGS_BUILD_DATE __DATE__
#define DEBUG_INI_NAME "/etc/debug.ini"





/* Print version information */
void backup_logs_print_version(void) {
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "backup_logs version %s (built %s)\n", BACKUP_LOGS_VERSION, BACKUP_LOGS_BUILD_DATE);
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Copyright 2024 Comcast Cable Communications Management, LLC\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Licensed under Apache License 2.0\n");
}

/* Print usage information */
void backup_logs_print_usage(const char *program_name) {
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Usage: %s [OPTIONS]\n", program_name);
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Log backup utility for RDK systems\n\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Options:\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -h, --help           Show this help message\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -v, --version        Show version information\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -d, --debug          Enable debug logging\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -f, --force-rotation Force log rotation regardless of HDD status\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -s, --skip-disk-check Skip disk usage checks\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -n, --no-cleanup     Skip cleanup operations\n");
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "  -c, --config FILE    Specify configuration file path\n");
}



/* Initialize backup system */
int backup_logs_init(backup_config_t *config) {
    /* Initialize RDK logging */
#ifdef RDK_LOGGER_EXT
    /* Extended RDK logger configuration */
    rdk_logger_ext_config_t logger_config = {
        .pModuleName = "LOG.RDK.BACKUPLOGS",       /* Module name */
        .loglevel = RDK_LOG_INFO,                  /* Default log level */
        .output = RDKLOG_OUTPUT_CONSOLE,           /* Output to console (stdout/stderr) */
        .format = RDKLOG_FORMAT_WITH_TS,           /* Timestamped format */
        .pFilePolicy = NULL                        /* Not using file output, so NULL */
    };
    
    if (rdk_logger_ext_init(&logger_config) != RDK_SUCCESS) {
        printf("BACKUP_LOGS : ERROR - Extended logger init failed\n");
    }
#endif

#ifdef RDK_LOGGER_ENABLED
    if (0 == rdk_logger_init(DEBUG_INI_NAME)) {
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "RDK Logger initialized successfully\n");
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "RDK Logger initialization failed, logging may not work properly\n");
    }
#endif
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Initializing backup system\n");
    
    /* Initializing backup system */
    
    /* Load configuration from properties files */
    int result = config_load(config);
    if (result != BACKUP_SUCCESS) {

        return result;
    }
    
    /* Create log workspace if not there */
    if (createDir((char*)config->log_path) != 0) {

        return BACKUP_ERROR_FILESYSTEM;
    }
    
    /* Create intermediate log workspace if not there */
    if (createDir((char*)config->prev_log_path) != 0) {

        return BACKUP_ERROR_FILESYSTEM;
    }
    
    /* Create log backup workspace if not there, clean it if exists */
    if (createDir((char*)config->prev_log_backup_path) != 0) {

        return BACKUP_ERROR_FILESYSTEM;
    } else {
        /* Clean the backup directory like shell script does: rm -rf $PREV_LOG_BACKUP_PATH/asterisk */
        if (emptyFolder((char*)config->prev_log_backup_path) != 0) {

            /* Continue anyway - not critical */
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
    if (createDir((char*)config->persistent_path) != 0) {

    }
    
    /* Touch the logFileBackup file */
    FILE *fp = fopen(persistent_file, "a");
    if (fp) {
        fclose(fp);
    } else {

        /* Continue anyway - not critical */
    }
    
    /* Run disk threshold check if script exists */
    if (filePresentCheck("/lib/rdk/disk_threshold_check.sh") == 0) {
        result = system("/lib/rdk/disk_threshold_check.sh 0");
        if (result != 0) {

            /* Continue anyway - not critical */
        }
    }
    
    return BACKUP_SUCCESS;
}

/* Execute complete backup process */
int backup_logs_execute(const backup_config_t *config) {
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Starting backup execution process\n");
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
        RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Removing last_reboot file: %s\n", last_bootfile);
        if (removeFile(last_bootfile) != 0) {
            RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "Failed to remove last_reboot file: %s\n", last_bootfile);

            /* Continue anyway - not critical */
        }
    }
    
    /* Execute appropriate backup strategy based on HDD_ENABLED */
    int result;
    if (config->hdd_enabled) {
        result = backup_execute_hdd_enabled_strategy(config);
    } else {
        result = backup_execute_hdd_disabled_strategy(config);
    }
    
    if (result != BACKUP_SUCCESS) {

        return result;
    }
    
    /* Execute common operations (special files, version files, systemd notification) */
    result = backup_execute_common_operations(config);
    if (result != BACKUP_SUCCESS) {

        /* Continue anyway - not critical for main backup operation */
    }
    
    return BACKUP_SUCCESS;
}

/* Cleanup and shutdown backup system */
int backup_logs_cleanup(backup_config_t *config) {
    /* Suppress unused parameter warning */
    (void)config;
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting backup system cleanup\n");
    /* Cleanup special files manager */
    special_files_cleanup();
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Backup system cleanup completed\n");
    return BACKUP_SUCCESS;
}

/* Main entry point */
int backup_logs_main(int argc, char *argv[]) {
    /* Suppress unused parameter warnings */
    (void)argc;
    (void)argv;
    
    int result;
    backup_config_t config = {0};

    /* Initialize backup system */
    result = backup_logs_init(&config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to initialize backup system: %d\n", result);
        return EXIT_FAILURE;
    }

    /* Execute backup process */
    result = backup_logs_execute(&config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Backup execution failed: %d\n", result);
        backup_logs_cleanup(&config);
        return EXIT_FAILURE;
    }

    /* Cleanup and exit */
    result = backup_logs_cleanup(&config);
    if (result != BACKUP_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Cleanup failed: %d\n", result);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* Standard main function for executable */
int main(int argc, char *argv[]) {
    return backup_logs_main(argc, argv);
}
