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

#ifndef MOCK_RBUS_H
#define MOCK_RBUS_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// RBUS function declarations
bool rbus_init();
void rbus_cleanup();
bool rbus_get_string_param(const char* param, char* value, size_t size);
bool rbus_get_bool_param(const char* param, bool* value);

// RBUS error codes
typedef enum {
    RBUS_ERROR_SUCCESS = 0,
    RBUS_ERROR_BUS_ERROR,
    RBUS_ERROR_INVALID_INPUT,
    RBUS_ERROR_NOT_INITIALIZED,
    RBUS_ERROR_DESTINATION_NOT_FOUND
} rbusError_t;

// Mock class for RBUS functions
class MockRbus {
public:
    MOCK_METHOD0(rbus_init, bool());
    MOCK_METHOD0(rbus_cleanup, void());
    MOCK_METHOD3(rbus_get_string_param, bool(const char* param, char* value, size_t size));
    MOCK_METHOD2(rbus_get_bool_param, bool(const char* param, bool* value));
};

// Global mock instance
extern MockRbus* g_mockRbus;

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RBUS_H */
