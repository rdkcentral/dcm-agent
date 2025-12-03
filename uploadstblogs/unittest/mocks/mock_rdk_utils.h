/*
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOCK_RDK_UTILS_H
#define MOCK_RDK_UTILS_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stddef.h>

// Define UTILS constants directly (avoiding common_device_api.h dependency)
#define UTILS_SUCCESS 1
#define UTILS_FAIL -1

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations (excluding rdk_logger_init to avoid conflict)
int getIncludePropertyData(const char* property, char* value, int size);
int getDevicePropertyData(const char* property, char* value, int size);

#ifdef __cplusplus
}
#endif

// GetEstbMac with C++ linkage
size_t GetEstbMac(char* mac_buf, size_t buf_size);

// Mock class for RDK utility functions
class MockRdkUtils {
public:
    MOCK_METHOD3(getIncludePropertyData, int(const char* property, char* value, int size));
    MOCK_METHOD3(getDevicePropertyData, int(const char* property, char* value, int size));
    MOCK_METHOD2(GetEstbMac, size_t(char* mac_buf, size_t buf_size));
    MOCK_METHOD1(rdk_logger_init, int(const char* debug_ini));
};

// Global mock instance
extern MockRdkUtils* g_mockRdkUtils;

#endif /* MOCK_RDK_UTILS_H */