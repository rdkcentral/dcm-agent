/**
 * Copyright 2020 RDK Management
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
 * @file usb_log_utils.c
 * @brief Utility functions implementation for USB log upload
 *
 * This file contains the implementation of common utility functions
 * including logging, configuration management, and error handling.
 */

#include "usb_log_utils.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include "rdk_debug.h"
#include "safec_lib.h"


/* RDK utility constants */
#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 1
#endif
#ifndef UTILS_FAIL
#define UTILS_FAIL -1
#endif

/* RDK Logging component name for USB Log Upload */
#define LOG_USB_UPLOAD "LOG.RDK.USBLOGUPLOAD"
#define DEBUG_INI_NAME "/etc/debug.ini"

/* Static logging state */
static int g_log_initialized = 0;
static int g_rdk_logger_enabled = 0;
static int g_config_loaded = 0;

/**
 * @brief Initialize logging system
 * 
 * @return int 0 on success, negative error code on failure
 */
int usb_log_init(void)
{
    if (g_log_initialized) {
        return 0; /* Already initialized */
    }

#ifdef RDK_LOGGER_EXT
    /* Extended RDK logger configuration */
    rdk_logger_ext_config_t config = {
        .pModuleName = "LOG.RDK.USBLOGUPLOAD",    /* Module name */
        .loglevel = RDK_LOG_INFO,                  /* Default log level */
        .output = RDKLOG_OUTPUT_CONSOLE,           /* Output to console (stdout/stderr) */
        .format = RDKLOG_FORMAT_WITH_TS,           /* Timestamped format */
        .pFilePolicy = NULL                        /* Not using file output, so NULL */
    };
    
    if (rdk_logger_ext_init(&config) != RDK_SUCCESS) {
        printf("USBLOGUPLOAD : ERROR - Extended logger init failed\n");
    }
#endif

    /* Initialize RDK debug logging */
    if (0 == rdk_logger_init(DEBUG_INI_NAME)) {
        g_rdk_logger_enabled = 1;
        g_log_initialized = 1;
        RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, "[%s:%d] USB Log Upload RDK Logger initialized\n", __FUNCTION__, __LINE__);
    } else {
        fprintf(stderr, "WARNING: USB Log Upload RDK Logger initialization failed, using fallback logging\n");
        g_log_initialized = 1; /* Mark as initialized even if RDK logger failed */
    }
    
    return 0;
}

/**
 * @brief Send signal to reload syslog-ng service
 * 
 * @return int 0 on success, negative error code on failure
 */
int reload_syslog_service(void)
{
    char syslog_enabled[64];
    char log_path[256];
    char timestamp_buf[32];
    
    /* Check if SYSLOG_NG_ENABLED is set to "true" */
    memset(syslog_enabled, 0, sizeof(syslog_enabled));
    if (getDevicePropertyData("SYSLOG_NG_ENABLED", syslog_enabled, sizeof(syslog_enabled)) != UTILS_SUCCESS) {
        /* SYSLOG_NG_ENABLED not found, skip reload */
        return 0;
    }
    
    if (strcmp(syslog_enabled, "true") != 0) {
        /* SYSLOG_NG_ENABLED is not "true", skip reload */
        return 0;
    }
    
    /* Get LOG_PATH for logging */
    memset(log_path, 0, sizeof(log_path));
    if (getIncludePropertyData("LOG_PATH", log_path, sizeof(log_path)) != UTILS_SUCCESS) {
        strncpy(log_path, "/opt/logs", sizeof(log_path) - 1);
    }
    
    /* Get current timestamp */
    if (get_current_timestamp(timestamp_buf, sizeof(timestamp_buf)) != 0) {
        strncpy(timestamp_buf, "00/00/00-00:00:00", sizeof(timestamp_buf) - 1);
    }
    
    /* Log the reload attempt */
    RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
            "[%s:%d] %s Sending SIGHUP to reload syslog-ng\n", 
            __FUNCTION__, __LINE__, timestamp_buf);
    
    /* Send SIGHUP signal to syslog-ng process */
    /* Find syslog-ng PID first */
    FILE *pid_fp = popen("pidof syslog-ng", "r");
    if (!pid_fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to find syslog-ng process\n", __FUNCTION__, __LINE__);
        return -1;
    }
    
    char pid_str[32];
    if (!fgets(pid_str, sizeof(pid_str), pid_fp)) {
        pclose(pid_fp);
        RDK_LOG(RDK_LOG_WARN, LOG_USB_UPLOAD, 
                "[%s:%d] syslog-ng process not found\n", __FUNCTION__, __LINE__);
        return 0; /* Not an error - service may not be running */
    }
    pclose(pid_fp);
    
    /* Convert PID string to integer */
    pid_t syslog_pid = (pid_t)atoi(pid_str);
    if (syslog_pid <= 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Invalid syslog-ng PID: %s\n", __FUNCTION__, __LINE__, pid_str);
        return -1;
    }
    
    /* Send SIGHUP signal using kill() */
    if (kill(syslog_pid, SIGHUP) == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_USB_UPLOAD, 
                "[%s:%d] %s syslog-ng reloaded successfully\n", 
                __FUNCTION__, __LINE__, timestamp_buf);
        
        return 0;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_USB_UPLOAD, 
                "[%s:%d] Failed to send SIGHUP to syslog-ng PID %d: %s\n", 
                __FUNCTION__, __LINE__, syslog_pid, strerror(errno));
        return -1;
    }
}

/**
 * @brief Perform filesystem sync operation
 * 
 * @return int 0 on success, negative error code on failure
 */
int perform_filesystem_sync(void)
{
    /* Perform filesystem sync to flush all data to storage */
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Performing filesystem sync\n", __FUNCTION__, __LINE__);
    
    sync();
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_USB_UPLOAD, 
            "[%s:%d] Filesystem sync completed\n", __FUNCTION__, __LINE__);
    
    return 0;
}

/**
 * @brief Get current timestamp for logging
 * 
 * @param timestamp_buffer Buffer to store timestamp
 * @param buffer_size Size of timestamp_buffer
 * @return int 0 on success, negative error code on failure
 */
int get_current_timestamp(char *timestamp_buffer, size_t buffer_size)
{
    if (!timestamp_buffer || buffer_size < 20) {
        return -1; /* Invalid parameters */
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return -2; /* Failed to get time */
    }

    /* Format: MM/DD/YY-HH:MM:SS */
    size_t written = strftime(timestamp_buffer, buffer_size, "%m/%d/%y-%H:%M:%S", tm_info);
    if (written == 0) {
        return -3; /* Buffer too small */
    }

    return 0;
}