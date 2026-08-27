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

// Test get_current_timestamp with exact minimum buffer size (20 bytes)
TEST(UsbLogUtilsTest, GetCurrentTimestamp_ExactMinBuffer) {
    char buf[20];
    EXPECT_EQ(get_current_timestamp(buf, sizeof(buf)), 0);
    EXPECT_GT(strlen(buf), 0u);
}

// Verify timestamp format MM/DD/YY-HH:MM:SS
TEST(UsbLogUtilsTest, GetCurrentTimestamp_FormatCheck) {
    char buf[32];
    EXPECT_EQ(get_current_timestamp(buf, sizeof(buf)), 0);
    EXPECT_EQ(strlen(buf), 17u);
    EXPECT_EQ(buf[2], '/');
    EXPECT_EQ(buf[5], '/');
    EXPECT_EQ(buf[8], '-');
    EXPECT_EQ(buf[11], ':');
    EXPECT_EQ(buf[14], ':');
}

// Test copy of an empty source file
TEST(UsbLogUtilsTest, CopyFileAndDelete_EmptyFile) {
    const char* src = "test_empty_src.txt";
    const char* dst = "test_empty_dst.txt";
    FILE* f = fopen(src, "w");
    ASSERT_NE(f, nullptr);
    fclose(f);

    EXPECT_EQ(copy_file_and_delete(src, dst), 0);

    FILE* f2 = fopen(dst, "r");
    ASSERT_NE(f2, nullptr);
    char c;
    EXPECT_EQ(fread(&c, 1, 1, f2), 0u);
    fclose(f2);
    unlink(dst);
}

// Test copy of a file larger than 8KB to exercise multi-chunk read loop
TEST(UsbLogUtilsTest, CopyFileAndDelete_LargeFile) {
    const char* src = "test_large_src.bin";
    const char* dst = "test_large_dst.bin";
    const size_t file_size = 8192 * 3 + 100; /* ~24.1 KB */

    FILE* f = fopen(src, "wb");
    ASSERT_NE(f, nullptr);
    for (size_t i = 0; i < file_size; i++) {
        unsigned char byte = (unsigned char)(i & 0xFF);
        fwrite(&byte, 1, 1, f);
    }
    fclose(f);

    EXPECT_EQ(copy_file_and_delete(src, dst), 0);

    /* Verify content */
    FILE* f2 = fopen(dst, "rb");
    ASSERT_NE(f2, nullptr);
    for (size_t i = 0; i < file_size; i++) {
        unsigned char byte;
        ASSERT_EQ(fread(&byte, 1, 1, f2), 1u);
        EXPECT_EQ(byte, (unsigned char)(i & 0xFF));
    }
    fclose(f2);
    unlink(dst);
}

// Test copy with binary data including null bytes
TEST(UsbLogUtilsTest, CopyFileAndDelete_BinaryData) {
    const char* src = "test_bin_src.bin";
    const char* dst = "test_bin_dst.bin";
    unsigned char data[] = {0x00, 0xFF, 0x01, 0xFE, 0x00, 0x80, 0x7F, 0x00};
    size_t data_len = sizeof(data);

    FILE* f = fopen(src, "wb");
    ASSERT_NE(f, nullptr);
    fwrite(data, 1, data_len, f);
    fclose(f);

    EXPECT_EQ(copy_file_and_delete(src, dst), 0);

    FILE* f2 = fopen(dst, "rb");
    ASSERT_NE(f2, nullptr);
    unsigned char readback[sizeof(data)];
    EXPECT_EQ(fread(readback, 1, data_len, f2), data_len);
    fclose(f2);
    EXPECT_EQ(memcmp(data, readback, data_len), 0);
    unlink(dst);
}

// Verify the source file is actually deleted after a successful copy
TEST(UsbLogUtilsTest, CopyFileAndDelete_SourceRemovedAfterCopy) {
    const char* src = "test_rm_src.txt";
    const char* dst = "test_rm_dst.txt";
    FILE* f = fopen(src, "w");
    ASSERT_NE(f, nullptr);
    fputs("delete me", f);
    fclose(f);

    EXPECT_EQ(copy_file_and_delete(src, dst), 0);
    EXPECT_NE(access(src, F_OK), 0); /* source must not exist */
    unlink(dst);
}

// Test copy when destination directory doesn't exist (dest fopen fails)
TEST(UsbLogUtilsTest, CopyFileAndDelete_DestDirMissing) {
    const char* src = "test_destfail_src.txt";
    FILE* f = fopen(src, "w");
    ASSERT_NE(f, nullptr);
    fputs("data", f);
    fclose(f);

    EXPECT_EQ(copy_file_and_delete(src, "/no_such_dir/test_dst.txt"), -1);
    unlink(src);
}

// Test copy when both parameters are null
TEST(UsbLogUtilsTest, CopyFileAndDelete_BothNull) {
    EXPECT_EQ(copy_file_and_delete(nullptr, nullptr), -1);
}

// Test overwriting an existing destination file
TEST(UsbLogUtilsTest, CopyFileAndDelete_OverwriteExisting) {
    const char* src = "test_ow_src.txt";
    const char* dst = "test_ow_dst.txt";

    FILE* f1 = fopen(dst, "w");
    ASSERT_NE(f1, nullptr);
    fputs("old content", f1);
    fclose(f1);

    FILE* f2 = fopen(src, "w");
    ASSERT_NE(f2, nullptr);
    fputs("new content", f2);
    fclose(f2);

    EXPECT_EQ(copy_file_and_delete(src, dst), 0);

    FILE* f3 = fopen(dst, "r");
    ASSERT_NE(f3, nullptr);
    char buf[32] = {0};
    fread(buf, 1, sizeof(buf) - 1, f3);
    fclose(f3);
    EXPECT_STREQ(buf, "new content");
    unlink(dst);
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
