/**
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
#include "usb_log_utils.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>

// Mocks for external dependencies
extern "C" {
    int rdk_logger_init(const char*) { return 0; }
    int getDevicePropertyData(const char*, char* buf, size_t) { strcpy(buf, "false"); return UTILS_SUCCESS; }
    int getIncludePropertyData(const char*, char* buf, size_t) { strcpy(buf, "/opt/logs"); return UTILS_SUCCESS; }

}

// Test usb_log_init
TEST(UsbLogUtilsTest, UsbLogInit_Success) {
    EXPECT_EQ(usb_log_init(), 0);
    EXPECT_EQ(usb_log_init(), 0); // Should not reinitialize
}

// Test get_current_timestamp
TEST(UsbLogUtilsTest, GetCurrentTimestamp_Valid) {
    char buf[32];
    EXPECT_EQ(get_current_timestamp(buf, sizeof(buf)), 0);
    ASSERT_GT(strlen(buf), 0);
}

TEST(UsbLogUtilsTest, GetCurrentTimestamp_InvalidBuffer) {
    EXPECT_EQ(get_current_timestamp(nullptr, 32), -1);
    char buf[10];
    EXPECT_EQ(get_current_timestamp(buf, sizeof(buf)), -1);
}

// Test perform_filesystem_sync
TEST(UsbLogUtilsTest, PerformFilesystemSync) {
    EXPECT_EQ(perform_filesystem_sync(), 0);
}

// Test copy_file_and_delete
TEST(UsbLogUtilsTest, CopyFileAndDelete_Success) {
    const char* src = "test_src.txt";
    const char* dst = "test_dst.txt";
    FILE* f = fopen(src, "w");
    fputs("testdata", f);
    fclose(f);

    EXPECT_EQ(copy_file_and_delete(src, dst), 0);

    FILE* f2 = fopen(dst, "r");
    ASSERT_NE(f2, nullptr);
    char buf[16] = {0};
    fread(buf, 1, sizeof(buf)-1, f2);
    fclose(f2);
    EXPECT_STREQ(buf, "testdata");
    unlink(dst);
}

TEST(UsbLogUtilsTest, CopyFileAndDelete_InvalidParams) {
    EXPECT_EQ(copy_file_and_delete(nullptr, "dst.txt"), -1);
    EXPECT_EQ(copy_file_and_delete("src.txt", nullptr), -1);
}

TEST(UsbLogUtilsTest, CopyFileAndDelete_SourceMissing) {
    EXPECT_EQ(copy_file_and_delete("no_such_file.txt", "dst.txt"), -1);
}

// reload_syslog_service is hard to test directly due to system dependencies,
// but you can stub getDevicePropertyData/getIncludePropertyData and test return values.
TEST(UsbLogUtilsTest, ReloadSyslogService_NotEnabled) {
    EXPECT_EQ(reload_syslog_service(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    return result;
}
