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
 */

#include "mock_rdk_utils.h"
#include <cstring>

// Function declarations (avoiding common_device_api.h dependency)
extern "C" {
    int getIncludePropertyData(const char* property, char* value, int size);
    int getDevicePropertyData(const char* property, char* value, int size);
}

// GetEstbMac is declared with C++ linkage to match the original
size_t GetEstbMac(char* mac_buf, size_t buf_size);

// Global mock instance
MockRdkUtils* g_mockRdkUtils = nullptr;

extern "C" {

// Mock implementations that delegate to the global mock object
int getIncludePropertyData(const char* property, char* value, int size) {
    if (g_mockRdkUtils) {
        return g_mockRdkUtils->getIncludePropertyData(property, value, size);
    }
    return UTILS_FAIL;
}

int getDevicePropertyData(const char* property, char* value, int size) {
    if (g_mockRdkUtils) {
        return g_mockRdkUtils->getDevicePropertyData(property, value, size);
    }
    return UTILS_FAIL;
}

}

// GetEstbMac needs to be outside extern "C" since it's declared with C++ linkage in common_device_api.h
size_t GetEstbMac(char* mac_buf, size_t buf_size) {
    if (g_mockRdkUtils) {
        return g_mockRdkUtils->GetEstbMac(mac_buf, buf_size);
    }
    return 0;
}
