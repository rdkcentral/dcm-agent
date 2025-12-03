/**
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

// Mock external dependencies
extern "C" {
// Mock RBUS API functions
#ifdef GTEST_ENABLE
typedef void* rbusHandle_t;
typedef void* rbusValue_t;

typedef enum {
    RBUS_ERROR_SUCCESS = 0,
    RBUS_ERROR_BUS_ERROR,
    RBUS_ERROR_INVALID_INPUT,
    RBUS_ERROR_NOT_INITIALIZED,
    RBUS_ERROR_DESTINATION_NOT_FOUND
} rbusError_t;

// Mock functions
rbusError_t rbus_open(rbusHandle_t* handle, const char* componentName);
rbusError_t rbus_close(rbusHandle_t handle);
rbusError_t rbus_get(rbusHandle_t handle, const char* paramName, rbusValue_t* value);
const char* rbusValue_GetString(rbusValue_t value, int* len);
bool rbusValue_GetBoolean(rbusValue_t value);
int rbusValue_GetInt32(rbusValue_t value);
void rbusValue_Release(rbusValue_t value);

// Mock state
static rbusError_t mock_rbus_open_result = RBUS_ERROR_SUCCESS;
static rbusError_t mock_rbus_get_result = RBUS_ERROR_SUCCESS;
static const char* mock_string_value = "test_value";
static bool mock_bool_value = true;
static int mock_int_value = 42;
static bool mock_rbus_initialized = false;

// Mock implementations
rbusError_t rbus_open(rbusHandle_t* handle, const char* componentName) {
    if (mock_rbus_open_result == RBUS_ERROR_SUCCESS) {
        *handle = (rbusHandle_t)0x1234; // Dummy non-null handle
        mock_rbus_initialized = true;
    } else {
        *handle = NULL;
    }
    return mock_rbus_open_result;
}

rbusError_t rbus_close(rbusHandle_t handle) {
    mock_rbus_initialized = false;
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbus_get(rbusHandle_t handle, const char* paramName, rbusValue_t* value) {
    if (mock_rbus_get_result == RBUS_ERROR_SUCCESS) {
        *value = (rbusValue_t)0x5678; // Dummy non-null value
    } else {
        *value = NULL;
    }
    return mock_rbus_get_result;
}

const char* rbusValue_GetString(rbusValue_t value, int* len) {
    if (len) *len = strlen(mock_string_value);
    return mock_string_value;
}

bool rbusValue_GetBoolean(rbusValue_t value) {
    return mock_bool_value;
}

int rbusValue_GetInt32(rbusValue_t value) {
    return mock_int_value;
}

void rbusValue_Release(rbusValue_t value) {
    // No-op for mock
}
#endif
}

// Include the actual rbus interface implementation
#include "rbus_interface.h"
#include "../src/rbus_interface.c"

using namespace testing;

class RbusInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        mock_rbus_open_result = RBUS_ERROR_SUCCESS;
        mock_rbus_get_result = RBUS_ERROR_SUCCESS;
        mock_string_value = "test_value";
        mock_bool_value = true;
        mock_int_value = 42;
        mock_rbus_initialized = false;
        
        // Reset global RBUS state
        rbus_cleanup();
        
        strcpy(test_string_buffer, "");
    }
    
    void TearDown() override {
        rbus_cleanup();
    }
    
    char test_string_buffer[256];
    bool test_bool_value;
    int test_int_value;
};

// Test rbus_init function
TEST_F(RbusInterfaceTest, RbusInit_Success) {
    EXPECT_TRUE(rbus_init());
}

TEST_F(RbusInterfaceTest, RbusInit_Failure) {
    mock_rbus_open_result = RBUS_ERROR_BUS_ERROR;
    EXPECT_FALSE(rbus_init());
}

TEST_F(RbusInterfaceTest, RbusInit_AlreadyInitialized) {
    // First initialization
    EXPECT_TRUE(rbus_init());
    
    // Second initialization should return true (already initialized)
    EXPECT_TRUE(rbus_init());
}

// Test rbus_cleanup function
TEST_F(RbusInterfaceTest, RbusCleanup_WhenInitialized) {
    // Initialize first
    EXPECT_TRUE(rbus_init());
    
    // Cleanup should work without errors
    rbus_cleanup();
    
    // Can be called multiple times safely
    rbus_cleanup();
}

TEST_F(RbusInterfaceTest, RbusCleanup_WhenNotInitialized) {
    // Cleanup when not initialized should be safe
    rbus_cleanup();
}

// Test rbus_get_string_param function
TEST_F(RbusInterfaceTest, GetStringParam_Success) {
    EXPECT_TRUE(rbus_init());
    
    mock_string_value = "Device.DeviceInfo.SoftwareVersion";
    EXPECT_TRUE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                      test_string_buffer, sizeof(test_string_buffer)));
    EXPECT_STREQ(test_string_buffer, "Device.DeviceInfo.SoftwareVersion");
}

TEST_F(RbusInterfaceTest, GetStringParam_NotInitialized) {
    // Don't call rbus_init()
    EXPECT_FALSE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                       test_string_buffer, sizeof(test_string_buffer)));
}

TEST_F(RbusInterfaceTest, GetStringParam_NullParameters) {
    EXPECT_TRUE(rbus_init());
    
    // Null param_name
    EXPECT_FALSE(rbus_get_string_param(nullptr, test_string_buffer, sizeof(test_string_buffer)));
    
    // Null value_buf
    EXPECT_FALSE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", nullptr, sizeof(test_string_buffer)));
    
    // Zero buf_size
    EXPECT_FALSE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", test_string_buffer, 0));
}

TEST_F(RbusInterfaceTest, GetStringParam_RbusGetFailure) {
    EXPECT_TRUE(rbus_init());
    
    mock_rbus_get_result = RBUS_ERROR_DESTINATION_NOT_FOUND;
    EXPECT_FALSE(rbus_get_string_param("Device.Invalid.Parameter", 
                                       test_string_buffer, sizeof(test_string_buffer)));
}

TEST_F(RbusInterfaceTest, GetStringParam_BufferTruncation) {
    EXPECT_TRUE(rbus_init());
    
    mock_string_value = "This is a very long string that should be truncated";
    char small_buffer[10];
    
    EXPECT_TRUE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                      small_buffer, sizeof(small_buffer)));
    
    // Should be truncated and null-terminated
    EXPECT_EQ(strlen(small_buffer), sizeof(small_buffer) - 1);
    EXPECT_EQ(small_buffer[sizeof(small_buffer) - 1], '\0');
}

// Test rbus_get_bool_param function
TEST_F(RbusInterfaceTest, GetBoolParam_Success) {
    EXPECT_TRUE(rbus_init());
    
    mock_bool_value = true;
    EXPECT_TRUE(rbus_get_bool_param("Device.DeviceInfo.UploadEnable", &test_bool_value));
    EXPECT_TRUE(test_bool_value);
    
    mock_bool_value = false;
    EXPECT_TRUE(rbus_get_bool_param("Device.DeviceInfo.UploadEnable", &test_bool_value));
    EXPECT_FALSE(test_bool_value);
}

TEST_F(RbusInterfaceTest, GetBoolParam_NotInitialized) {
    // Don't call rbus_init()
    EXPECT_FALSE(rbus_get_bool_param("Device.DeviceInfo.UploadEnable", &test_bool_value));
}

TEST_F(RbusInterfaceTest, GetBoolParam_NullParameters) {
    EXPECT_TRUE(rbus_init());
    
    // Null param_name
    EXPECT_FALSE(rbus_get_bool_param(nullptr, &test_bool_value));
    
    // Null value
    EXPECT_FALSE(rbus_get_bool_param("Device.DeviceInfo.UploadEnable", nullptr));
}

TEST_F(RbusInterfaceTest, GetBoolParam_RbusGetFailure) {
    EXPECT_TRUE(rbus_init());
    
    mock_rbus_get_result = RBUS_ERROR_DESTINATION_NOT_FOUND;
    EXPECT_FALSE(rbus_get_bool_param("Device.Invalid.Parameter", &test_bool_value));
}

// Test rbus_get_int_param function
TEST_F(RbusInterfaceTest, GetIntParam_Success) {
    EXPECT_TRUE(rbus_init());
    
    mock_int_value = 100;
    EXPECT_TRUE(rbus_get_int_param("Device.DeviceInfo.LogUploadInterval", &test_int_value));
    EXPECT_EQ(test_int_value, 100);
    
    mock_int_value = -50;
    EXPECT_TRUE(rbus_get_int_param("Device.DeviceInfo.LogUploadInterval", &test_int_value));
    EXPECT_EQ(test_int_value, -50);
}

TEST_F(RbusInterfaceTest, GetIntParam_NotInitialized) {
    // Don't call rbus_init()
    EXPECT_FALSE(rbus_get_int_param("Device.DeviceInfo.LogUploadInterval", &test_int_value));
}

TEST_F(RbusInterfaceTest, GetIntParam_NullParameters) {
    EXPECT_TRUE(rbus_init());
    
    // Null param_name
    EXPECT_FALSE(rbus_get_int_param(nullptr, &test_int_value));
    
    // Null value
    EXPECT_FALSE(rbus_get_int_param("Device.DeviceInfo.LogUploadInterval", nullptr));
}

TEST_F(RbusInterfaceTest, GetIntParam_RbusGetFailure) {
    EXPECT_TRUE(rbus_init());
    
    mock_rbus_get_result = RBUS_ERROR_DESTINATION_NOT_FOUND;
    EXPECT_FALSE(rbus_get_int_param("Device.Invalid.Parameter", &test_int_value));
}

// Integration tests
TEST_F(RbusInterfaceTest, Integration_MultipleParameterRetrieval) {
    EXPECT_TRUE(rbus_init());
    
    // Get string parameter
    mock_string_value = "1.0.0";
    EXPECT_TRUE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                      test_string_buffer, sizeof(test_string_buffer)));
    EXPECT_STREQ(test_string_buffer, "1.0.0");
    
    // Get bool parameter
    mock_bool_value = true;
    EXPECT_TRUE(rbus_get_bool_param("Device.DeviceInfo.UploadEnable", &test_bool_value));
    EXPECT_TRUE(test_bool_value);
    
    // Get int parameter
    mock_int_value = 3600;
    EXPECT_TRUE(rbus_get_int_param("Device.DeviceInfo.LogUploadInterval", &test_int_value));
    EXPECT_EQ(test_int_value, 3600);
}

TEST_F(RbusInterfaceTest, Integration_InitCleanupCycle) {
    // Multiple init/cleanup cycles
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(rbus_init());
        
        mock_string_value = "test";
        EXPECT_TRUE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                          test_string_buffer, sizeof(test_string_buffer)));
        
        rbus_cleanup();
        
        // After cleanup, should not be able to get parameters
        EXPECT_FALSE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                           test_string_buffer, sizeof(test_string_buffer)));
    }
}

// Error handling tests
TEST_F(RbusInterfaceTest, ErrorHandling_RbusErrors) {
    EXPECT_TRUE(rbus_init());
    
    // Test various RBUS error codes
    rbusError_t error_codes[] = {
        RBUS_ERROR_BUS_ERROR,
        RBUS_ERROR_INVALID_INPUT,
        RBUS_ERROR_NOT_INITIALIZED,
        RBUS_ERROR_DESTINATION_NOT_FOUND
    };
    
    for (rbusError_t error_code : error_codes) {
        mock_rbus_get_result = error_code;
        
        EXPECT_FALSE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                           test_string_buffer, sizeof(test_string_buffer)));
        EXPECT_FALSE(rbus_get_bool_param("Device.DeviceInfo.UploadEnable", &test_bool_value));
        EXPECT_FALSE(rbus_get_int_param("Device.DeviceInfo.LogUploadInterval", &test_int_value));
    }
}

TEST_F(RbusInterfaceTest, ErrorHandling_EmptyStringValue) {
    EXPECT_TRUE(rbus_init());
    
    // Test empty string value
    mock_string_value = "";
    EXPECT_FALSE(rbus_get_string_param("Device.DeviceInfo.SoftwareVersion", 
                                       test_string_buffer, sizeof(test_string_buffer)));
}

// Real-world TR-181 parameter tests
TEST_F(RbusInterfaceTest, RealWorldParameters_CommonTR181) {
    EXPECT_TRUE(rbus_init());
    
    // Test common TR-181 parameters
    const char* tr181_params[] = {
        "Device.DeviceInfo.SoftwareVersion",
        "Device.DeviceInfo.HardwareVersion",
        "Device.DeviceInfo.SerialNumber",
        "Device.DeviceInfo.Manufacturer",
        "Device.DeviceInfo.ManufacturerOUI",
        "Device.DeviceInfo.ModelName"
    };
    
    for (const char* param : tr181_params) {
        mock_string_value = "test_value";
        EXPECT_TRUE(rbus_get_string_param(param, test_string_buffer, sizeof(test_string_buffer))) 
            << "Failed to get parameter: " << param;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}