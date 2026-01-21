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
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"
#include "./mocks/mock_rdk_utils.h"
#include "./mocks/mock_rbus.h"
#include "./mocks/mock_curl.h"

using namespace testing;

class UploadSTBLogsTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mockRdkUtils = new MockRdkUtils();
        g_mockRbus = new MockRbus();
        g_mockCurl = new MockCurl();
    }
    
    void TearDown() override {
        delete g_mockRdkUtils;
        delete g_mockRbus;
        delete g_mockCurl;
        g_mockRdkUtils = nullptr;
        g_mockRbus = nullptr;
        g_mockCurl = nullptr;
    }
};

TEST_F(UploadSTBLogsTest, BasicTest) {
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
