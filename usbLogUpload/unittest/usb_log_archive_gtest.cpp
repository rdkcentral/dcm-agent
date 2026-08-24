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
#include "usb_log_archive.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>

/* Controllable mock return values */
static int g_timestamp_retval;
static int g_create_archive_retval;
static int g_copy_file_retval;
static bool g_generate_archive_name_retval;
static char g_session_archive_file[256];

extern "C" {
    int get_current_timestamp(char *buf, size_t len) {
        if (g_timestamp_retval != 0) {
            return g_timestamp_retval;
        }
        strncpy(buf, "01/01/26-12:00:00", len - 1);
        buf[len - 1] = '\0';
        return 0;
    }

    int copy_file_and_delete(const char *src, const char *dst) {
        return g_copy_file_retval;
    }

    int create_archive(RuntimeContext *ctx, SessionState *session, const char *source_dir) {
        if (g_create_archive_retval == 0 && g_session_archive_file[0] != '\0') {
            strncpy(session->archive_file, g_session_archive_file,
                    sizeof(session->archive_file) - 1);
            session->archive_file[sizeof(session->archive_file) - 1] = '\0';
        }
        return g_create_archive_retval;
    }

    bool generate_archive_name(char *buffer, size_t buffer_size,
                               const char *mac_address, const char *prefix) {
        if (!g_generate_archive_name_retval) {
            return false;
        }
        snprintf(buffer, buffer_size, "%s_%s_Logs.tar.gz", prefix, mac_address);
        return true;
    }

    void RDK_LOG(int level, int module, const char *fmt, ...) {}
}

class UsbLogArchiveTest : public ::testing::Test {
protected:
    std::string temp_dir;

    void SetUp() override {
        temp_dir = "./test_usb_log_dir";
        mkdir(temp_dir.c_str(), 0777);

        /* Reset all mock controls to success defaults */
        g_timestamp_retval = 0;
        g_create_archive_retval = 0;
        g_copy_file_retval = 0;
        g_generate_archive_name_retval = true;
        memset(g_session_archive_file, 0, sizeof(g_session_archive_file));
    }

    void TearDown() override {
        rmdir(temp_dir.c_str());
    }
};

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_Success) {
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    int ret = create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55");
    EXPECT_EQ(ret, 0);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_NullSourceDir) {
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(nullptr, archive_path, "00:11:22:33:44:55"), -1);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_NullArchivePath) {
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), nullptr, "00:11:22:33:44:55"), -1);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_NullMacAddress) {
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), archive_path, nullptr), -1);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_SourceDirMissing) {
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive("./does_not_exist", archive_path, "00:11:22:33:44:55"), -2);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_SourcePathIsFile) {
    std::string filepath = temp_dir + "/afile.txt";
    std::ofstream f(filepath);
    f << "data";
    f.close();

    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(filepath.c_str(), archive_path, "00:11:22:33:44:55"), -2);
    remove(filepath.c_str());
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_TimestampFails_UsesFallback) {
    g_timestamp_retval = -1;
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    int ret = create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55");
    EXPECT_EQ(ret, 0);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_CreateArchiveFails) {
    g_create_archive_retval = -1;
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55"), 3);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_SessionArchiveFileSet) {
    strncpy(g_session_archive_file, "session_archive.tar.gz", sizeof(g_session_archive_file) - 1);
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    int ret = create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55");
    EXPECT_EQ(ret, 0);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_FallbackToGenerateArchiveName) {
    /* session.archive_file stays empty, triggers generate_archive_name path */
    memset(g_session_archive_file, 0, sizeof(g_session_archive_file));
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    int ret = create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55");
    EXPECT_EQ(ret, 0);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_GenerateArchiveNameFails) {
    memset(g_session_archive_file, 0, sizeof(g_session_archive_file));
    g_generate_archive_name_retval = false;
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55"), 3);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_CopyFileAndDeleteFails) {
    g_copy_file_retval = -1;
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55"), 3);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_CopyFailsWithSessionFile) {
    strncpy(g_session_archive_file, "test.tar.gz", sizeof(g_session_archive_file) - 1);
    g_copy_file_retval = -1;
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive(temp_dir.c_str(), archive_path, "00:11:22:33:44:55"), 3);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_AllNullParams) {
    EXPECT_EQ(create_usb_log_archive(nullptr, nullptr, nullptr), -1);
}

TEST_F(UsbLogArchiveTest, CreateUsbLogArchive_EmptySourceDir) {
    char archive_path[] = "./test_usb_log_dir/test_archive.tar.gz";
    EXPECT_EQ(create_usb_log_archive("", archive_path, "00:11:22:33:44:55"), -2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

