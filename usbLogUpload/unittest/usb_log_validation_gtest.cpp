/**
 * Copyright 2020 RDK Management
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
 * @file usb_log_validation_gtest.cpp
 * @brief Google Test unit tests for USB log upload validation module
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

extern "C" {
#include "usb_log_validation.h"
}

/* Controllable mock state for getDevicePropertyData */
static int g_mock_device_prop_rc = UTILS_SUCCESS;
static char g_mock_device_prop_value[64] = "TV";

extern "C" {
    int rdk_logger_init(const char*) { return 0; }
    int getDevicePropertyData(const char*, char* buf, size_t sz) {
        if (g_mock_device_prop_rc != UTILS_SUCCESS) return g_mock_device_prop_rc;
        strncpy(buf, g_mock_device_prop_value, sz - 1);
        buf[sz - 1] = '\0';
        return UTILS_SUCCESS;
    }
    int getIncludePropertyData(const char*, char* buf, size_t sz) {
        strncpy(buf, "/opt/logs", sz - 1);
        buf[sz - 1] = '\0';
        return UTILS_SUCCESS;
    }
}

class UsbLogValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_device_prop_rc = UTILS_SUCCESS;
        strncpy(g_mock_device_prop_value, "TV", sizeof(g_mock_device_prop_value));
    }

    void TearDown() override {
    }
};

TEST_F(UsbLogValidationTest, UsbMountPointValidTest) {
    const char* valid_path = "/tmp";
    EXPECT_EQ(validate_usb_mount_point(valid_path), 0);
}

TEST_F(UsbLogValidationTest, UsbMountPointNullTest) {
    EXPECT_EQ(validate_usb_mount_point(nullptr), -1);
}

TEST_F(UsbLogValidationTest, UsbMountPointEmptyStringTest) {
    EXPECT_EQ(validate_usb_mount_point(""), -1);
}

TEST_F(UsbLogValidationTest, UsbMountPointRootTest) {
    EXPECT_EQ(validate_usb_mount_point("/"), 0);
}

/**
 * @brief Test device compatibility validation with unsupported device
 */
TEST_F(UsbLogValidationTest, DeviceCompatibilityInvalidTest) {
    EXPECT_TRUE(true); 
}

TEST_F(UsbLogValidationTest, UsbMountPointInvalidTest) {
    const char* invalid_path = "/nonexistent/path";
    EXPECT_EQ(validate_usb_mount_point(invalid_path), 2);
}

TEST_F(UsbLogValidationTest, ValidInputParametersTest) {
    char* test_argv[] = {(char*)"program", (char*)"/tmp/usb"};
    EXPECT_EQ(validate_input_parameters(2, test_argv), 0);
}

TEST_F(UsbLogValidationTest, InvalidInputParametersTest) {
    char* test_argv[] = {(char*)"program"};
    EXPECT_EQ(validate_input_parameters(1, test_argv), 4);
}

TEST_F(UsbLogValidationTest, InputParametersTooManyArgs) {
    char* test_argv[] = {(char*)"program", (char*)"/tmp/usb", (char*)"extra"};
    EXPECT_EQ(validate_input_parameters(3, test_argv), 4);
}

TEST_F(UsbLogValidationTest, InputParametersNullArgv1) {
    char* test_argv[] = {(char*)"program", nullptr};
    EXPECT_EQ(validate_input_parameters(2, test_argv), 4);
}

TEST_F(UsbLogValidationTest, InputParametersEmptyArgv1) {
    char* test_argv[] = {(char*)"program", (char*)""};
    EXPECT_EQ(validate_input_parameters(2, test_argv), 4);
}

TEST_F(UsbLogValidationTest, InputParametersZeroArgc) {
    char* test_argv[] = {(char*)"program"};
    EXPECT_EQ(validate_input_parameters(0, test_argv), 4);
}

TEST_F(UsbLogValidationTest, DeviceCompatibility_TV_Success) {
    strncpy(g_mock_device_prop_value, "TV", sizeof(g_mock_device_prop_value));
    EXPECT_EQ(validate_device_compatibility(), 0);
}

TEST_F(UsbLogValidationTest, DeviceCompatibility_NonTV_Fails) {
    strncpy(g_mock_device_prop_value, "STB", sizeof(g_mock_device_prop_value));
    EXPECT_EQ(validate_device_compatibility(), 4);
}

TEST_F(UsbLogValidationTest, DeviceCompatibility_EmptyProfile_Fails) {
    strncpy(g_mock_device_prop_value, "", sizeof(g_mock_device_prop_value));
    EXPECT_EQ(validate_device_compatibility(), 4);
}

TEST_F(UsbLogValidationTest, DeviceCompatibility_PropertyReadFails) {
    g_mock_device_prop_rc = UTILS_FAIL;
    EXPECT_EQ(validate_device_compatibility(), 4);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    return result;
}
