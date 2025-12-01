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
 * @file telemetry.c
 * @brief Telemetry reporting implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "telemetry.h"
#include "rdk_debug.h"

#ifdef T2_EVENT_ENABLED
#include <telemetry_busmessage_sender.h>
#endif

void telemetry_init(void)
{
#ifdef T2_EVENT_ENABLED
    t2_init("uploadstblogs");
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Telemetry system initialized\n", __FUNCTION__, __LINE__);
#else
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Telemetry disabled (T2_EVENT_ENABLED not defined)\n", __FUNCTION__, __LINE__);
#endif
}

void telemetry_uninit(void)
{
#ifdef T2_EVENT_ENABLED
    t2_uninit();
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Telemetry system uninitialized\n", __FUNCTION__, __LINE__);
#endif
}

void report_upload_success(const SessionState* session)
{
    if (!session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid session state\n", __FUNCTION__, __LINE__);
        return;
    }
    
    // Report success telemetry (matches script t2CountNotify)
    t2_count_notify("SYST_INFO_lu_success");
    
    const char* path_used = session->used_fallback ? "CodeBig" : "Direct";
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported upload success telemetry via %s path\n", 
            __FUNCTION__, __LINE__, path_used);
}

void report_upload_failure(const SessionState* session)
{
    if (!session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid session state\n", __FUNCTION__, __LINE__);
        return;
    }
    
    // Report failure telemetry (matches script t2CountNotify)
    t2_count_notify("SYST_ERR_LogUpload_Failed");
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported upload failure telemetry\n", __FUNCTION__, __LINE__);
}

void report_dri_upload(void)
{
    // Report DRI upload telemetry (matches script line 883, 886)
    // Script sends this marker for BOTH success and failure of DRI upload
    // Note: RRD upload (line 920-936) does NOT send telemetry
    t2_count_notify("SYST_INFO_PDRILogUpload");
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported DRI upload telemetry\n", __FUNCTION__, __LINE__);
}

void report_cert_error(int error_code, const char* fqdn)
{
    // Report certificate error with code and FQDN (matches script line 307)
    // Script: t2ValNotify "certerr_split" "STBLogUL, $TLSRet, $fqdn"
    char error_value[256];
    if (fqdn && fqdn[0] != '\0') {
        snprintf(error_value, sizeof(error_value), "STBLogUL, %d, %s", error_code, fqdn);
    } else {
        snprintf(error_value, sizeof(error_value), "STBLogUL, %d", error_code);
    }
    
    t2_val_notify("certerr_split", error_value);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported certificate error telemetry: %s\n", 
            __FUNCTION__, __LINE__, error_value);
}

void report_curl_error(int curl_code)
{
    // Report curl error (matches script t2ValNotify)
    char curl_value[32];
    snprintf(curl_value, sizeof(curl_value), "%d", curl_code);
    
    t2_val_notify("LUCurlErr_split", curl_value);
    
    // Special handling for timeout errors (matches script)
    if (curl_code == 28) {
        t2_count_notify("SYST_ERR_Curl28");
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported curl error telemetry: code %d\n", 
            __FUNCTION__, __LINE__, curl_code);
}

void report_upload_attempt(void)
{
    // Report upload attempt (matches script t2CountNotify)
    t2_count_notify("SYST_INFO_LUattempt");
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported upload attempt telemetry\n", __FUNCTION__, __LINE__);
}

void report_mtls_usage(void)
{
    // Report mTLS usage (matches script t2CountNotify)
    t2_count_notify("SYST_INFO_mtls_xpki");
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Reported mTLS usage telemetry\n", __FUNCTION__, __LINE__);
}

void t2_count_notify(const char* marker_name)
{
    if (!marker_name) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid marker name\n", __FUNCTION__, __LINE__);
        return;
    }
    
#ifdef T2_EVENT_ENABLED
    t2_event_d((char*)marker_name, 1);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Sent telemetry count: %s\n", __FUNCTION__, __LINE__, marker_name);
#else
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Telemetry disabled (T2_EVENT_ENABLED not defined): %s\n", 
            __FUNCTION__, __LINE__, marker_name);
#endif
}

void t2_val_notify(const char* marker_name, const char* value)
{
    if (!marker_name || !value) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid telemetry parameters\n", __FUNCTION__, __LINE__);
        return;
    }
    
#ifdef T2_EVENT_ENABLED
    t2_event_s((char*)marker_name, (char*)value);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Sent telemetry value: %s = %s\n", 
            __FUNCTION__, __LINE__, marker_name, value);
#else
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB, 
            "[%s:%d] Telemetry disabled (T2_EVENT_ENABLED not defined): %s = %s\n", 
            __FUNCTION__, __LINE__, marker_name, value);
#endif
}


