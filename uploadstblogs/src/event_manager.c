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
#include "rdk_debug.h"
#ifndef GTEST_ENABLE
#include "system_utils.h"
#endif

#if defined(IARM_ENABLED)
#include "libIBus.h"
#include "sysMgr.h"

static bool iarm_initialized = false;
#define IARM_UPLOADSTB_EVENT "UploadSTBLogsEvent"

// Define log upload system state ID if not defined in sysMgr.h
#ifndef IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD
#define IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD 10
#endif
#endif

// Event constants matching script behavior
#define LOG_UPLOAD_SUCCESS 0
#define LOG_UPLOAD_FAILED 1 
#define LOG_UPLOAD_ABORTED 2


// Check device type (matches script DEVICE_TYPE check)
static bool is_device_broadband(const RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    return (strcmp(ctx->device_type, "broadband") == 0);
}

void emit_privacy_abort(void)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Upload aborted due to privacy mode\n", __FUNCTION__, __LINE__);
    
}

void emit_no_logs_reboot(const RuntimeContext* ctx)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Log directory empty, skipping log upload\n", __FUNCTION__, __LINE__);
    
    // Check for null context first
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid runtime context\n", __FUNCTION__, __LINE__);
        return;
    }
    
}

void emit_no_logs_ondemand(void)
{
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Log directory empty, skipping log upload\n", __FUNCTION__, __LINE__);
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
    t2_count_notify("SYST_INFO_lu_success");
    
    // Skip IARM events for uploadLogNow case
    if (ctx && ctx->uploadlogsnow_mode) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Skipping IARM events for uploadLogNow mode\n", __FUNCTION__, __LINE__);
        return;
    }
    
    // Send success events (matches script behavior)
    send_iarm_event("LogUploadEvent", LOG_UPLOAD_SUCCESS);
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
    t2_count_notify("SYST_ERR_LogUpload_Failed");
    
    // Skip IARM events for uploadLogNow case
    if (ctx && ctx->uploadlogsnow_mode) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] Skipping IARM events for uploadLogNow mode\n", __FUNCTION__, __LINE__);
        return;
    }
    
    // Send failure events (matches script behavior)  
    send_iarm_event("LogUploadEvent", LOG_UPLOAD_FAILED);
    
}

void emit_upload_aborted(void)
{
    RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
            "[%s:%d] Not Uploading Logs with DCM \n", __FUNCTION__, __LINE__);
    
    send_iarm_event("LogUploadEvent", LOG_UPLOAD_FAILED);
}

#ifndef GTEST_ENABLE
#if defined(IARM_ENABLED)

/**
 * @brief Initialize IARM connection for event management
 * Based on rdkfwupdater iarmInterface.c init_event_handler()
 */
static bool init_iarm_connection(void)
{
    IARM_Result_t res;
    int isRegistered = 0;
    
    if (iarm_initialized) {
        return true;
    }
    
    // Check if already connected
    res = IARM_Bus_IsConnected(IARM_UPLOADSTB_EVENT, &isRegistered);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] IARM_Bus_IsConnected: %d (registered: %d)\n", 
            __FUNCTION__, __LINE__, res, isRegistered);
    
    if (isRegistered == 1) {
        iarm_initialized = true;
        return true;
    }
    
    // Initialize IARM bus
    res = IARM_Bus_Init(IARM_UPLOADSTB_EVENT);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] IARM_Bus_Init: %d\n", __FUNCTION__, __LINE__, res);
    
    if (res == IARM_RESULT_SUCCESS || res == IARM_RESULT_INVALID_STATE) {
        // Connect to IARM bus
        res = IARM_Bus_Connect();
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                "[%s:%d] IARM_Bus_Connect: %d\n", __FUNCTION__, __LINE__, res);
        
        if (res == IARM_RESULT_SUCCESS || res == IARM_RESULT_INVALID_STATE) {
            // Verify connection
            res = IARM_Bus_IsConnected(IARM_UPLOADSTB_EVENT, &isRegistered);
            if (isRegistered == 1) {
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                        "[%s:%d] IARM connection established successfully\n", __FUNCTION__, __LINE__);
                iarm_initialized = true;
                return true;
            }
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] IARM_Bus_Connect failure: %d\n", __FUNCTION__, __LINE__, res);
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] IARM_Bus_Init failure: %d\n", __FUNCTION__, __LINE__, res);
    }
    
    return false;
}

/**
 * @brief Send IARM system state event
 * Based on rdkfwupdater iarmInterface.c eventManager()
 */
void send_iarm_event(const char* event_name, int event_code)
{
    if (!event_name) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid event name\n", __FUNCTION__, __LINE__);
        return;
    }
    
    if (!init_iarm_connection()) {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                "[%s:%d] IARM not initialized, skipping event: %s\n", 
                __FUNCTION__, __LINE__, event_name);
        return;
    }
    
    IARM_Bus_SYSMgr_EventData_t event_data;
    IARM_Result_t ret_code = IARM_RESULT_SUCCESS;
    bool event_sent = false;
    
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Sending IARM event: %s with code: %d\n", 
            __FUNCTION__, __LINE__, event_name, event_code);
    
    // Map log upload events to IARM system states
    if (strcmp(event_name, "LogUploadEvent") == 0) {
        // Map log upload status to appropriate system state
        switch (event_code) {
            case LOG_UPLOAD_SUCCESS:
                event_data.data.systemStates.stateId = IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD;
                event_data.data.systemStates.state = 0; // Success
                event_sent = true;
                break;
            case LOG_UPLOAD_FAILED:
                event_data.data.systemStates.stateId = IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD;
                event_data.data.systemStates.state = 1; // Failure
                event_sent = true;
                break;
            case LOG_UPLOAD_ABORTED:
                event_data.data.systemStates.stateId = IARM_BUS_SYSMGR_SYSSTATE_LOG_UPLOAD;
                event_data.data.systemStates.state = 2; // Aborted
                event_sent = true;
                break;
            default:
                RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                        "[%s:%d] Unknown log upload event code: %d\n", 
                        __FUNCTION__, __LINE__, event_code);
                break;
        }
    }
    
    if (event_sent) {
        event_data.data.systemStates.error = 0;
        ret_code = IARM_Bus_BroadcastEvent(IARM_BUS_SYSMGR_NAME, 
                                          (IARM_EventId_t)IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, 
                                          (void*)&event_data, sizeof(event_data));
        
        if (ret_code == IARM_RESULT_SUCCESS) {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
                    "[%s:%d] IARM system event sent successfully: %s\n", 
                    __FUNCTION__, __LINE__, event_name);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] IARM system event failed: %s (result: %d)\n", 
                    __FUNCTION__, __LINE__, event_name, ret_code);
        }
    }
}

/**
 * @brief Cleanup IARM connection
 * Based on rdkfwupdater iarmrInterface.c term_event_handler()
 */
void cleanup_iarm_connection(void)
{
    if (iarm_initialized) {
        IARM_Bus_Disconnect();
        IARM_Bus_Term();
        iarm_initialized = false;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] IARM connection cleaned up\n", __FUNCTION__, __LINE__);
    }
}

#else
// IARM disabled - provide stub implementations
void send_iarm_event(const char* event_name, int event_code)
{
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] IARM disabled - would send event: %s %d\n", 
            __FUNCTION__, __LINE__, event_name ? event_name : "NULL", event_code);
}

void cleanup_iarm_connection(void)
{
    // No-op when IARM disabled
}
#endif
#endif

void emit_folder_missing_error(void)
{
    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
            "[%s:%d] Required folder missing for log upload\n", __FUNCTION__, __LINE__);
}

