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
#include <systemd/sd-daemon.h>

#include "sys_integration.h"
#include "backup_types.h"

/* Send systemd notification - C equivalent of /bin/systemd-notify */
int sys_send_systemd_notification(const char* message) {
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Starting systemd notification send\n");
    
    char notification[512];
    int result;
    
    if (!message) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Systemd notification failed: NULL message parameter\n");
        return BACKUP_ERROR_INVALID_PARAM;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Preparing systemd notification with message: '%s'\n", message);
    
    /* Build notification string for sd_notify */
    snprintf(notification, sizeof(notification), "READY=1\nSTATUS=%s", message);
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Built notification string: '%s'\n", notification);
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Sending systemd notification: %s\n", message);
    
    result = sd_notify(0, notification);
    if (result < 0) {
        RDK_LOG(RDK_LOG_ERROR, LOG_BACKUP_LOGS, "Systemd notification failed: sd_notify returned %d\n", result);
        return BACKUP_ERROR_SYSTEM;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_BACKUP_LOGS, "Systemd notification sent successfully (returned %d)\n", result);
    RDK_LOG(RDK_LOG_INFO, LOG_BACKUP_LOGS, "Systemd notification completed successfully\n");
    return BACKUP_SUCCESS;
}
