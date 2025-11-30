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
 * @file rbus_interface.c
 * @brief RBUS interface implementation for TR-181 parameter access
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rbus_interface.h"
#include "rdk_debug.h"

#include "rbus/rbus.h"


#define LOG_UPLOADSTB "LOG.RDK.UPLOADSTB"

// Global RBUS handle - initialized once and reused
static rbusHandle_t g_rbusHandle = NULL;
static bool g_rbusInitialized = false;

bool rbus_init(void)
{
    if (g_rbusInitialized) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] RBUS already initialized\n", __FUNCTION__, __LINE__);
        return true;
    }

    rbusError_t rc = rbus_open(&g_rbusHandle, "UploadSTBLogs");
    if (rc != RBUS_ERROR_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Failed to open RBUS connection: %d\n", 
                __FUNCTION__, __LINE__, rc);
        return false;
    }

    g_rbusInitialized = true;
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] RBUS connection initialized\n", __FUNCTION__, __LINE__);
    return true;
}

void rbus_cleanup(void)
{
    if (g_rbusInitialized && g_rbusHandle != NULL) {
        rbus_close(g_rbusHandle);
        g_rbusHandle = NULL;
        g_rbusInitialized = false;
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, "[%s:%d] RBUS connection closed\n", __FUNCTION__, __LINE__);
    }
}

bool rbus_get_string_param(const char* param_name, char* value_buf, size_t buf_size)
{
    if (!param_name || !value_buf || buf_size == 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    if (!g_rbusInitialized || g_rbusHandle == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] RBUS not initialized, call rbus_init() first\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    rbusValue_t paramValue = NULL;
    rbusError_t rc = RBUS_ERROR_SUCCESS;
    const char* stringValue = NULL;
    bool success = false;

    // Get parameter value using global handle
    rc = rbus_get(g_rbusHandle, param_name, &paramValue);
    if (rc == RBUS_ERROR_SUCCESS && paramValue != NULL) {
        stringValue = rbusValue_GetString(paramValue, NULL);
        if (stringValue != NULL && strlen(stringValue) > 0) {
            strncpy(value_buf, stringValue, buf_size - 1);
            value_buf[buf_size - 1] = '\0';
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] %s=%s\n", 
                    __FUNCTION__, __LINE__, param_name, value_buf);
            success = true;
        }
        rbusValue_Release(paramValue);
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to get %s: %d\n", 
                __FUNCTION__, __LINE__, param_name, rc);
    }

    return success;
}

bool rbus_get_bool_param(const char* param_name, bool* value)
{
    if (!param_name || !value) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    if (!g_rbusInitialized || g_rbusHandle == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] RBUS not initialized, call rbus_init() first\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    rbusValue_t paramValue = NULL;
    rbusError_t rc = RBUS_ERROR_SUCCESS;
    bool success = false;

    // Get parameter value using global handle
    rc = rbus_get(g_rbusHandle, param_name, &paramValue);
    if (rc == RBUS_ERROR_SUCCESS && paramValue != NULL) {
        *value = rbusValue_GetBoolean(paramValue);
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] %s=%s\n", 
                __FUNCTION__, __LINE__, param_name, *value ? "true" : "false");
        rbusValue_Release(paramValue);
        success = true;
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to get %s: %d\n", 
                __FUNCTION__, __LINE__, param_name, rc);
    }

    return success;
}

bool rbus_get_int_param(const char* param_name, int* value)
{
    if (!param_name || !value) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }

    if (!g_rbusInitialized || g_rbusHandle == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, "[%s:%d] RBUS not initialized, call rbus_init() first\n", 
                __FUNCTION__, __LINE__);
        return false;
    }

    rbusValue_t paramValue = NULL;
    rbusError_t rc = RBUS_ERROR_SUCCESS;
    bool success = false;

    // Get parameter value using global handle
    rc = rbus_get(g_rbusHandle, param_name, &paramValue);
    if (rc == RBUS_ERROR_SUCCESS && paramValue != NULL) {
        *value = rbusValue_GetInt32(paramValue);
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, "[%s:%d] %s=%d\n", 
                __FUNCTION__, __LINE__, param_name, *value);
        rbusValue_Release(paramValue);
        success = true;
    } else {
        RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, "[%s:%d] Failed to get %s: %d\n", 
                __FUNCTION__, __LINE__, param_name, rc);
    }

    return success;
}
