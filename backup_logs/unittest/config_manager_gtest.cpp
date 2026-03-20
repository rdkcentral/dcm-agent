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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>     // For PATH_MAX
#include <sys/param.h>  // For PATH_MAX (backup)
#include <stddef.h>     // For offsetof

// Ensure PATH_MAX is defined
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern "C" {
#include "config_manager.h"
#include "backup_types.h"

// Define UTILS_SUCCESS and RDK logging constants for test environment
#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 0
#endif

// RDK Log level definitions for test environment
#ifndef RDK_LOG_FATAL
#define RDK_LOG_FATAL   0
#define RDK_LOG_ERROR   1
#define RDK_LOG_WARN    2
#define RDK_LOG_NOTICE  3
#define RDK_LOG_INFO    4
#define RDK_LOG_DEBUG   5
#define RDK_LOG_TRACE1  6
#define RDK_LOG_TRACE2  7
#define RDK_LOG_TRACE3  8
#define RDK_LOG_TRACE4  9
#define RDK_LOG_TRACE5  10
#define RDK_LOG_TRACE6  11
#define RDK_LOG_TRACE7  12
#define RDK_LOG_TRACE8  13
#define RDK_LOG_TRACE9  14
#endif

// RDK Log component name for test environment
#ifndef LOG_BACKUP_LOGS
#define LOG_BACKUP_LOGS "LOG.RDK.BACKUPLOGS"
#endif

// Mock RDK_LOG function declaration
void RDK_LOG(int level, const char* module, const char* format, ...);
}

using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArrayArgument;
using ::testing::StrEq;
using ::testing::_;

// Mock functions for external dependencies
extern "C" {
    // Mock RDK logging functions
    void __wrap_RDK_LOG(int level, const char* module, const char* format, ...) {
        // Suppress logging during tests
        (void)level;
        (void)module;
        (void)format;
    }

    // Mock property access functions
    int __real_getIncludePropertyData(const char* property, char* value, int size);
    int __wrap_getIncludePropertyData(const char* property, char* value, int size);

    int __real_getDevicePropertyData(const char* property, char* value, int size);
    int __wrap_getDevicePropertyData(const char* property, char* value, int size);
}

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Explicit initialization instead of just memset
        config.log_path[0] = '\0';
        config.persistent_path[0] = '\0';
        config.prev_log_path[0] = '\0';
        config.prev_log_backup_path[0] = '\0';
        config.hdd_enabled = false;  // Explicit false initialization

        // Also do memset to clear any padding
        memset(&config, 0, sizeof(config));
        config.hdd_enabled = false;  // Set again after memset

        // Reset mock expectations
        getIncludePropertyData_return_value = -1;
        getDevicePropertyData_return_value = -1;
        strcpy(mock_log_path, "");
        strcpy(mock_persistent_path, "");
        strcpy(mock_hdd_enabled, "");
    }

    void TearDown() override {
        // Clean up
    }

    backup_config_t config;

public:
    // Mock control variables - made public for wrapper function access
    static int getIncludePropertyData_return_value;
    static int getDevicePropertyData_return_value;
    static char mock_log_path[PATH_MAX];        // Match actual structure size
    static char mock_persistent_path[PATH_MAX]; // Match actual structure size
    static char mock_hdd_enabled[32];
};

// Static member definitions
int ConfigManagerTest::getIncludePropertyData_return_value = -1;
int ConfigManagerTest::getDevicePropertyData_return_value = -1;
char ConfigManagerTest::mock_log_path[PATH_MAX] = "";
char ConfigManagerTest::mock_persistent_path[PATH_MAX] = "";
char ConfigManagerTest::mock_hdd_enabled[32] = "";

// Mock implementation for getIncludePropertyData
int __wrap_getIncludePropertyData(const char* property, char* value, int size) {
    if (strcmp(property, "LOG_PATH") == 0 && ConfigManagerTest::getIncludePropertyData_return_value == 0) {
        strncpy(value, ConfigManagerTest::mock_log_path, size - 1);
        value[size - 1] = '\0';
        return 0;
    }
    return ConfigManagerTest::getIncludePropertyData_return_value;
}

// Mock implementation for getDevicePropertyData
int __wrap_getDevicePropertyData(const char* property, char* value, int size) {
    printf("DEBUG: Mock getDevicePropertyData called with property='%s', return_value=%d\n",
           property, ConfigManagerTest::getDevicePropertyData_return_value);

    if (ConfigManagerTest::getDevicePropertyData_return_value != UTILS_SUCCESS) {
        printf("DEBUG: Mock returning early with value %d\n", ConfigManagerTest::getDevicePropertyData_return_value);
        return ConfigManagerTest::getDevicePropertyData_return_value;
    }

    if (strcmp(property, "APP_PERSISTENT_PATH") == 0) {
        strncpy(value, ConfigManagerTest::mock_persistent_path, size - 1);
        value[size - 1] = '\0';
        printf("DEBUG: Mock returning APP_PERSISTENT_PATH='%s'\n", value);
        return UTILS_SUCCESS;
    } else if (strcmp(property, "HDD_ENABLED") == 0) {
        strncpy(value, ConfigManagerTest::mock_hdd_enabled, size - 1);
        value[size - 1] = '\0';
        printf("DEBUG: Mock returning HDD_ENABLED='%s'\n", value);
        return UTILS_SUCCESS;
    }

    printf("DEBUG: Mock property not found, returning -1\n");
    return -1;
}

// Test Cases

TEST_F(ConfigManagerTest, ConfigLoadNullPointer) {
    // Test NULL parameter handling
    int result = config_load(nullptr);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_AllPropertiesFound) {
    // Setup mock data
    strcpy(mock_log_path, "/custom/logs");
    strcpy(mock_persistent_path, "/custom/persistent");
    strcpy(mock_hdd_enabled, "true");

    getIncludePropertyData_return_value = 0;  // Success
    getDevicePropertyData_return_value = UTILS_SUCCESS;  // Success

    // Execute
    int result = config_load(&config);

    // Verify results
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(config.log_path, "/custom/logs");
    EXPECT_STREQ(config.persistent_path, "/custom/persistent");
    EXPECT_TRUE(config.hdd_enabled);
    EXPECT_STREQ(config.prev_log_path, "/custom/logs/PreviousLogs");
    EXPECT_STREQ(config.prev_log_backup_path, "/custom/logs/PreviousLogs_backup");
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_UseDefaults) {
    // Setup - all properties fail to load
    getIncludePropertyData_return_value = -1;  // Fail
    getDevicePropertyData_return_value = -1;   // Fail

    // Debug: Check initial state
    printf("DEBUG: Before config_load - hdd_enabled = %s\n", config.hdd_enabled ? "true" : "false");
    printf("DEBUG: Before config_load - raw value = %d\n", (int)config.hdd_enabled);
    printf("DEBUG: getDevicePropertyData_return_value = %d\n", getDevicePropertyData_return_value);
    printf("DEBUG: UTILS_SUCCESS = %d\n", UTILS_SUCCESS);
    printf("DEBUG: sizeof(backup_config_t) = %zu\n", sizeof(backup_config_t));
    printf("DEBUG: offset of hdd_enabled = %zu\n", offsetof(backup_config_t, hdd_enabled));
    printf("DEBUG: address of config = %p\n", (void*)&config);
    printf("DEBUG: address of hdd_enabled = %p\n", (void*)&config.hdd_enabled);

    // Test direct boolean assignment
    printf("DEBUG: Testing direct assignment...\n");
    config.hdd_enabled = false;
    printf("DEBUG: After direct false assignment = %s\n", config.hdd_enabled ? "true" : "false");
    config.hdd_enabled = true;
    printf("DEBUG: After direct true assignment = %s\n", config.hdd_enabled ? "true" : "false");
    config.hdd_enabled = false;
    printf("DEBUG: After second direct false assignment = %s\n", config.hdd_enabled ? "true" : "false");

    // Execute
    int result = config_load(&config);

    // Debug: Check final state
    printf("DEBUG: After config_load - hdd_enabled = %s\n", config.hdd_enabled ? "true" : "false");
    printf("DEBUG: After config_load - raw value = %d\n", (int)config.hdd_enabled);
    printf("DEBUG: Result = %d\n", result);

    // Verify results - should use defaults
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(config.log_path, "/opt/logs");
    EXPECT_STREQ(config.persistent_path, "/opt/persistent");
    EXPECT_STREQ(config.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_STREQ(config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_HddEnabledFalse) {
    // Setup
    strcpy(mock_log_path, "/opt/logs");
    strcpy(mock_persistent_path, "/opt/persistent");
    strcpy(mock_hdd_enabled, "false");

    getIncludePropertyData_return_value = 0;
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_FALSE(config.hdd_enabled);
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_HddEnabledTrue) {
    // Setup
    strcpy(mock_log_path, "/opt/logs");
    strcpy(mock_persistent_path, "/opt/persistent");
    strcpy(mock_hdd_enabled, "true");

    getIncludePropertyData_return_value = 0;
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(config.hdd_enabled);
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_HddEnabledNonFalse) {
    // Any value other than "false" should be treated as true
    strcpy(mock_log_path, "/opt/logs");
    strcpy(mock_persistent_path, "/opt/persistent");
    strcpy(mock_hdd_enabled, "yes");

    getIncludePropertyData_return_value = 0;
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(config.hdd_enabled);
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_EmptyLogPath) {
    // Test empty LOG_PATH falls back to default
    strcpy(mock_log_path, "");  // Empty string
    strcpy(mock_persistent_path, "/opt/persistent");
    strcpy(mock_hdd_enabled, "false");

    getIncludePropertyData_return_value = 0;  // Success but empty
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify - should use default
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(config.log_path, "/opt/logs");
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_EmptyPersistentPath) {
    // Test empty APP_PERSISTENT_PATH falls back to default
    strcpy(mock_log_path, "/opt/logs");
    strcpy(mock_persistent_path, "");  // Empty string
    strcpy(mock_hdd_enabled, "false");

    getIncludePropertyData_return_value = 0;
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify - should use default
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(config.persistent_path, "/opt/persistent");
}


TEST_F(ConfigManagerTest, ConfigLoadSuccess_MixedPropertyResults) {
    // Test scenario where some properties succeed and others fail
    strcpy(mock_log_path, "/custom/logs");
    strcpy(mock_hdd_enabled, "true");

    getIncludePropertyData_return_value = 0;  // LOG_PATH succeeds
    getDevicePropertyData_return_value = -1;  // Device properties fail

    // Execute
    int result = config_load(&config);

    // Verify - should mix custom and default values
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(config.log_path, "/custom/logs");
    EXPECT_STREQ(config.persistent_path, "/opt/persistent");  // Default
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_BoundaryValues) {
    // Test with boundary condition paths
    strcpy(mock_log_path, "/a");  // Very short path
    strcpy(mock_persistent_path, "/b");
    strcpy(mock_hdd_enabled, "false");

    getIncludePropertyData_return_value = 0;
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(config.log_path, "/a");
    EXPECT_STREQ(config.persistent_path, "/b");
    EXPECT_STREQ(config.prev_log_path, "/a/PreviousLogs");
    EXPECT_STREQ(config.prev_log_backup_path, "/a/PreviousLogs_backup");
}

TEST_F(ConfigManagerTest, ConfigLoadSuccess_VerifyNullTermination) {
    // Test that all strings are properly null-terminated
    strcpy(mock_log_path, "/test/logs");
    strcpy(mock_persistent_path, "/test/persistent");
    strcpy(mock_hdd_enabled, "true");

    getIncludePropertyData_return_value = 0;
    getDevicePropertyData_return_value = UTILS_SUCCESS;

    // Execute
    int result = config_load(&config);

    // Verify null termination
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_EQ(config.log_path[sizeof(config.log_path) - 1], '\0');
    EXPECT_EQ(config.persistent_path[sizeof(config.persistent_path) - 1], '\0');

    // Verify string lengths are reasonable
    EXPECT_GT(strlen(config.log_path), 0);
    EXPECT_GT(strlen(config.persistent_path), 0);
    EXPECT_GT(strlen(config.prev_log_path), 0);
    EXPECT_GT(strlen(config.prev_log_backup_path), 0);
}

// Test runner
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
