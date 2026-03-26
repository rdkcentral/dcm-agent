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


#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>

extern "C" {
#include "sys_integration.h"
#include "backup_types.h"

// Define return codes for test environment
#ifndef BACKUP_SUCCESS
#define BACKUP_SUCCESS 0
#endif

#ifndef BACKUP_ERROR_INVALID_PARAM
#define BACKUP_ERROR_INVALID_PARAM -5
#endif

#ifndef BACKUP_ERROR_SYSTEM
#define BACKUP_ERROR_SYSTEM -8
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

// Mock sd_notify function declaration
int sd_notify(int unset_environment, const char *state);
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

    // Mock systemd functions
    int __real_sd_notify(int unset_environment, const char *state);
    int __wrap_sd_notify(int unset_environment, const char *state);
}

class SysIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock expectations
        sd_notify_return_value = 1;  // Default success (positive value)
        sd_notify_call_count = 0;
        last_sd_notify_unset_environment = -999;  // Invalid value to detect if called
        memset(last_sd_notify_state, 0, sizeof(last_sd_notify_state));
    }

    void TearDown() override {
        // Clean up
    }

public:
    // Mock control variables - made public for wrapper function access
    static int sd_notify_return_value;
    static int sd_notify_call_count;
    static int last_sd_notify_unset_environment;
    static char last_sd_notify_state[1024];
};

// Static member definitions
int SysIntegrationTest::sd_notify_return_value = 1;
int SysIntegrationTest::sd_notify_call_count = 0;
int SysIntegrationTest::last_sd_notify_unset_environment = -999;
char SysIntegrationTest::last_sd_notify_state[1024] = "";

// Mock implementation for sd_notify
int __wrap_sd_notify(int unset_environment, const char *state) {
    SysIntegrationTest::sd_notify_call_count++;
    SysIntegrationTest::last_sd_notify_unset_environment = unset_environment;

    if (state && strlen(state) < sizeof(SysIntegrationTest::last_sd_notify_state)) {
        strncpy(SysIntegrationTest::last_sd_notify_state, state, sizeof(SysIntegrationTest::last_sd_notify_state) - 1);
        SysIntegrationTest::last_sd_notify_state[sizeof(SysIntegrationTest::last_sd_notify_state) - 1] = '\0';
    }

    return SysIntegrationTest::sd_notify_return_value;
}

// Test Cases

TEST_F(SysIntegrationTest, SystemdNotificationNullPointer) {
    // Test NULL parameter handling
    int result = sys_send_systemd_notification(nullptr);

    // Verify error handling
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);

    // Verify sd_notify was not called
    EXPECT_EQ(sd_notify_call_count, 0);
}

TEST_F(SysIntegrationTest, SystemdNotificationSuccess) {
    // Setup successful sd_notify return
    sd_notify_return_value = 1;  // Positive value indicates success

    const char* test_message = "Backup completed successfully";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify success
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called correctly
    EXPECT_EQ(sd_notify_call_count, 1);
    EXPECT_EQ(last_sd_notify_unset_environment, 0);

    // Verify the notification string format
    const char* expected_state = "READY=1\nSTATUS=Backup completed successfully";
    EXPECT_STREQ(last_sd_notify_state, expected_state);
}

TEST_F(SysIntegrationTest, SystemdNotificationFailure) {
    // Setup failed sd_notify return
    sd_notify_return_value = -1;  // Negative value indicates failure

    const char* test_message = "Test message";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify error handling
    EXPECT_EQ(result, BACKUP_ERROR_SYSTEM);

    // Verify sd_notify was called
    EXPECT_EQ(sd_notify_call_count, 1);
    EXPECT_EQ(last_sd_notify_unset_environment, 0);

    // Verify the notification string was built correctly even on failure
    const char* expected_state = "READY=1\nSTATUS=Test message";
    EXPECT_STREQ(last_sd_notify_state, expected_state);
}

TEST_F(SysIntegrationTest, SystemdNotificationEmptyMessage) {
    // Test with empty message
    sd_notify_return_value = 1;  // Success

    const char* test_message = "";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify success
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called
    EXPECT_EQ(sd_notify_call_count, 1);

    // Verify the notification string with empty status
    const char* expected_state = "READY=1\nSTATUS=";
    EXPECT_STREQ(last_sd_notify_state, expected_state);
}

TEST_F(SysIntegrationTest, SystemdNotificationLongMessage) {
    // Test with long message that approaches buffer limits
    sd_notify_return_value = 1;  // Success

    // Create a message that will test snprintf buffer handling
    // The notification buffer is 512 bytes, and "READY=1\nSTATUS=" uses 15 bytes
    // So we can safely use up to ~490 characters for the message
    std::string long_message(400, 'A');  // 400 'A' characters

    // Execute
    int result = sys_send_systemd_notification(long_message.c_str());

    // Verify success
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called
    EXPECT_EQ(sd_notify_call_count, 1);

    // Verify the notification string was built correctly
    std::string expected_state = "READY=1\nSTATUS=" + long_message;
    EXPECT_STREQ(last_sd_notify_state, expected_state.c_str());
}

TEST_F(SysIntegrationTest, SystemdNotificationVeryLongMessage) {
    // Test with message that would cause truncation
    sd_notify_return_value = 1;  // Success

    // Create a message longer than the notification buffer can handle
    // The notification buffer is 512 bytes total
    std::string very_long_message(600, 'B');  // 600 'B' characters

    // Execute
    int result = sys_send_systemd_notification(very_long_message.c_str());

    // Verify success (function should handle truncation gracefully)
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called
    EXPECT_EQ(sd_notify_call_count, 1);

    // Verify the notification string was truncated properly
    // The message should be truncated to fit in the 512-byte buffer
    size_t state_len = strlen(last_sd_notify_state);
    EXPECT_LT(state_len, 512);  // Should be less than buffer size

    // Should start with the expected prefix
    EXPECT_TRUE(strncmp(last_sd_notify_state, "READY=1\nSTATUS=", 15) == 0);
}

TEST_F(SysIntegrationTest, SystemdNotificationSpecialCharacters) {
    // Test with message containing special characters
    sd_notify_return_value = 1;  // Success

    const char* test_message = "Backup: 100% complete!\nFiles: 1,234\tSize: 5.6GB";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify success
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called
    EXPECT_EQ(sd_notify_call_count, 1);

    // Verify the notification string preserves special characters
    const char* expected_state = "READY=1\nSTATUS=Backup: 100% complete!\nFiles: 1,234\tSize: 5.6GB";
    EXPECT_STREQ(last_sd_notify_state, expected_state);
}

TEST_F(SysIntegrationTest, SystemdNotificationZeroReturn) {
    // Test sd_notify returning zero (which is not an error, but no notification sent)
    sd_notify_return_value = 0;  // Zero return (not negative, so no error)

    const char* test_message = "Test message";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify success (zero is not treated as an error)
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called
    EXPECT_EQ(sd_notify_call_count, 1);
}

TEST_F(SysIntegrationTest, SystemdNotificationMultipleCalls) {
    // Test multiple successive calls
    sd_notify_return_value = 1;  // Success

    // First call
    int result1 = sys_send_systemd_notification("First message");
    EXPECT_EQ(result1, BACKUP_SUCCESS);
    EXPECT_EQ(sd_notify_call_count, 1);
    EXPECT_STREQ(last_sd_notify_state, "READY=1\nSTATUS=First message");

    // Second call
    int result2 = sys_send_systemd_notification("Second message");
    EXPECT_EQ(result2, BACKUP_SUCCESS);
    EXPECT_EQ(sd_notify_call_count, 2);
    EXPECT_STREQ(last_sd_notify_state, "READY=1\nSTATUS=Second message");

    // Third call with different return value
    sd_notify_return_value = -1;  // Failure
    int result3 = sys_send_systemd_notification("Third message");
    EXPECT_EQ(result3, BACKUP_ERROR_SYSTEM);
    EXPECT_EQ(sd_notify_call_count, 3);
    EXPECT_STREQ(last_sd_notify_state, "READY=1\nSTATUS=Third message");
}

TEST_F(SysIntegrationTest, SystemdNotificationStringFormatValidation) {
    // Test that the notification string is always formatted correctly
    sd_notify_return_value = 1;  // Success

    const char* test_message = "Status update";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify success
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Detailed verification of the notification string format
    EXPECT_EQ(sd_notify_call_count, 1);

    // Check that it starts with "READY=1\n"
    EXPECT_TRUE(strncmp(last_sd_notify_state, "READY=1\n", 8) == 0);

    // Check that it has "STATUS=" after "READY=1\n"
    EXPECT_TRUE(strncmp(last_sd_notify_state + 8, "STATUS=", 7) == 0);

    // Check that the message appears correctly after "STATUS="
    EXPECT_TRUE(strncmp(last_sd_notify_state + 15, test_message, strlen(test_message)) == 0);

    // Verify total expected length
    size_t expected_len = 8 + 7 + strlen(test_message);  // READY=1\n + STATUS= + message
    EXPECT_EQ(strlen(last_sd_notify_state), expected_len);
}

TEST_F(SysIntegrationTest, SystemdNotificationParameterPassing) {
    // Test that parameters are passed correctly to sd_notify
    sd_notify_return_value = 2;  // Positive return value

    const char* test_message = "Parameter test";

    // Execute
    int result = sys_send_systemd_notification(test_message);

    // Verify success
    EXPECT_EQ(result, BACKUP_SUCCESS);

    // Verify sd_notify was called with correct parameters
    EXPECT_EQ(sd_notify_call_count, 1);

    // Verify unset_environment parameter is 0 (false)
    EXPECT_EQ(last_sd_notify_unset_environment, 0);

    // Verify state parameter content
    const char* expected_state = "READY=1\nSTATUS=Parameter test";
    EXPECT_STREQ(last_sd_notify_state, expected_state);
}

// Test runner
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
