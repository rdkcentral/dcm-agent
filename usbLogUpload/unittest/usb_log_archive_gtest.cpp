// Copyright 2026
// Unit tests for usb_log_archive.c
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

