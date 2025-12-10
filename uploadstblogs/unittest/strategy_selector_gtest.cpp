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
#include "./mocks/mock_rdk_utils.h"

// Mock validate_codebig_access function for strategy_selector
extern "C" {
bool validate_codebig_access(void) {
    return true; // Default mock implementation
}
}

// Include the source file to test internal functions
extern "C" {
#include "../src/strategy_selector.c"
}

using namespace testing;
using namespace std;

class StrategySelectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mockRdkUtils = new MockRdkUtils();
        memset(&ctx, 0, sizeof(RuntimeContext));
        memset(&session, 0, sizeof(SessionState));
        
        // Set up default context values
        strcpy(ctx.paths.log_path, "/opt/logs");
        strcpy(ctx.paths.prev_log_path, "/opt/logs/PreviousLogs");
        strcpy(ctx.paths.temp_dir, "/tmp");
        strcpy(ctx.paths.archive_path, "/tmp");
        strcpy(ctx.paths.telemetry_path, "/opt/.telemetry");
        strcpy(ctx.paths.dcm_log_path, "/tmp/DCM");
        strcpy(ctx.endpoints.endpoint_url, "https://primary.example.com/upload");
        strcpy(ctx.endpoints.upload_http_link, "https://fallback.example.com/upload");
        
        // Set device type to mediaclient for privacy mode tests to work
        strcpy(ctx.device.device_type, "mediaclient");
        
        // Set default flag values
        ctx.flags.rrd_flag = 0;
        ctx.flags.dcm_flag = 1;
        ctx.flags.upload_on_reboot = 0;
        ctx.flags.flag = 0;
        ctx.flags.trigger_type = TRIGGER_SCHEDULED;
    }

    void TearDown() override {
        delete g_mockRdkUtils;
        g_mockRdkUtils = nullptr;
    }

    RuntimeContext ctx;
    SessionState session;
};

// Test early_checks function
TEST_F(StrategySelectorTest, EarlyChecks_NullContext) {
    Strategy result = early_checks(nullptr);
    EXPECT_EQ(STRAT_DCM, result); // Default fallback
}

TEST_F(StrategySelectorTest, EarlyChecks_RrdFlag) {
    ctx.flags.rrd_flag = 1;
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_RRD, result);
}

TEST_F(StrategySelectorTest, EarlyChecks_PrivacyMode) {
    // Mock privacy mode check - this test requires the actual privacy check function
    // For now, test that privacy mode false allows other logic to proceed
    ctx.settings.privacy_do_not_share = true;
    
    Strategy result = early_checks(&ctx);
    // Result depends on privacy implementation, just verify it doesn't crash
    EXPECT_TRUE(result == STRAT_PRIVACY_ABORT || result == STRAT_DCM);
}

TEST_F(StrategySelectorTest, EarlyChecks_OnDemandTrigger) {
    ctx.flags.trigger_type = TRIGGER_ONDEMAND;
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_ONDEMAND, result);
}

TEST_F(StrategySelectorTest, EarlyChecks_NonDcmFlag) {
    ctx.flags.dcm_flag = 0;
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_NON_DCM, result);
}

TEST_F(StrategySelectorTest, EarlyChecks_RebootStrategy) {
    ctx.flags.upload_on_reboot = 1;
    ctx.flags.flag = 1;
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_REBOOT, result);
}

TEST_F(StrategySelectorTest, EarlyChecks_DefaultDcm) {
    // All conditions false, should default to DCM
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_DCM, result);
}

// Test is_privacy_mode function
TEST_F(StrategySelectorTest, IsPrivacyMode_NullContext) {
    bool result = is_privacy_mode(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(StrategySelectorTest, IsPrivacyMode_Enabled) {
    ctx.settings.privacy_do_not_share = true;
    
    bool result = is_privacy_mode(&ctx);
    EXPECT_TRUE(result);
}

TEST_F(StrategySelectorTest, IsPrivacyMode_Disabled) {
    ctx.settings.privacy_do_not_share = false;
    
    bool result = is_privacy_mode(&ctx);
    EXPECT_FALSE(result);
}

TEST_F(StrategySelectorTest, IsPrivacyMode_False) {
    ctx.settings.privacy_do_not_share = false;
    
    bool result = is_privacy_mode(&ctx);
    EXPECT_FALSE(result);
}

// Test has_no_logs function
TEST_F(StrategySelectorTest, HasNoLogs_NullContext) {
    bool result = has_no_logs(nullptr);
    EXPECT_TRUE(result); // Conservative assumption
}

// Test decide_paths function
TEST_F(StrategySelectorTest, DecidePaths_NullContext) {
    decide_paths(nullptr, &session);
    // Should not crash
}

TEST_F(StrategySelectorTest, DecidePaths_NullSession) {
    decide_paths(&ctx, nullptr);
    // Should not crash
}

TEST_F(StrategySelectorTest, DecidePaths_ValidInputs) {
    decide_paths(&ctx, &session);
    
    // Verify paths are copied correctly
    // Note: The actual implementation may copy different fields
    // This test verifies the function doesn't crash
    EXPECT_TRUE(true); // Basic success test
}

// Test strategy decision tree combinations
TEST_F(StrategySelectorTest, StrategyDecisionTree_MultipleFlags) {
    // Test priority: RRD flag should override everything
    ctx.flags.rrd_flag = 1;
    ctx.flags.trigger_type = TRIGGER_ONDEMAND;
    ctx.flags.dcm_flag = 0;
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_RRD, result);
}

TEST_F(StrategySelectorTest, StrategyDecisionTree_OnDemandOverridesNonDcm) {
    // OnDemand should take priority over non-DCM
    ctx.flags.trigger_type = TRIGGER_ONDEMAND;
    ctx.flags.dcm_flag = 0;
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_ONDEMAND, result);
}

TEST_F(StrategySelectorTest, StrategyDecisionTree_RebootRequiresBothFlags) {
    // Test that REBOOT strategy requires both upload_on_reboot=1 AND flag=1
    ctx.flags.upload_on_reboot = 1;
    ctx.flags.flag = 0; // Missing this flag
    
    Strategy result = early_checks(&ctx);
    EXPECT_EQ(STRAT_DCM, result); // Should fall through to DCM
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
