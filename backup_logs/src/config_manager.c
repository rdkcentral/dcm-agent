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
#include <stdbool.h>



#include "config_manager.h"
#include "rdk_fwdl_utils.h"
#include "common_device_api.h"
#include "backup_types.h"


/* RDK Logging component name for Backup Logs */


/* Load backup configuration - simplified version matching shell script */
int config_load(backup_config_t* config) {
    char log_path_buf[32] = {0};
    char hdd_enabled_buf[32] = {0};
    char app_persistent_path_buf[32] = {0};
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting configuration loading\n");
    
    if (!config) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration loading failed: NULL config parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }

    /* Get LOG_PATH from include properties (equivalent to sourcing include.properties) */
    if (getIncludePropertyData("LOG_PATH", log_path_buf, sizeof(log_path_buf)) == 0 && strlen(log_path_buf) > 0) {
        strncpy(config->log_path, log_path_buf, sizeof(config->log_path) - 1);
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "LOG_PATH loaded from properties: %s\n", log_path_buf);
    } else {
        /* Default fallback */
        strncpy(config->log_path, "/opt/logs", sizeof(config->log_path) - 1);
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "LOG_PATH not found in properties, using default: /opt/logs\n");
    }
    config->log_path[sizeof(config->log_path) - 1] = '\0';
    
    /* Build derived paths like the shell script does */
    int ret1 = snprintf(config->prev_log_path, sizeof(config->prev_log_path), "%s/PreviousLogs", config->log_path);
    if (ret1 >= sizeof(config->prev_log_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "prev_log_path truncated: required %d bytes, available %zu\n", 
                ret1, sizeof(config->prev_log_path));
        return BACKUP_ERROR_CONFIG;
    }
    
    int ret2 = snprintf(config->prev_log_backup_path, sizeof(config->prev_log_backup_path), "%s/PreviousLogs_backup", config->log_path);
    if (ret2 >= sizeof(config->prev_log_backup_path)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "prev_log_backup_path truncated: required %d bytes, available %zu\n", 
                ret2, sizeof(config->prev_log_backup_path));
        return BACKUP_ERROR_CONFIG;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Derived paths - prev_log_path: %s, prev_log_backup_path: %s\n", 
            config->prev_log_path, config->prev_log_backup_path);
    
    /* Handle APP_PERSISTENT_PATH like the shell script */
    if (getDevicePropertyData("APP_PERSISTENT_PATH", app_persistent_path_buf, sizeof(app_persistent_path_buf)) == 0 && strlen(app_persistent_path_buf) > 0) {
        strncpy(config->persistent_path, app_persistent_path_buf, sizeof(config->persistent_path) - 1);
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "APP_PERSISTENT_PATH loaded from properties: %s\n", app_persistent_path_buf);
    } else {
        /* Default fallback */
        strncpy(config->persistent_path, "/opt/persistent", sizeof(config->persistent_path) - 1);
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "APP_PERSISTENT_PATH not found in properties, using default: /opt/persistent\n");
    }
    config->persistent_path[sizeof(config->persistent_path) - 1] = '\0';
    
    /* Check HDD_ENABLED like shell script */
    if (getDevicePropertyData("HDD_ENABLED", hdd_enabled_buf, sizeof(hdd_enabled_buf)) == 0) {
        config->hdd_enabled = (strcmp(hdd_enabled_buf, "false") != 0);
        RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "HDD_ENABLED loaded from properties: %s (evaluated to %s)\n", 
                hdd_enabled_buf, config->hdd_enabled ? "true" : "false");
    } else {
        config->hdd_enabled = true;  /* Default to true if not found */
        RDK_LOG(RDK_LOG_WARN, LOG_BACKUP_LOGS, "HDD_ENABLED not found in properties, using default: true\n");
    }
    
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Configuration loading completed successfully\n");
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Final config - log_path: %s, persistent_path: %s, hdd_enabled: %s\n",
            config->log_path, config->persistent_path, config->hdd_enabled ? "true" : "false");
    
    return BACKUP_SUCCESS;
}

/* Global configuration instance */
static backup_config_t g_config = {0};
static bool g_config_loaded = false;

/* Validate backup configuration */
int config_validate(const backup_config_t* config) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting configuration validation\n");
    
    if (!config) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration validation failed: NULL config parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }

    /* Check if log_path is set and valid */
    if (strlen(config->log_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration validation failed: log_path is empty\n");
        return BACKUP_ERROR_CONFIG;
    }

    /* Check if persistent_path is set and valid */
    if (strlen(config->persistent_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration validation failed: persistent_path is empty\n");
        return BACKUP_ERROR_CONFIG;
    }

    /* Check if derived paths are properly constructed */
    if (strlen(config->prev_log_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration validation failed: prev_log_path is empty\n");
        return BACKUP_ERROR_CONFIG;
    }

    if (strlen(config->prev_log_backup_path) == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Configuration validation failed: prev_log_backup_path is empty\n");
        return BACKUP_ERROR_CONFIG;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Configuration validation completed successfully\n");
    return BACKUP_SUCCESS;
}

/* Get log path from configuration */
const char* config_get_log_path(void) {
    if (!g_config_loaded) {
        if (config_load(&g_config) != BACKUP_SUCCESS) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to load configuration in config_get_log_path\n");
            return NULL;
        }
        g_config_loaded = true;
    }
    
    return g_config.log_path;
}

/* Check if HDD is enabled */
bool config_is_hdd_enabled(void) {
    if (!g_config_loaded) {
        if (config_load(&g_config) != BACKUP_SUCCESS) {
            RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Failed to load configuration in config_is_hdd_enabled\n");
            return true;  /* Default to true on error */
        }
        g_config_loaded = true;
    }
    
    return g_config.hdd_enabled;
}
