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

#ifndef CONFIG_MANAGER_TEST_MOCKS_H
#define CONFIG_MANAGER_TEST_MOCKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Only define things not already defined in real headers
#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 0
#endif

// Forward declarations only - actual definitions come from real headers
struct backup_config_t;

// Mock function declarations - these will be wrapped
void RDK_LOG(int level, const char* module, const char* format, ...);
int getIncludePropertyData(const char* property, char* value, int size);
int getDevicePropertyData(const char* property, char* value, int size);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_TEST_MOCKS_H
