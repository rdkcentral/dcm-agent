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
#include <cstdio>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

// Mock functions for external telemetry 2.0 API
extern "C" {
#ifdef T2_EVENT_ENABLED
// Mock telemetry 2.0 API functions
int t2_init(const char* component);
void t2_uninit(void);
void t2_event_d(char* marker, int value);
void t2_event_s(char* marker, char* value);
#endif
}

// Global variables to track mock calls
static bool g_t2_init_called = false;
static bool g_t2_uninit_called = false;
static char g_last_count_marker[256] = {0};
static char g_last_value_marker[256] = {0};
static char g_last_value[256] = {0};
static int g_last_count_value = 0;

// Mock implementations for external T2 API
#ifdef T2_EVENT_ENABLED
int t2_init(const char* component) {
    g_t2_init_called = true;
    return 0;
}

void t2_uninit(void) {
    g_t2_uninit_called = true;
}

void t2_event_d(char* marker, int value) {
    if (marker) strncpy(g_last_count_marker, marker, sizeof(g_last_count_marker) - 1);
    g_last_count_value = value;
}

void t2_event_s(char* marker, char* value) {
    if (marker) strncpy(g_last_value_marker, marker, sizeof(g_last_value_marker) - 1);
    if (value) strncpy(g_last_value, value, sizeof(g_last_value) - 1);
}
#endif

// Include the actual telemetry implementation to test
#include "telemetry.h"
#include "../src/telemetry.c"

using namespace testing;

class TelemetryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        g_t2_init_called = false;
        g_t2_uninit_called = false;
        memset(g_last_count_marker, 0, sizeof(g_last_count_marker));
        memset(g_last_value_marker, 0, sizeof(g_last_value_marker));
        memset(g_last_value, 0, sizeof(g_last_value));
        g_last_count_value = 0;
        
        // Set up session state
        memset(&session, 0, sizeof(SessionState));
        strcpy(session.archive_file, "/tmp/test_archive.tar.gz");
        session.strategy = STRAT_DCM;
        session.used_fallback = false;
    }
    
    void TearDown() override {}
    
    SessionState session;
};

// Test telemetry initialization
TEST_F(TelemetryTest, TelemetryInit_Success) {
    telemetry_init();
    
#ifdef T2_EVENT_ENABLED
    EXPECT_TRUE(g_t2_init_called);
#else
    EXPECT_FALSE(g_t2_init_called);
#endif
}

TEST_F(TelemetryTest, TelemetryUninit_Success) {
    telemetry_uninit();
    
#ifdef T2_EVENT_ENABLED
    EXPECT_TRUE(g_t2_uninit_called);
#else
    EXPECT_FALSE(g_t2_uninit_called);
#endif
}

// Test upload success reporting
TEST_F(TelemetryTest, ReportUploadSuccess_ValidSession) {
    session.used_fallback = false;
    
    report_upload_success(&session);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "SYST_INFO_lu_success");
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

TEST_F(TelemetryTest, ReportUploadSuccess_WithFallback) {
    session.used_fallback = true;
    
    report_upload_success(&session);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "SYST_INFO_lu_success");
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

TEST_F(TelemetryTest, ReportUploadSuccess_NullSession) {
    report_upload_success(nullptr);
    
    // Should not crash and should not send telemetry
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "");
#endif
}

// Test upload failure reporting
TEST_F(TelemetryTest, ReportUploadFailure_ValidSession) {
    report_upload_failure(&session);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "SYST_ERR_LogUpload_Failed");
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

TEST_F(TelemetryTest, ReportUploadFailure_NullSession) {
    report_upload_failure(nullptr);
    
    // Should not crash and should not send telemetry
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "");
#endif
}

// Test DRI upload reporting
TEST_F(TelemetryTest, ReportDriUpload_Success) {
    report_dri_upload();
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "SYST_INFO_PDRILogUpload");
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

// Test certificate error reporting
TEST_F(TelemetryTest, ReportCertError_WithFqdn) {
    const char* test_fqdn = "upload.example.com";
    int error_code = 60; // SSL certificate error
    
    report_cert_error(error_code, test_fqdn);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "certerr_split");
    EXPECT_STRNE(g_last_value, "");
    EXPECT_TRUE(strstr(g_last_value, "STBLogUL") != nullptr);
    EXPECT_TRUE(strstr(g_last_value, "60") != nullptr);
    EXPECT_TRUE(strstr(g_last_value, test_fqdn) != nullptr);
#endif
}

TEST_F(TelemetryTest, ReportCertError_WithoutFqdn) {
    int error_code = 35;
    
    report_cert_error(error_code, nullptr);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "certerr_split");
    EXPECT_STRNE(g_last_value, "");
    EXPECT_TRUE(strstr(g_last_value, "STBLogUL") != nullptr);
    EXPECT_TRUE(strstr(g_last_value, "35") != nullptr);
#endif
}

TEST_F(TelemetryTest, ReportCertError_EmptyFqdn) {
    int error_code = 51;
    
    report_cert_error(error_code, "");
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "certerr_split");
    EXPECT_STRNE(g_last_value, "");
    EXPECT_TRUE(strstr(g_last_value, "STBLogUL") != nullptr);
    EXPECT_TRUE(strstr(g_last_value, "51") != nullptr);
#endif
}

// Test CURL error reporting
TEST_F(TelemetryTest, ReportCurlError_GeneralError) {
    int curl_code = 7; // Couldn't connect to host
    
    report_curl_error(curl_code);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "LUCurlErr_split");
    EXPECT_STREQ(g_last_value, "7");
#endif
}

TEST_F(TelemetryTest, ReportCurlError_TimeoutError) {
    int curl_code = 28; // Timeout was reached
    
    // Reset markers to check both calls
    memset(g_last_count_marker, 0, sizeof(g_last_count_marker));
    memset(g_last_value_marker, 0, sizeof(g_last_value_marker));
    
    report_curl_error(curl_code);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "LUCurlErr_split");
    EXPECT_STREQ(g_last_value, "28");
    // Note: The count marker will be overwritten by the second call
    EXPECT_STREQ(g_last_count_marker, "SYST_ERR_Curl28");
#endif
}

// Test upload attempt reporting
TEST_F(TelemetryTest, ReportUploadAttempt_Success) {
    report_upload_attempt();
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "SYST_INFO_LUattempt");
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

// Test mTLS usage reporting
TEST_F(TelemetryTest, ReportMtlsUsage_Success) {
    report_mtls_usage();
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "SYST_INFO_mtls_xpki");
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

// Test low-level telemetry functions
TEST_F(TelemetryTest, T2CountNotify_ValidMarker) {
    const char* marker = "TEST_MARKER";
    
    t2_count_notify(marker);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, marker);
    EXPECT_EQ(g_last_count_value, 1);
#endif
}

TEST_F(TelemetryTest, T2CountNotify_NullMarker) {
    t2_count_notify(nullptr);
    
    // Should not crash and should not send telemetry
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_count_marker, "");
#endif
}

TEST_F(TelemetryTest, T2ValNotify_ValidParams) {
    const char* marker = "TEST_VALUE_MARKER";
    const char* value = "test_value";
    
    t2_val_notify(marker, value);
    
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, marker);
    EXPECT_STREQ(g_last_value, value);
#endif
}

TEST_F(TelemetryTest, T2ValNotify_NullMarker) {
    t2_val_notify(nullptr, "value");
    
    // Should not crash and should not send telemetry
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "");
#endif
}

TEST_F(TelemetryTest, T2ValNotify_NullValue) {
    t2_val_notify("marker", nullptr);
    
    // Should not crash and should not send telemetry
#ifdef T2_EVENT_ENABLED
    EXPECT_STREQ(g_last_value_marker, "");
#endif
}

// Test integration scenarios
TEST_F(TelemetryTest, UploadFlow_SuccessfulDirect) {
    // Simulate a successful direct upload flow
    report_upload_attempt();
    report_mtls_usage();
    report_upload_success(&session);
    
#ifdef T2_EVENT_ENABLED
    // Last call should be success
    EXPECT_STREQ(g_last_count_marker, "SYST_INFO_lu_success");
#endif
}

TEST_F(TelemetryTest, UploadFlow_FailureWithErrors) {
    // Simulate an upload flow with failures
    report_upload_attempt();
    report_curl_error(60); // SSL certificate problem
    report_cert_error(60, "upload.example.com");
    report_upload_failure(&session);
    
#ifdef T2_EVENT_ENABLED
    // Last call should be failure
    EXPECT_STREQ(g_last_count_marker, "SYST_ERR_LogUpload_Failed");
#endif
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}