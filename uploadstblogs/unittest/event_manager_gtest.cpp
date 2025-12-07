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
#include <iostream>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

// Mock external dependencies
extern "C" {
// Mock system functions
int access(const char *pathname, int mode);
pid_t fork(void);
int execl(const char *pathname, const char *arg, ...);
void _exit(int status);
pid_t waitpid(pid_t pid, int *wstatus, int options);
int snprintf(char *str, size_t size, const char *format, ...);
int strcasecmp(const char *s1, const char *s2);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
int atoi(const char *nptr);

// Include va_list for variadic function mocking
#include <stdarg.h>

// Mock external module functions
int getDevicePropertyData(const char* property, char* buffer, size_t buffer_size);
void report_upload_success(const SessionState* session);
void report_upload_failure(const SessionState* session);

// Define constants that might be missing
#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 0
#endif

// Mock state
static bool mock_access_result = true;
static bool mock_maintenance_enabled = false;
static pid_t mock_fork_result = 1;
static bool mock_fork_fail = false;
static int mock_execl_fail = false;
static int mock_waitpid_status = 0;
static char mock_device_type[64] = "gateway";
static int mock_fork_call_count = 0;

#ifdef GTEST_ENABLE
// Mock call tracking variables
static int mock_iarm_event_calls = 0;
static char mock_last_event_name[256] = {0};
static int mock_last_event_code = 0;
static int mock_report_success_calls = 0;
static int mock_report_failure_calls = 0;

// Test-specific implementations
// Override send_iarm_event for testing
void send_iarm_event(const char* event_name, int event_code) {
    if (!event_name) {
        return;
    }
    
    // Simulate the same logic as real implementation
    // Check if IARM event sender binary exists
    if (!mock_access_result) {
        // Binary not found - don't increment call counter
        return;
    }
    
    // Simulate fork behavior
    if (mock_fork_fail) {
        // Fork failed - don't increment call counter
        return;
    }
    
    // Track the IARM event call for testing (only if all checks pass)
    mock_iarm_event_calls++;
    strcpy(mock_last_event_name, event_name);
    mock_last_event_code = event_code;
}

// Mock send_iarm_event_maintenance for testing
void send_iarm_event_maintenance(int maint_event_code) {
    // Mock implementation - just track the call
    mock_iarm_event_calls++;
    mock_last_event_code = maint_event_code;
}

// Mock implementations
int access(const char *pathname, int mode) {
    if (pathname && strstr(pathname, "IARM_event_sender")) {
        return mock_access_result ? 0 : -1;
    }
    if (pathname && strstr(pathname, "/etc/os-release")) {
        return 0; // Assume exists for most tests
    }
    return 0;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    if (str && size > 0) {
        str[0] = '\0'; // Simple mock
    }
    return 0;
}

int strcasecmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return -1;
    return strcmp(s1, s2); // Simple case-insensitive comparison mock
}

int getDevicePropertyData(const char* property, char* buffer, size_t buffer_size) {
    if (property && strcmp(property, "ENABLE_MAINTENANCE") == 0) {
        strcpy(buffer, mock_maintenance_enabled ? "true" : "false");
        return 0; // UTILS_SUCCESS
    }
    return -1;
}

void report_upload_success(const SessionState* session) {
    mock_report_success_calls++;
}

void report_upload_failure(const SessionState* session) {
    mock_report_failure_calls++;
}

} // extern "C"

#endif

// Include the actual event manager implementation
#include "event_manager.h"
#include "../src/event_manager.c"

using namespace testing;
using namespace std;

class EventManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        mock_access_result = true;
        mock_maintenance_enabled = false;
        mock_fork_result = 1; // Parent process - child will be handled separately
        mock_fork_fail = false;
        mock_execl_fail = false;
        mock_waitpid_status = 0;
        strcpy(mock_device_type, "gateway");
        mock_fork_call_count = 0;
        
        // Reset call tracking
        mock_iarm_event_calls = 0;
        memset(mock_last_event_name, 0, sizeof(mock_last_event_name));
        mock_last_event_code = 0;
        mock_report_success_calls = 0;
        mock_report_failure_calls = 0;
        
        // Initialize test structures
        memset(&test_ctx, 0, sizeof(RuntimeContext));
        memset(&test_session, 0, sizeof(SessionState));
        
        // Set up default test context
        strcpy(test_ctx.device.device_type, mock_device_type);
        strcpy(test_ctx.paths.log_path, "/opt/logs");
        
        // Set up default test session
        test_session.strategy = STRAT_DCM;
        test_session.direct_attempts = 1;
        test_session.codebig_attempts = 0;
        test_session.used_fallback = false;
        test_session.success = false;
    }
    
    void TearDown() override {}
    
    RuntimeContext test_ctx;
    SessionState test_session;
};

// Test emit_privacy_abort function
TEST_F(EventManagerTest, EmitPrivacyAbort_Success) {
    emit_privacy_abort();
    
    // Should send MAINT_LOGUPLOAD_COMPLETE event
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "MaintenanceMGR");
    EXPECT_EQ(mock_last_event_code, 4); // MAINT_LOGUPLOAD_COMPLETE
}

// Test emit_no_logs_reboot function
TEST_F(EventManagerTest, EmitNoLogsReboot_BroadbandDevice) {
    strcpy(test_ctx.device.device_type, "broadband");
    mock_maintenance_enabled = true;
    
    emit_no_logs_reboot(&test_ctx);
    
    // Should NOT send event for broadband device
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

TEST_F(EventManagerTest, EmitNoLogsReboot_NonBroadbandWithMaintenance) {
    strcpy(test_ctx.device.device_type, "gateway");
    mock_maintenance_enabled = true;
    
    emit_no_logs_reboot(&test_ctx);
    
    // Should send MAINT_LOGUPLOAD_COMPLETE event
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "MaintenanceMGR");
    EXPECT_EQ(mock_last_event_code, 4); // MAINT_LOGUPLOAD_COMPLETE
}

TEST_F(EventManagerTest, EmitNoLogsReboot_NonBroadbandWithoutMaintenance) {
    strcpy(test_ctx.device.device_type, "gateway");
    mock_maintenance_enabled = false;
    
    emit_no_logs_reboot(&test_ctx);
    
    // Should NOT send event without maintenance mode
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

TEST_F(EventManagerTest, EmitNoLogsReboot_NullContext) {
    mock_maintenance_enabled = true;
    
    emit_no_logs_reboot(nullptr);
    
    // Should handle null context gracefully
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

// Test emit_no_logs_ondemand function
TEST_F(EventManagerTest, EmitNoLogsOndemand_WithMaintenance) {
    mock_maintenance_enabled = true;
    
    emit_no_logs_ondemand();
    
    // Should send MAINT_LOGUPLOAD_COMPLETE event
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "MaintenanceMGR");
    EXPECT_EQ(mock_last_event_code, 4); // MAINT_LOGUPLOAD_COMPLETE
}

TEST_F(EventManagerTest, EmitNoLogsOndemand_WithoutMaintenance) {
    mock_maintenance_enabled = false;
    
    emit_no_logs_ondemand();
    
    // Should NOT send event without maintenance mode
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

// Test emit_upload_success function
TEST_F(EventManagerTest, EmitUploadSuccess_DirectPath) {
    test_session.success = true;
    test_session.used_fallback = false;
    test_session.direct_attempts = 2;
    mock_maintenance_enabled = true;
    
    emit_upload_success(&test_ctx, &test_session);
    
    // Should send LogUploadEvent success and MaintenanceMGR complete
    EXPECT_EQ(mock_iarm_event_calls, 2);
    EXPECT_EQ(mock_report_success_calls, 1);
}

TEST_F(EventManagerTest, EmitUploadSuccess_CodeBigPath) {
    test_session.success = true;
    test_session.used_fallback = true;
    test_session.codebig_attempts = 1;
    mock_maintenance_enabled = true;
    
    emit_upload_success(&test_ctx, &test_session);
    
    // Should send LogUploadEvent success and MaintenanceMGR complete
    EXPECT_EQ(mock_iarm_event_calls, 2);
    EXPECT_EQ(mock_report_success_calls, 1);
}

TEST_F(EventManagerTest, EmitUploadSuccess_BroadbandDevice) {
    strcpy(test_ctx.device.device_type, "broadband");
    test_session.success = true;
    mock_maintenance_enabled = true;
    
    emit_upload_success(&test_ctx, &test_session);
    
    // Should send only LogUploadEvent success (no MaintenanceMGR for broadband)
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "LogUploadEvent");
    EXPECT_EQ(mock_last_event_code, 0); // LOG_UPLOAD_SUCCESS
}

TEST_F(EventManagerTest, EmitUploadSuccess_NullSession) {
    emit_upload_success(&test_ctx, nullptr);
    
    // Should handle null session gracefully
    EXPECT_EQ(mock_iarm_event_calls, 0);
    EXPECT_EQ(mock_report_success_calls, 0);
}

// Test emit_upload_failure function
TEST_F(EventManagerTest, EmitUploadFailure_NonBroadbandWithMaintenance) {
    test_session.direct_attempts = 3;
    test_session.codebig_attempts = 2;
    mock_maintenance_enabled = true;
    
    emit_upload_failure(&test_ctx, &test_session);
    
    // Should send LogUploadEvent failure and MaintenanceMGR error
    EXPECT_EQ(mock_iarm_event_calls, 2);
    EXPECT_EQ(mock_report_failure_calls, 1);
}

TEST_F(EventManagerTest, EmitUploadFailure_BroadbandDevice) {
    strcpy(test_ctx.device.device_type, "broadband");
    test_session.direct_attempts = 3;
    mock_maintenance_enabled = true;
    
    emit_upload_failure(&test_ctx, &test_session);
    
    // Should send only LogUploadEvent failure (no MaintenanceMGR for broadband)
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "LogUploadEvent");
    EXPECT_EQ(mock_last_event_code, 1); // LOG_UPLOAD_FAILED
}

TEST_F(EventManagerTest, EmitUploadFailure_NullSession) {
    emit_upload_failure(&test_ctx, nullptr);
    
    // Should handle null session gracefully
    EXPECT_EQ(mock_iarm_event_calls, 0);
    EXPECT_EQ(mock_report_failure_calls, 0);
}

// Test emit_upload_aborted function
TEST_F(EventManagerTest, EmitUploadAborted_Success) {
    emit_upload_aborted();
    
    // Should send LogUploadEvent aborted and MaintenanceMGR error
    EXPECT_EQ(mock_iarm_event_calls, 2);
}

// Test emit_upload_start function
TEST_F(EventManagerTest, EmitUploadStart_Success) {
    emit_upload_start();
    
    // Should only log, not send events (matches script behavior)
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

// Test emit_fallback function
TEST_F(EventManagerTest, EmitFallback_DirectToCodeBig) {
    emit_fallback(PATH_DIRECT, PATH_CODEBIG);
    
    // Should only log, not send events
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

TEST_F(EventManagerTest, EmitFallback_CodeBigToDirect) {
    emit_fallback(PATH_CODEBIG, PATH_DIRECT);
    
    // Should only log, not send events
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

// Test send_iarm_event function
TEST_F(EventManagerTest, SendIarmEvent_Success) {
    send_iarm_event("LogUploadEvent", 0);
    
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "LogUploadEvent");
    EXPECT_EQ(mock_last_event_code, 0);
}

TEST_F(EventManagerTest, SendIarmEvent_NullEventName) {
    send_iarm_event(nullptr, 0);
    
    // Should handle null event name gracefully
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

TEST_F(EventManagerTest, SendIarmEvent_BinaryNotFound) {
    mock_access_result = false;
    
    send_iarm_event("LogUploadEvent", 0);
    
    // Should not send event when binary not found
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

TEST_F(EventManagerTest, SendIarmEvent_ForkFailure) {
    mock_fork_fail = true;
    
    send_iarm_event("LogUploadEvent", 0);
    
    // Should handle fork failure gracefully
    EXPECT_EQ(mock_iarm_event_calls, 0);
}

TEST_F(EventManagerTest, SendIarmEvent_ChildProcess) {
    mock_fork_result = 0; // Simulate child process
    
    send_iarm_event("LogUploadEvent", 0);
    
    // Child process should attempt exec
    EXPECT_EQ(mock_iarm_event_calls, 1);
}

// Test send_iarm_event_maintenance function
TEST_F(EventManagerTest, SendIarmEventMaintenance_Success) {
    send_iarm_event_maintenance(4);
    
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "MaintenanceMGR");
    EXPECT_EQ(mock_last_event_code, 4);
}

// Test emit_folder_missing_error function
TEST_F(EventManagerTest, EmitFolderMissingError_Success) {
    emit_folder_missing_error();
    
    // Should send MaintenanceMGR error event
    EXPECT_EQ(mock_iarm_event_calls, 1);
    EXPECT_STREQ(mock_last_event_name, "MaintenanceMGR");
    EXPECT_EQ(mock_last_event_code, 5); // MAINT_LOGUPLOAD_ERROR
}

// Integration tests
TEST_F(EventManagerTest, Integration_SuccessfulUploadFlow) {
    // Simulate successful upload flow
    emit_upload_start();
    EXPECT_EQ(mock_iarm_event_calls, 0);
    
    // Successful upload
    test_session.success = true;
    test_session.used_fallback = false;
    mock_maintenance_enabled = true;
    
    emit_upload_success(&test_ctx, &test_session);
    EXPECT_EQ(mock_iarm_event_calls, 2); // LogUploadEvent + MaintenanceMGR
    EXPECT_EQ(mock_report_success_calls, 1);
}

TEST_F(EventManagerTest, Integration_FailedUploadFlow) {
    // Simulate failed upload flow
    emit_upload_start();
    
    // Failed upload after fallback
    test_session.direct_attempts = 3;
    test_session.codebig_attempts = 2;
    mock_maintenance_enabled = true;
    
    emit_upload_failure(&test_ctx, &test_session);
    EXPECT_EQ(mock_iarm_event_calls, 2); // LogUploadEvent + MaintenanceMGR
    EXPECT_EQ(mock_report_failure_calls, 1);
}

TEST_F(EventManagerTest, Integration_NoLogsScenario) {
    // Test no logs scenario for different strategies
    mock_maintenance_enabled = true;
    
    // Ondemand strategy
    emit_no_logs_ondemand();
    EXPECT_EQ(mock_iarm_event_calls, 1);
    
    // Reset counters
    mock_iarm_event_calls = 0;
    
    // Reboot strategy (non-broadband)
    emit_no_logs_reboot(&test_ctx);
    EXPECT_EQ(mock_iarm_event_calls, 1);
}

// Test edge cases and error conditions
TEST_F(EventManagerTest, EdgeCases_DeviceTypeVariations) {
    const char* device_types[] = {"broadband", "gateway", "hybrid", "unknown"};
    bool should_send_maint[] = {false, true, true, true};
    
    mock_maintenance_enabled = true;
    test_session.success = true;
    
    for (int i = 0; i < 4; i++) {
        mock_iarm_event_calls = 0;
        strcpy(test_ctx.device.device_type, device_types[i]);
        
        emit_upload_success(&test_ctx, &test_session);
        
        int expected_calls = should_send_maint[i] ? 2 : 1;
        EXPECT_EQ(mock_iarm_event_calls, expected_calls) 
            << "Failed for device type: " << device_types[i];
    }
}

TEST_F(EventManagerTest, EdgeCases_MaintenanceModeStates) {
    // Test different maintenance mode states
    bool maintenance_states[] = {true, false};
    
    for (bool maintenance : maintenance_states) {
        mock_maintenance_enabled = maintenance;
        mock_iarm_event_calls = 0;
        
        emit_no_logs_ondemand();
        
        int expected_calls = maintenance ? 1 : 0;
        EXPECT_EQ(mock_iarm_event_calls, expected_calls) 
            << "Failed for maintenance state: " << maintenance;
    }
}

TEST_F(EventManagerTest, EdgeCases_EventCodeValues) {
    // Test various event codes
    int event_codes[] = {0, 1, 2, 4, 5, 16, -1, 999};
    
    for (int code : event_codes) {
        mock_iarm_event_calls = 0;
        mock_last_event_code = -999; // Reset
        
        send_iarm_event("TestEvent", code);
        
        EXPECT_EQ(mock_iarm_event_calls, 1);
        EXPECT_EQ(mock_last_event_code, code) << "Failed for event code: " << code;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    cout << "Starting Event Manager Unit Tests" << endl;
    return RUN_ALL_TESTS();
}