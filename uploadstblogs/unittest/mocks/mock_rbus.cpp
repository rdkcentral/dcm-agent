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

#include "mock_rbus.h"
#include <cstring>

// Global mock instance
MockRbus* g_mockRbus = nullptr;

extern "C" {

// Mock implementations that delegate to the global mock object
bool rbus_init() {
    if (g_mockRbus) {
        return g_mockRbus->rbus_init();
    }
    return false;
}

void rbus_cleanup() {
    if (g_mockRbus) {
        g_mockRbus->rbus_cleanup();
    }
}

bool rbus_get_string_param(const char* param, char* value, size_t size) {
    if (g_mockRbus) {
        return g_mockRbus->rbus_get_string_param(param, value, size);
    }
    return false;
}

bool rbus_get_bool_param(const char* param, bool* value) {
    if (g_mockRbus) {
        return g_mockRbus->rbus_get_bool_param(param, value);
    }
    return false;
}

}