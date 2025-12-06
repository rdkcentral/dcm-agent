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
 * @file telemetry.h
 * @brief Telemetry and metrics reporting
 *
 * This module handles telemetry data collection and reporting via
 * Telemetry 2.0 API.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "uploadstblogs_types.h"

#ifdef T2_EVENT_ENABLED
#include <telemetry_busmessage_sender.h>
#endif

/**
 * @brief Report upload success telemetry
 * @param session Session state
 */
void report_upload_success(const SessionState* session);

/**
 * @brief Report upload failure telemetry
 * @param session Session state
 */
void report_upload_failure(const SessionState* session);

/**
 * @brief Report DRI upload telemetry
 * Script sends SYST_INFO_PDRILogUpload for DRI logs (not RRD)
 */
void report_dri_upload(void);

/**
 * @brief Report certificate error telemetry
 * @param error_code Certificate error code
 * @param fqdn Fully qualified domain name (optional, can be NULL)
 */
void report_cert_error(int error_code, const char* fqdn);

/**
 * @brief Report curl error telemetry
 * @param curl_code Curl error code
 */
void report_curl_error(int curl_code);

/**
 * @brief Report upload attempt telemetry
 */
void report_upload_attempt(void);

/**
 * @brief Report mTLS usage telemetry
 */
void report_mtls_usage(void);

/**
 * @brief Initialize telemetry system
 * Called during application startup
 */
void telemetry_init(void);

/**
 * @brief Uninitialize telemetry system
 * Called during application shutdown
 */
void telemetry_uninit(void);

/**
 * @brief Send telemetry count notification (equivalent to t2CountNotify)
 * @param marker_name Telemetry marker name
 */
void t2_count_notify(const char* marker_name);

/**
 * @brief Send telemetry value notification (equivalent to t2ValNotify)
 * @param marker_name Telemetry marker name
 * @param value Telemetry value
 */
void t2_val_notify(const char* marker_name, const char* value);

#endif /* TELEMETRY_H */
