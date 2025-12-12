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
#include <stdio.h>
#include <fstream>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif

// Mock RDK_LOG before including uploadstblogs_types.h
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"
#include "./mocks/mock_rdk_utils.h"
#include "./mocks/mock_rbus.h"

// Include the source file to test internal functions
extern "C" {
#include "../src/context_manager.c"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "context_manager_gtest_report.json"

using namespace testing;
using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::DoAll;
using ::testing::StrEq;
using ::testing::Invoke;

class ContextManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up mock objects
        g_mockRdkUtils = new MockRdkUtils();
        g_mockRbus = new MockRbus();
        
        // Clear context
        memset(&ctx, 0, sizeof(RuntimeContext));
    }

    void TearDown() override {
        // Clean up temp files
        unlink("/tmp/.lastdirectfail_upl");
        unlink("/tmp/.lastcodebigfail_upl");
        unlink("/tmp/.EnableOCSPStapling");
        unlink("/tmp/.EnableOCSPCA");
        
        delete g_mockRdkUtils;
        delete g_mockRbus;
        g_mockRdkUtils = nullptr;
        g_mockRbus = nullptr;
    }

    RuntimeContext ctx;
};

// Helper functions
void CreateTestFile(const char* filename, const char* content = "") {
    std::ofstream ofs(filename);
    ofs << content;
}

void CreateTestFileWithAge(const char* filename, time_t age_seconds) {
    CreateTestFile(filename, "test");
    struct stat st;
    if (stat(filename, &st) == 0) {
        struct timespec times[2];
        times[0].tv_sec = st.st_atime;
        times[0].tv_nsec = 0;
        times[1].tv_sec = time(NULL) - age_seconds;  // Set mtime to age_seconds ago
        times[1].tv_nsec = 0;
        utimensat(AT_FDCWD, filename, times, 0);
    }
}

// Test is_direct_blocked function
TEST_F(ContextManagerTest, DirectBlocked_NoFile) {
    unlink("/tmp/.lastdirectfail_upl");
    EXPECT_FALSE(is_direct_blocked(86400));
}

TEST_F(ContextManagerTest, DirectBlocked_FileWithinBlockTime) {
    CreateTestFileWithAge("/tmp/.lastdirectfail_upl", 3600); // 1 hour ago
    EXPECT_TRUE(is_direct_blocked(86400)); // 24 hour block time
}

TEST_F(ContextManagerTest, DirectBlocked_FileExpired) {
    CreateTestFileWithAge("/tmp/.lastdirectfail_upl", 90000); // 25 hours ago
    EXPECT_FALSE(is_direct_blocked(86400)); // 24 hour block time
    
    // File should be removed
    EXPECT_EQ(access("/tmp/.lastdirectfail_upl", F_OK), -1);
}

// Test is_codebig_blocked function  
TEST_F(ContextManagerTest, CodebigBlocked_NoFile) {
    unlink("/tmp/.lastcodebigfail_upl");
    EXPECT_FALSE(is_codebig_blocked(1800));
}

TEST_F(ContextManagerTest, CodebigBlocked_FileWithinBlockTime) {
    CreateTestFileWithAge("/tmp/.lastcodebigfail_upl", 900); // 15 minutes ago
    EXPECT_TRUE(is_codebig_blocked(1800)); // 30 minute block time
}

TEST_F(ContextManagerTest, CodebigBlocked_FileExpired) {
    CreateTestFileWithAge("/tmp/.lastcodebigfail_upl", 2000); // 33+ minutes ago
    EXPECT_FALSE(is_codebig_blocked(1800)); // 30 minute block time
    
    // File should be removed
    EXPECT_EQ(access("/tmp/.lastcodebigfail_upl", F_OK), -1);
}

// Test load_environment function
TEST_F(ContextManagerTest, LoadEnvironment_NullContext) {
    EXPECT_FALSE(load_environment(nullptr));
}

TEST_F(ContextManagerTest, LoadEnvironment_Success) {
    // Set up mock expectations for successful property loading
    EXPECT_CALL(*g_mockRdkUtils, getIncludePropertyData(StrEq("LOG_PATH"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("/opt/test", "/opt/test" + 9),
                       Return(UTILS_SUCCESS)));
    
    EXPECT_CALL(*g_mockRdkUtils, getIncludePropertyData(StrEq("DIRECT_BLOCK_TIME"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("43200", "43200" + 6),
                       Return(UTILS_SUCCESS)));
    
    EXPECT_CALL(*g_mockRdkUtils, getIncludePropertyData(StrEq("CB_BLOCK_TIME"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("900", "900" + 4),
                       Return(UTILS_SUCCESS)));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(StrEq("PROXY_BUCKET"), _, _))
        .WillOnce(Return(UTILS_FAIL));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(StrEq("DEVICE_TYPE"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("mediaclient", "mediaclient" + 11),
                       Return(UTILS_SUCCESS)));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(StrEq("BUILD_TYPE"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("prod", "prod" + 5),
                       Return(UTILS_SUCCESS)));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(StrEq("DCM_LOG_PATH"), _, _))
        .WillOnce(Return(UTILS_FAIL));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(StrEq("ENABLE_MAINTENANCE"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("true", "true" + 5),
                       Return(UTILS_SUCCESS)));

    EXPECT_TRUE(load_environment(&ctx));
    
    // Verify loaded values
    EXPECT_STREQ(ctx.paths.log_path, "/opt/test");
    EXPECT_STREQ(ctx.paths.prev_log_path, "/opt/test/PreviousLogs");
    EXPECT_EQ(ctx.retry.direct_retry_delay, 43200);
    EXPECT_EQ(ctx.retry.codebig_retry_delay, 900);
    EXPECT_STREQ(ctx.device.device_type, "mediaclient");
    EXPECT_STREQ(ctx.device.build_type, "prod");
    EXPECT_TRUE(ctx.settings.maintenance_enabled);
}

TEST_F(ContextManagerTest, LoadEnvironment_DefaultValues) {
    // All property calls fail, should use defaults
    EXPECT_CALL(*g_mockRdkUtils, getIncludePropertyData(_, _, _))
        .WillRepeatedly(Return(UTILS_FAIL));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(_, _, _))
        .WillRepeatedly(Return(UTILS_FAIL));

    EXPECT_TRUE(load_environment(&ctx));
    
    // Verify default values
    EXPECT_STREQ(ctx.paths.log_path, "/opt/logs");
    EXPECT_STREQ(ctx.paths.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_EQ(ctx.retry.direct_retry_delay, 86400);
    EXPECT_EQ(ctx.retry.codebig_retry_delay, 1800);
    EXPECT_EQ(ctx.retry.direct_max_attempts, 3);
    EXPECT_EQ(ctx.retry.codebig_max_attempts, 1);
}

TEST_F(ContextManagerTest, LoadEnvironment_OCSPEnabled) {
    // Create OCSP marker files
    CreateTestFile("/tmp/.EnableOCSPStapling");
    
    EXPECT_CALL(*g_mockRdkUtils, getIncludePropertyData(_, _, _))
        .WillRepeatedly(Return(UTILS_FAIL));
    
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(_, _, _))
        .WillRepeatedly(Return(UTILS_FAIL));

    EXPECT_TRUE(load_environment(&ctx));
    EXPECT_TRUE(ctx.settings.ocsp_enabled);
}

// Test load_tr181_params function
TEST_F(ContextManagerTest, LoadTR181Params_NullContext) {
    EXPECT_FALSE(load_tr181_params(nullptr));
}

TEST_F(ContextManagerTest, LoadTR181Params_RbusInitFail) {
    EXPECT_CALL(*g_mockRbus, rbus_init())
        .WillOnce(Return(false));
    
    EXPECT_FALSE(load_tr181_params(&ctx));
}

TEST_F(ContextManagerTest, LoadTR181Params_Success) {
    EXPECT_CALL(*g_mockRbus, rbus_init())
        .WillOnce(Return(true));
    
    EXPECT_CALL(*g_mockRbus, rbus_get_string_param(
        StrEq("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("https://example.com/upload", "https://example.com/upload" + 27),
                       Return(true)));
    
    EXPECT_CALL(*g_mockRbus, rbus_get_bool_param(
        StrEq("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.EncryptCloudUpload.Enable"), _))
        .WillOnce(DoAll(SetArgPointee<1>(true), Return(true)));
    
    EXPECT_CALL(*g_mockRbus, rbus_get_string_param(
        StrEq("Device.X_RDKCENTRAL-COM_Privacy.PrivacyMode"), _, _))
        .WillOnce(DoAll(SetArrayArgument<1>("DO_NOT_SHARE", "DO_NOT_SHARE" + 12),
                       Return(true)));
    
    EXPECT_TRUE(load_tr181_params(&ctx));
    
    // Verify loaded values
    EXPECT_STREQ(ctx.endpoints.endpoint_url, "https://example.com/upload");
    EXPECT_TRUE(ctx.settings.encryption_enable);
    EXPECT_TRUE(ctx.settings.privacy_do_not_share);
}

// Test get_mac_address function
TEST_F(ContextManagerTest, GetMacAddress_NullBuffer) {
    EXPECT_FALSE(get_mac_address(nullptr, 32));
}

TEST_F(ContextManagerTest, GetMacAddress_ZeroSize) {
    char buffer[32];
    EXPECT_FALSE(get_mac_address(buffer, 0));
}

TEST_F(ContextManagerTest, GetMacAddress_Success) {
    char mac_buffer[32];
    
    EXPECT_CALL(*g_mockRdkUtils, GetEstbMac(_, _))
        .WillOnce(DoAll(
            Invoke([](char* mac_buf, size_t buf_size) -> size_t {
                strcpy(mac_buf, "AA:BB:CC:DD:EE:FF");
                return strlen("AA:BB:CC:DD:EE:FF");
            })));
    
    EXPECT_TRUE(get_mac_address(mac_buffer, sizeof(mac_buffer)));
    EXPECT_STREQ(mac_buffer, "AA:BB:CC:DD:EE:FF");
}

TEST_F(ContextManagerTest, GetMacAddress_Failure) {
    char mac_buffer[32];
    
    EXPECT_CALL(*g_mockRdkUtils, GetEstbMac(_, _))
        .WillOnce(Return(0));
    
    EXPECT_FALSE(get_mac_address(mac_buffer, sizeof(mac_buffer)));
}

// Test init_context function
TEST_F(ContextManagerTest, InitContext_NullPointer) {
    EXPECT_FALSE(init_context(nullptr));
}

TEST_F(ContextManagerTest, InitContext_Success) {
    // Mock load_environment success
    EXPECT_CALL(*g_mockRdkUtils, getIncludePropertyData(_, _, _))
        .WillRepeatedly(Return(UTILS_FAIL));
    EXPECT_CALL(*g_mockRdkUtils, getDevicePropertyData(_, _, _))
        .WillRepeatedly(Return(UTILS_FAIL));
    
    // Mock load_tr181_params success
    EXPECT_CALL(*g_mockRbus, rbus_init())
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockRbus, rbus_get_string_param(_, _, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockRbus, rbus_get_bool_param(_, _))
        .WillRepeatedly(Return(false));
    
    // Mock get_mac_address success
    EXPECT_CALL(*g_mockRdkUtils, GetEstbMac(_, _))
        .WillOnce(DoAll(
            Invoke([](char* mac_buf, size_t buf_size) -> size_t {
                strcpy(mac_buf, "AA:BB:CC:DD:EE:FF");
                return strlen("AA:BB:CC:DD:EE:FF");
            })));
    
    EXPECT_TRUE(init_context(&ctx));
}

TEST_F(ContextManagerTest, InitContext_LoadEnvironmentFails) {
    // Return null context to make load_environment fail
    RuntimeContext* nullCtx = nullptr;
    EXPECT_FALSE(init_context(nullCtx));
}

// Test main function for Google Test
int main(int argc, char** argv) {
    // Create test results directory
    system("mkdir -p " GTEST_DEFAULT_RESULT_FILEPATH);
    
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    return RUN_ALL_TESTS();
}
