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
 * @file event_manager.h
 * @brief Event emission and notification handling
 *
 * This module handles emission of IARM events and other notifications
 * for upload lifecycle events.
 */

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "uploadstblogs_types.h"

/**
 * @brief Emit privacy abort event
 */
void emit_privacy_abort(void);

/**
 * @brief Emit no logs event for reboot strategy
 * Script: uploadLogOnReboot lines 809-814 (DEVICE_TYPE != broadband && ENABLE_MAINTENANCE)
 * @param ctx Runtime context
 */
void emit_no_logs_reboot(const RuntimeContext* ctx);

/**
 * @brief Emit no logs event for ondemand strategy  
 * Script: uploadLogOnDemand lines 746-750 (only ENABLE_MAINTENANCE)
 */
void emit_no_logs_ondemand(void);

/**
 * @brief Emit upload success event
 * @param ctx Runtime context
 * @param session Session state
 */
void emit_upload_success(const RuntimeContext* ctx, const SessionState* session);

/**
 * @brief Emit upload failure event
 * @param ctx Runtime context
 * @param session Session state
 */
void emit_upload_failure(const RuntimeContext* ctx, const SessionState* session);

/**
 * @brief Emit upload aborted event
 */
void emit_upload_aborted(void);

/**
 * @brief Emit upload start event
 */
void emit_upload_start(void);

/**
 * @brief Emit fallback event
 * @param from_path Original path
 * @param to_path Fallback path
 */
void emit_fallback(UploadPath from_path, UploadPath to_path);

/**
 * @brief Send IARM event
 * @param event_name Event name (e.g., "LogUploadEvent")
 * @param event_code Event code
 */
void send_iarm_event(const char* event_name, int event_code);

/**
 * @brief Send maintenance manager IARM event
 * @param maint_event_code Maintenance event code
 */
void send_iarm_event_maintenance(int maint_event_code);

/**
 * @brief Cleanup IARM connection resources
 * Should be called during application shutdown
 */
void cleanup_iarm_connection(void);

/**
 * @brief Emit folder missing error event
 */
void emit_folder_missing_error(void);

#endif /* EVENT_MANAGER_H */
