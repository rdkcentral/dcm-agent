/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#ifndef SYS_INTEGRATION_H
#define SYS_INTEGRATION_H

#include "backup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send systemd notification
 * 
 * @param message Notification message to send
 * @return int BACKUP_SUCCESS on success, error code on failure
 */
int sys_send_systemd_notification(const char* message);


#ifdef __cplusplus
}
#endif

#endif /* SYS_INTEGRATION_H */
