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

#include <gtest/gtest.h>
#include "usb_log_archive.h"
#include <string>
#include <cstdio>
#include <sys/stat.h>

// Mocks and stubs for dependencies
extern "C" {
    int get_current_timestamp(char *buf, size_t len) {
        strncpy(buf, "01/01/26-12:00:00", len-1);
        buf[len-1] = '\0';
        return 0;
    }
    int copy_file_and_delete(const char *src, const char *dst) {
        // Simulate successful copy
        return 0;
    }
    void RDK_LOG(int level, int module, const char *fmt, ...) {}
}

class UsbLogArchiveTest : public ::testing::Test {
protected:
    std::string temp_dir;
    void SetUp() override {
        temp_dir = "./test_usb_log_dir";
        mkdir(temp_dir.c_str(), 0777);
    }
    void TearDown() override {
        rmdir(temp_dir.c_str());
    }
};

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_Success) {
    char archive_path[256] = "./test_usb_log_dir/test_archive.tar.gz";
    int ret = create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55");
    EXPECT_EQ(ret, 0);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_InvalidParams) {
    char archive_path[256] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(nullptr, archive_path, "00:11:22:33:44:55"), -1);
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), nullptr, "00:11:22:33:44:55"), -1);
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), archive_path, nullptr), -1);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_SourceDirMissing) {
    char archive_path[256] = "./test_usb_log_dir/test_archive.tar.gz";
    std::string missing_dir = "./does_not_exist";
    EXPECT_EQ(create_usb_log_archive(missing_dir.c_str(), archive_path, "00:11:22:33:44:55"), -2);
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    return result;
}

