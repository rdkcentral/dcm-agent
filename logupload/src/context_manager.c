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
 * @file context_manager.c
 * @brief Runtime context initialization and management implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "context_manager.h"
#include "rdk_fwdl_utils.h"
#include "common_device_api.h"
#include "rdk_debug.h"
#include "rbus_interface.h"

#define DEBUG_INI_NAME "/etc/debug.ini"


static int g_rdk_logger_enabled = 0;



/**
 * @brief Check if direct upload path is blocked based on marker file age
 * @param block_time Maximum blocking time in seconds
 * @return true if blocked, false if not blocked or block expired
 */
bool is_direct_blocked(int block_time)
{
    const char *block_file = "/tmp/.lastdirectfail_upl";
    struct stat file_stat;
    
    if (stat(block_file, &file_stat) != 0) {
        // File doesn't exist, not blocked
        return false;
    }
    
    time_t current_time = time(NULL);
    time_t mod_time = file_stat.st_mtime;
    time_t elapsed = current_time - mod_time;
    
    if (elapsed <= block_time) {
        // Still within block period
        int remaining_hours = (block_time - elapsed) / 3600;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Last direct failed blocking is still valid for %d hrs, preventing direct\n",
                __FUNCTION__, __LINE__, remaining_hours);
        return true;
    } else {
        // Block period expired, remove file
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Last direct failed blocking has expired, removing %s, allowing direct\n",
                __FUNCTION__, __LINE__, block_file);
        unlink(block_file);
        return false;
    }
}

/**
 * @brief Check if CodeBig upload path is blocked based on marker file age
 * @param block_time Maximum blocking time in seconds
 * @return true if blocked, false if not blocked or block expired
 */
bool is_codebig_blocked(int block_time)
{
    const char *block_file = "/tmp/.lastcodebigfail_upl";
    struct stat file_stat;
    
    if (stat(block_file, &file_stat) != 0) {
        // File doesn't exist, not blocked
        return false;
    }
    
    time_t current_time = time(NULL);
    time_t mod_time = file_stat.st_mtime;
    time_t elapsed = current_time - mod_time;
    
    if (elapsed <= block_time) {
        // Still within block period
        int remaining_mins = (block_time - elapsed) / 60;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Last Codebig failed blocking is still valid for %d mins, preventing Codebig\n",
                __FUNCTION__, __LINE__, remaining_mins);
        return true;
    } else {
        // Block period expired, remove file
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] Last Codebig failed blocking has expired, removing %s, allowing Codebig\n",
                __FUNCTION__, __LINE__, block_file);
        unlink(block_file);
        return false;
    }
}

bool init_context(RuntimeContext* ctx)
{
    // Initialize RDK Logger

    if (0 == rdk_logger_init(DEBUG_INI_NAME)) {
        g_rdk_logger_enabled = 1;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] RDK Logger initialized\n", __FUNCTION__, __LINE__);
    } else {
        fprintf(stderr, "WARNING: RDK Logger initialization failed, using fallback logging\n");
    }

    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Context pointer is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    // Zero out the entire context structure
    memset(ctx, 0, sizeof(RuntimeContext));

    // Load environment properties from config files
    if (!load_environment(ctx)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to load environment properties\n", __FUNCTION__, __LINE__);
        return false;
    }

    // Load TR-181 parameters
    if (!load_tr181_params(ctx)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to load TR-181 parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    // Get device MAC address
    if (!get_mac_address(ctx->device.mac_address, sizeof(ctx->device.mac_address))) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to get MAC address\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Context initialization successful\n", __FUNCTION__, __LINE__);
    return true;
}

bool load_environment(RuntimeContext* ctx)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Context pointer is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    char buffer[32] = {0};

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Loading environment properties\n", __FUNCTION__, __LINE__);

    // Load LOG_PATH from /etc/include.properties
    // Used throughout script: PREV_LOG_PATH, DCM_LOG_FILE, RRD_LOG_FILE, TLS_LOG_FILE
    if (getIncludePropertyData("LOG_PATH", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        strncpy(ctx->paths.log_path, buffer, sizeof(ctx->paths.log_path) - 1);
        ctx->paths.log_path[sizeof(ctx->paths.log_path) - 1] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] LOG_PATH=%s\n", __FUNCTION__, __LINE__, ctx->paths.log_path);
    } else {
        // Use default if not found
        strncpy(ctx->paths.log_path, "/opt/logs", sizeof(ctx->paths.log_path) - 1);
        ctx->paths.log_path[sizeof(ctx->paths.log_path) - 1] = '\0';
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] LOG_PATH not found, using default: %s\n", __FUNCTION__, __LINE__, ctx->paths.log_path);
    }



    // Construct PREV_LOG_PATH = "$LOG_PATH/PreviousLogs"
    // Ensure sufficient space for the suffix
    size_t log_path_len = strlen(ctx->paths.log_path);
    if (log_path_len + 14 <= sizeof(ctx->paths.prev_log_path)) {
        memset(ctx->paths.prev_log_path, 0, sizeof(ctx->paths.prev_log_path));
        strcpy(ctx->paths.prev_log_path, ctx->paths.log_path);
        strcat(ctx->paths.prev_log_path, "/PreviousLogs");
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] LOG_PATH too long for constructing PREV_LOG_PATH\n", 
                __FUNCTION__, __LINE__);
        strncpy(ctx->paths.prev_log_path, "/opt/logs/PreviousLogs", sizeof(ctx->paths.prev_log_path) - 1);
        ctx->paths.prev_log_path[sizeof(ctx->paths.prev_log_path) - 1] = '\0';
    }

    // Set DRI_LOG_PATH (hardcoded in script)
    strncpy(ctx->paths.dri_log_path, "/opt/logs/drilogs", 
            sizeof(ctx->paths.dri_log_path) - 1);
    ctx->paths.dri_log_path[sizeof(ctx->paths.dri_log_path) - 1] = '\0';

    // Set RRD_LOG_FILE = "$LOG_PATH/remote-debugger.log"
    // Ensure sufficient space for the suffix
    if (log_path_len + 21 <= sizeof(ctx->paths.rrd_file)) {
        memset(ctx->paths.rrd_file, 0, sizeof(ctx->paths.rrd_file));
        strcpy(ctx->paths.rrd_file, ctx->paths.log_path);
        strcat(ctx->paths.rrd_file, "/remote-debugger.log");
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] LOG_PATH too long for constructing RRD_LOG_FILE\n", 
                __FUNCTION__, __LINE__);
        strncpy(ctx->paths.rrd_file, "/opt/logs/remote-debugger.log", sizeof(ctx->paths.rrd_file) - 1);
        ctx->paths.rrd_file[sizeof(ctx->paths.rrd_file) - 1] = '\0';
    }

    // Load DIRECT_BLOCK_TIME from /etc/include.properties (default: 86400 = 24 hours)
    memset(buffer, 0, sizeof(buffer));
    if (getIncludePropertyData("DIRECT_BLOCK_TIME", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        ctx->retry.direct_retry_delay = atoi(buffer);
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DIRECT_BLOCK_TIME=%d\n", __FUNCTION__, __LINE__, ctx->retry.direct_retry_delay);
    } else {
        ctx->retry.direct_retry_delay = 86400;  // Default 24 hours
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DIRECT_BLOCK_TIME not found, using default: %d\n", __FUNCTION__, __LINE__, ctx->retry.direct_retry_delay);
    }

    // Load CB_BLOCK_TIME from /etc/include.properties (default: 1800 = 30 minutes)
    memset(buffer, 0, sizeof(buffer));
    if (getIncludePropertyData("CB_BLOCK_TIME", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        ctx->retry.codebig_retry_delay = atoi(buffer);
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] CB_BLOCK_TIME=%d\n", __FUNCTION__, __LINE__, ctx->retry.codebig_retry_delay);
    } else {
        ctx->retry.codebig_retry_delay = 1800;  // Default 30 minutes
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] CB_BLOCK_TIME not found, using default: %d\n", __FUNCTION__, __LINE__, ctx->retry.codebig_retry_delay);
    }

    // Load PROXY_BUCKET from /etc/device.properties (for mediaclient proxy fallback)
    memset(buffer, 0, sizeof(buffer));
    if (getDevicePropertyData("PROXY_BUCKET", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        strncpy(ctx->endpoints.proxy_bucket, buffer, sizeof(ctx->endpoints.proxy_bucket) - 1);
        ctx->endpoints.proxy_bucket[sizeof(ctx->endpoints.proxy_bucket) - 1] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] PROXY_BUCKET=%s\n", __FUNCTION__, __LINE__, ctx->endpoints.proxy_bucket);
    } else {
        ctx->endpoints.proxy_bucket[0] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] PROXY_BUCKET not found, proxy fallback disabled\n", __FUNCTION__, __LINE__);
    }

    // Set hardcoded retry attempts and timeouts from script
    ctx->retry.direct_max_attempts = 3;      // NUM_UPLOAD_ATTEMPTS=3
    ctx->retry.codebig_max_attempts = 1;     // CB_NUM_UPLOAD_ATTEMPTS=1
    ctx->retry.curl_timeout = 10;            // CURL_TIMEOUT=10
    ctx->retry.curl_tls_timeout = 30;        // CURL_TLS_TIMEOUT=30

    // Load DEVICE_TYPE from /etc/device.properties
    memset(buffer, 0, sizeof(buffer));
    if (getDevicePropertyData("DEVICE_TYPE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        strncpy(ctx->device.device_type, buffer, sizeof(ctx->device.device_type) - 1);
        ctx->device.device_type[sizeof(ctx->device.device_type) - 1] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DEVICE_TYPE=%s\n", __FUNCTION__, __LINE__, ctx->device.device_type);
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] DEVICE_TYPE not found in device.properties\n", __FUNCTION__, __LINE__);
    }

    // Load BUILD_TYPE from /etc/device.properties
    memset(buffer, 0, sizeof(buffer));
    if (getDevicePropertyData("BUILD_TYPE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        strncpy(ctx->device.build_type, buffer, sizeof(ctx->device.build_type) - 1);
        ctx->device.build_type[sizeof(ctx->device.build_type) - 1] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] BUILD_TYPE=%s\n", __FUNCTION__, __LINE__, ctx->device.build_type);
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] BUILD_TYPE not found in device.properties\n", __FUNCTION__, __LINE__);
    }

    // Set TELEMETRY_PATH (hardcoded in script)
    strncpy(ctx->paths.telemetry_path, "/opt/.telemetry", sizeof(ctx->paths.telemetry_path) - 1);
    ctx->paths.telemetry_path[sizeof(ctx->paths.telemetry_path) - 1] = '\0';

    // Set DCM_LOG_FILE path
    if (log_path_len + 16 <= sizeof(ctx->paths.dcm_log_file)) {
        memset(ctx->paths.dcm_log_file, 0, sizeof(ctx->paths.dcm_log_file));
        strcpy(ctx->paths.dcm_log_file, ctx->paths.log_path);
        strcat(ctx->paths.dcm_log_file, "/dcmscript.log");
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] LOG_PATH too long for constructing DCM_LOG_FILE\n", 
                __FUNCTION__, __LINE__);
        strncpy(ctx->paths.dcm_log_file, "/opt/logs/dcmscript.log", sizeof(ctx->paths.dcm_log_file) - 1);
        ctx->paths.dcm_log_file[sizeof(ctx->paths.dcm_log_file) - 1] = '\0';
    }

    // Load DCM_LOG_PATH from /etc/device.properties (default: /tmp/DCM/)
    memset(buffer, 0, sizeof(buffer));
    if (getDevicePropertyData("DCM_LOG_PATH", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        strncpy(ctx->paths.dcm_log_path, buffer, sizeof(ctx->paths.dcm_log_path) - 1);
        ctx->paths.dcm_log_path[sizeof(ctx->paths.dcm_log_path) - 1] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DCM_LOG_PATH=%s\n", __FUNCTION__, __LINE__, ctx->paths.dcm_log_path);
    } else {
        strncpy(ctx->paths.dcm_log_path, "/tmp/DCM/", sizeof(ctx->paths.dcm_log_path) - 1);
        ctx->paths.dcm_log_path[sizeof(ctx->paths.dcm_log_path) - 1] = '\0';
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] DCM_LOG_PATH not found, using default: %s\n", __FUNCTION__, __LINE__, ctx->paths.dcm_log_path);
    }

    // Check for TLS support (set TLS flag if /etc/os-release exists)
    if (access("/etc/os-release", F_OK) == 0) {
        ctx->settings.tls_enabled = true;
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] TLS 1.2 support enabled\n", __FUNCTION__, __LINE__);
    } else {
        ctx->settings.tls_enabled = false;
    }

    // Set IARM event binary location based on os-release
    if (access("/etc/os-release", F_OK) == 0) {
        strncpy(ctx->paths.iarm_event_binary, "/usr/bin", sizeof(ctx->paths.iarm_event_binary) - 1);
    } else {
        strncpy(ctx->paths.iarm_event_binary, "/usr/local/bin", sizeof(ctx->paths.iarm_event_binary) - 1);
    }
    ctx->paths.iarm_event_binary[sizeof(ctx->paths.iarm_event_binary) - 1] = '\0';
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] IARM_EVENT_BINARY_LOCATION=%s\n", 
            __FUNCTION__, __LINE__, ctx->paths.iarm_event_binary);

    // Check for maintenance mode enable
    memset(buffer, 0, sizeof(buffer));
    if (getDevicePropertyData("ENABLE_MAINTENANCE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        if (strcasecmp(buffer, "true") == 0) {
            ctx->settings.maintenance_enabled = true;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Maintenance mode enabled\n", __FUNCTION__, __LINE__);
        }
    }

    
    // Check for OCSP marker files
    // EnableOCSPStapling="/tmp/.EnableOCSPStapling"
    // EnableOCSP="/tmp/.EnableOCSPCA"
    if (access("/tmp/.EnableOCSPStapling", F_OK) == 0 || 
        access("/tmp/.EnableOCSPCA", F_OK) == 0) {
        ctx->settings.ocsp_enabled = true;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] OCSP validation enabled\n", __FUNCTION__, __LINE__);
    }

    // Check for block marker files with time-based validation
    // DIRECT_BLOCK_FILENAME="/tmp/.lastdirectfail_upl"
    // CB_BLOCK_FILENAME="/tmp/.lastcodebigfail_upl"
    // These functions check file existence, age, and auto-remove expired blocks
    ctx->settings.direct_blocked = is_direct_blocked(ctx->retry.direct_retry_delay);
    ctx->settings.codebig_blocked = is_codebig_blocked(ctx->retry.codebig_retry_delay);

    // Set temp directory for archive operations
    strncpy(ctx->paths.temp_dir, "/tmp", sizeof(ctx->paths.temp_dir) - 1);
    strncpy(ctx->paths.archive_path, "/tmp", sizeof(ctx->paths.archive_path) - 1);

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Environment properties loaded successfully\n", __FUNCTION__, __LINE__);
    return true;
}

bool load_tr181_params(RuntimeContext* ctx)
{
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Context pointer is NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] Loading TR-181 parameters via RBUS\n", __FUNCTION__, __LINE__);

    // Initialize RBUS connection (idempotent - safe to call multiple times)
    if (!rbus_init()) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to initialize RBUS\n", __FUNCTION__, __LINE__);
        return false;
    }

    // Load LogUploadEndpoint URL
    // Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL
    if (!rbus_get_string_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL",
                               ctx->endpoints.endpoint_url, 
                               sizeof(ctx->endpoints.endpoint_url))) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to get LogUploadEndpoint.URL\n", 
                __FUNCTION__, __LINE__);
    } else {
        fprintf(stderr, "DEBUG: endpoint_url from TR-181 = '%s'\n", ctx->endpoints.endpoint_url);
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] LogUploadEndpoint.URL = '%s'\n", 
                __FUNCTION__, __LINE__, ctx->endpoints.endpoint_url);
    }

    // Load EncryptCloudUpload Enable flag (boolean parameter)
    // Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.EncryptCloudUpload.Enable
    if (!rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.EncryptCloudUpload.Enable",
                             &ctx->settings.encryption_enable)) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to get EncryptCloudUpload.Enable, using default: false\n", 
                __FUNCTION__, __LINE__);
        ctx->settings.encryption_enable = false;
    }

    // Load Privacy Mode (Device.X_RDKCENTRAL-COM_Privacy.PrivacyMode)
    // Used to check if user has disabled telemetry/log upload
    char privacy_mode[32] = {0};
    if (rbus_get_string_param("Device.X_RDKCENTRAL-COM_Privacy.PrivacyMode",
                             privacy_mode, sizeof(privacy_mode))) {
        // PrivacyMode values: "DO_NOT_SHARE" or "SHARE"
        ctx->settings.privacy_do_not_share = (strcasecmp(privacy_mode, "DO_NOT_SHARE") == 0);
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] Privacy Mode: %s (do_not_share=%d)\n", 
                __FUNCTION__, __LINE__, privacy_mode, ctx->settings.privacy_do_not_share);
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to get PrivacyMode, using default: false\n", 
                __FUNCTION__, __LINE__);
        ctx->settings.privacy_do_not_share = false;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] TR-181 parameters loaded via RBUS\n", __FUNCTION__, __LINE__);
    
    // Note: UploadLogsOnUnscheduledReboot.Disable is loaded at runtime when needed in maintenance window
    // Note: RDKRemoteDebugger.IssueType is only used for RRD mode which has separate handling

    return true;
}



bool get_mac_address(char* mac_buf, size_t buf_size)
{
    if (!mac_buf || buf_size == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    size_t copied = GetEstbMac(mac_buf, buf_size);
    
    if (copied > 0 && strlen(mac_buf) > 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] MAC address: %s\n", 
                __FUNCTION__, __LINE__, mac_buf);
        return true;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to get MAC address\n", 
                __FUNCTION__, __LINE__);
        return false;
    }
}

void cleanup_context(void)
{
    rbus_cleanup();
}
