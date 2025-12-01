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
 * @file event_manager.c
 * @brief Event management implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "event_manager.h"
#include "telemetry.h"
#include "rdk_debug.h"
#include "system_utils.h"

// Event constants matching script behavior
#define LOG_UPLOAD_SUCCESS 0
#define LOG_UPLOAD_FAILED 1 
#define LOG_UPLOAD_ABORTED 2

#define MAINT_LOGUPLOAD_COMPLETE 4
#define MAINT_LOGUPLOAD_ERROR 5
#define MAINT_LOGUPLOAD_INPROGRESS 16

// IARM event sender binary location (matches script behavior)
// Script: Default /usr/bin, but /usr/local/bin if no /etc/os-release
// Check maintenance mode (matches script ENABLE_MAINTENANCE check)
static bool is_maintenance_enabled(void)
{
    char buffer[32] = {0};
    if (getDevicePropertyData("ENABLE_MAINTENANCE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        return (strcasecmp(buffer, "true") == 0);
    }
    return false;
}

// Check device type (matches script DEVICE_TYPE check)
static bool is_device_broadband(const RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    return (strcmp(ctx->device.device_type, "broadband") == 0);
}

static const char* get_iarm_binary_location(void)
{
    // Check if /etc/os-release exists (matches script logic)
    // Script: IARM_EVENT_BINARY_LOCATION=/usr/bin by default
    // Script: if [ ! -f /etc/os-release ]; then IARM_EVENT_BINARY_LOCATION=/usr/local/bin; fi
    if (access("/etc/os-release", F_OK) != 0) {
        return "/usr/local/bin/IARM_event_sender";
    }
    return "/usr/bin/IARM_event_sender";
}

void emit_privacy_abort(void)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Upload aborted due to privacy mode\n", __FUNCTION__, __LINE__);
    
    // Send maintenance complete event (matches script behavior)
    // Script sends MAINT_LOGUPLOAD_COMPLETE=4 for privacy mode, not ERROR
    send_iarm_event_maintenance(MAINT_LOGUPLOAD_COMPLETE);
}

void emit_no_logs_reboot(const RuntimeContext* ctx)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Log directory empty, skipping log upload\n", __FUNCTION__, __LINE__);
    
    // Send maintenance complete event only if device is not broadband and maintenance enabled
    // Matches script uploadLogOnReboot line 810: if [ "$DEVICE_TYPE" != "broadband" ] && [ "x$ENABLE_MAINTENANCE" == "xtrue" ]
    if (!is_device_broadband(ctx) && is_maintenance_enabled()) {
        send_iarm_event_maintenance(MAINT_LOGUPLOAD_COMPLETE);
    }
}

void emit_no_logs_ondemand(void)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Log directory empty, skipping log upload\n", __FUNCTION__, __LINE__);
    
    // Send maintenance complete event only if maintenance enabled (no device type check)
    // Matches script uploadLogOnDemand line 746: if [ "x$ENABLE_MAINTENANCE" == "xtrue" ]
    if (is_maintenance_enabled()) {
        send_iarm_event_maintenance(MAINT_LOGUPLOAD_COMPLETE);
    }
}

void emit_upload_success(const RuntimeContext* ctx, const SessionState* session)
{
    if (!session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid session state\n", __FUNCTION__, __LINE__);
        return;
    }
    
    const char* path_used = session->used_fallback ? "CodeBig" : "Direct";
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Upload completed successfully via %s path (attempts: direct=%d, codebig=%d)\n", 
            __FUNCTION__, __LINE__, path_used, session->direct_attempts, session->codebig_attempts);
    
    // Send telemetry for successful upload (matches script t2CountNotify)
    if (session->used_fallback) {
        // Use telemetry.h functions instead
        report_upload_success(session);
    } else {
        report_upload_success(session);
    }
    
    // Send success events (matches script behavior)
    send_iarm_event("LogUploadEvent", LOG_UPLOAD_SUCCESS);
    
    // Send maintenance event only if device is not broadband and maintenance enabled
    if (!is_device_broadband(ctx) && is_maintenance_enabled()) {
        send_iarm_event_maintenance(MAINT_LOGUPLOAD_COMPLETE);
    }
}

void emit_upload_failure(const RuntimeContext* ctx, const SessionState* session)
{
    if (!session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid session state\n", __FUNCTION__, __LINE__);
        return;
    }
    
    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
            "[%s:%d] Upload failed after %d direct attempts and %d codebig attempts\n", 
            __FUNCTION__, __LINE__, session->direct_attempts, session->codebig_attempts);
    
    // Send telemetry for failed upload (matches script t2CountNotify)
    report_upload_failure(session);
    
    // Send failure events (matches script behavior)  
    send_iarm_event("LogUploadEvent", LOG_UPLOAD_FAILED);
    
    // Send maintenance event only if device is not broadband and maintenance enabled
    if (!is_device_broadband(ctx) && is_maintenance_enabled()) {
        send_iarm_event_maintenance(MAINT_LOGUPLOAD_ERROR);
    }
}

void emit_upload_aborted(void)
{
    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
            "[%s:%d] Upload operation was aborted\n", __FUNCTION__, __LINE__);
    
    // Send abort events
    send_iarm_event("LogUploadEvent", LOG_UPLOAD_ABORTED);
    send_iarm_event_maintenance(MAINT_LOGUPLOAD_ERROR);
}

void emit_fallback(UploadPath from_path, UploadPath to_path)
{
    const char* from_str = (from_path == PATH_DIRECT) ? "Direct" : "CodeBig";
    const char* to_str = (to_path == PATH_DIRECT) ? "Direct" : "CodeBig";
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Upload fallback: switching from %s to %s path\n", 
            __FUNCTION__, __LINE__, from_str, to_str);
    
    // Note: Script doesn't send specific fallback events, just logs the switch
}

void emit_upload_start(void)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Starting upload operation\n", __FUNCTION__, __LINE__);
    
    // Note: MAINT_LOGUPLOAD_INPROGRESS is sent in different contexts:
    // 1. When lock acquisition fails (handled in main())
    // 2. During normal upload start (here) - but script doesn't send this here
    // Script only sends MAINT_LOGUPLOAD_INPROGRESS on lock failure, not normal start
}

void send_iarm_event(const char* event_name, int event_code)
{
    if (!event_name) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid event name\n", __FUNCTION__, __LINE__);
        return;
    }
    
    // Determine IARM binary location (matches script conditional logic)
    const char* iarm_binary_path = get_iarm_binary_location();
    
    // Check if IARM event sender binary exists (matches script behavior)
    if (access(iarm_binary_path, F_OK) != 0) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] IARM event sender not found: %s\n", 
                __FUNCTION__, __LINE__, iarm_binary_path);
        return;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Sending IARM event: %s %d\n", 
            __FUNCTION__, __LINE__, event_name, event_code);
    
    // Fork and exec IARM_event_sender (matches script behavior exactly)
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - convert event_code to string
        char event_code_str[16];
        snprintf(event_code_str, sizeof(event_code_str), "%d", event_code);
        
        execl(iarm_binary_path, "IARM_event_sender", event_name, event_code_str, (char*)NULL);
        
        // If exec fails, exit child
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to exec IARM_event_sender\n", __FUNCTION__, __LINE__);
        _exit(1);
    } else if (pid > 0) {
        // Parent process - wait for child to complete
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] IARM event sent successfully\n", __FUNCTION__, __LINE__);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] IARM event sender returned non-zero status\n", __FUNCTION__, __LINE__);
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to fork for IARM event sender\n", __FUNCTION__, __LINE__);
    }
}

void emit_folder_missing_error(void)
{
    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
            "[%s:%d] Required folder missing for log upload\n", __FUNCTION__, __LINE__);
    
    // Send maintenance error event (matches script behavior)
    send_iarm_event_maintenance(MAINT_LOGUPLOAD_ERROR);
}

void send_iarm_event_maintenance(int maint_event_code)
{
    // Send maintenance manager event (matches script behavior)
    send_iarm_event("MaintenanceMGR", maint_event_code);
}
