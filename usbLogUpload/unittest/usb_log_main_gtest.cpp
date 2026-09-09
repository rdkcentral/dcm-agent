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

/**
 * @file usb_log_main_gtest.cpp
 * @brief Google Test unit tests for USB log upload main module
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../uploadstblogs/unittest/mocks/mock_file_operations.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

extern "C" {
#include "usb_log_main.h"
#include "usb_log_validation.h"
#include "usb_log_file_manager.h"
#include "usb_log_archive.h"
#include "usb_log_utils.h"
#include "uploadstblogs_types.h"
}

/* Controllable mock state for uploadstblogs / system library functions */
static bool g_mock_get_mac_result = true;
static char g_mock_mac_value[32] = "AA:BB:CC:DD:EE:FF";
static bool g_mock_gen_archive_result = true;
static char g_mock_archive_name[256] = "AABBCCDDEEFF_Logs_20260828.tgz";
static int g_mock_create_archive_result = 0;
static bool g_mock_remove_dir_result = true;

extern "C" {
    int rdk_logger_init(const char*) { return 0; }
    int getDevicePropertyData(const char*, char* buf, unsigned int sz) {
        strncpy(buf, "false", sz - 1);
        buf[sz - 1] = '\0';
        return UTILS_SUCCESS;
    }
    int getIncludePropertyData(const char*, char* buf, unsigned int sz) {
        strncpy(buf, "/tmp/test_logs", sz - 1);
        buf[sz - 1] = '\0';
        return UTILS_SUCCESS;
    }
    bool get_mac_address(char* buf, size_t sz) {
        if (!g_mock_get_mac_result) return false;
        strncpy(buf, g_mock_mac_value, sz - 1);
        buf[sz - 1] = '\0';
        return true;
    }
    bool generate_archive_name(char* buf, size_t sz, const char*, const char*) {
        if (!g_mock_gen_archive_result) return false;
        strncpy(buf, g_mock_archive_name, sz - 1);
        buf[sz - 1] = '\0';
        return true;
    }
    int create_archive(RuntimeContext* ctx, SessionState* session, const char* dir) {
        (void)ctx; (void)dir;
        if (session && g_mock_create_archive_result == 0) {
            strncpy(session->archive_file, g_mock_archive_name,
                    sizeof(session->archive_file) - 1);
        }
        return g_mock_create_archive_result;
    }
    bool remove_directory(const char*) { return g_mock_remove_dir_result; }
}

class UsbLogMainTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_get_mac_result = true;
        strncpy(g_mock_mac_value, "AA:BB:CC:DD:EE:FF", sizeof(g_mock_mac_value));
        g_mock_gen_archive_result = true;
        strncpy(g_mock_archive_name, "AABBCCDDEEFF_Logs_20260828.tgz",
                sizeof(g_mock_archive_name));
        g_mock_create_archive_result = 0;
        g_mock_remove_dir_result = true;
        g_mockFileOperations = nullptr;
    }

    void TearDown() override {
        if (g_mockFileOperations) {
            delete g_mockFileOperations;
            g_mockFileOperations = nullptr;
        }
    }
};

TEST_F(UsbLogMainTest, ExecuteWithValidInputTest) {
    const char* test_mount = "/tmp";
    EXPECT_NE(usb_log_upload_execute(test_mount), USB_LOG_ERROR_USB_NOT_MOUNTED);
}

TEST_F(UsbLogMainTest, ExecuteWithInvalidInputTest) {
    EXPECT_NE(usb_log_upload_execute(nullptr), USB_LOG_SUCCESS);
}

TEST_F(UsbLogMainTest, ExecuteWithEmptyPath) {
    EXPECT_NE(usb_log_upload_execute(""), USB_LOG_SUCCESS);
}

TEST_F(UsbLogMainTest, ExecuteWithNonexistentPath) {
    EXPECT_EQ(usb_log_upload_execute("/nonexistent_usb_mount"), 2);
}

TEST_F(UsbLogMainTest, ExecuteCreateUsbLogDirFails) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(false));
    ON_CALL(*g_mockFileOperations, create_directory(::testing::_))
        .WillByDefault(::testing::Return(false));

    EXPECT_NE(usb_log_upload_execute("/tmp"), USB_LOG_SUCCESS);
}

TEST_F(UsbLogMainTest, ExecuteGetMacAddressFails) {
    g_mock_get_mac_result = false;
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(true));

    EXPECT_EQ(usb_log_upload_execute("/tmp"), USB_LOG_ERROR_GENERAL);
}

TEST_F(UsbLogMainTest, ExecuteGenerateArchiveNameFails) {
    g_mock_gen_archive_result = false;
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(true));

    EXPECT_EQ(usb_log_upload_execute("/tmp"), USB_LOG_ERROR_GENERAL);
}

/* create_directory mock returns true but real dir doesn't exist → access() fails */
TEST_F(UsbLogMainTest, ExecuteCreateTempDirAccessFails) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(true));
    ON_CALL(*g_mockFileOperations, create_directory(::testing::_))
        .WillByDefault(::testing::Return(true));

    int ret = usb_log_upload_execute("/tmp");
    EXPECT_NE(ret, USB_LOG_SUCCESS);
}

/* create_directory mock returns false → immediate failure */
TEST_F(UsbLogMainTest, ExecuteCreateTempDirCreateFails) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(true));
    ON_CALL(*g_mockFileOperations, create_directory(::testing::_))
        .WillByDefault(::testing::Return(false));

    EXPECT_EQ(usb_log_upload_execute("/tmp"), USB_LOG_ERROR_WRITE_ERROR);
}

/* --- Direct tests for create_usb_log_directory --- */

TEST_F(UsbLogMainTest, CreateUsbLogDirNullParam) {
    EXPECT_EQ(create_usb_log_directory(nullptr), -1);
}

TEST_F(UsbLogMainTest, CreateUsbLogDirAlreadyExists) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(true));

    EXPECT_EQ(create_usb_log_directory("/tmp/Log"), 0);
}

TEST_F(UsbLogMainTest, CreateUsbLogDirCreateFails) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(false));
    ON_CALL(*g_mockFileOperations, create_directory(::testing::_))
        .WillByDefault(::testing::Return(false));

    EXPECT_EQ(create_usb_log_directory("/tmp/Log"), -2);
}

TEST_F(UsbLogMainTest, CreateUsbLogDirCreateSucceeds) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, dir_exists(::testing::_))
        .WillByDefault(::testing::Return(false));
    ON_CALL(*g_mockFileOperations, create_directory(::testing::_))
        .WillByDefault(::testing::Return(true));

    EXPECT_EQ(create_usb_log_directory("/tmp/Log"), 0);
}

/* --- Direct tests for move_log_files --- */

TEST_F(UsbLogMainTest, MoveLogFilesNullSource) {
    EXPECT_EQ(move_log_files(nullptr, "/tmp"), -1);
}

TEST_F(UsbLogMainTest, MoveLogFilesNullDest) {
    EXPECT_EQ(move_log_files("/tmp", nullptr), -1);
}

TEST_F(UsbLogMainTest, MoveLogFilesBothNull) {
    EXPECT_EQ(move_log_files(nullptr, nullptr), -1);
}

TEST_F(UsbLogMainTest, MoveLogFilesInvalidSourceDir) {
    EXPECT_EQ(move_log_files("/nonexistent_source_dir", "/tmp"), -2);
}

TEST_F(UsbLogMainTest, MoveLogFilesEmptyDir) {
    char src[] = "/tmp/usb_test_move_src_XXXXXX";
    ASSERT_NE(mkdtemp(src), nullptr);
    EXPECT_EQ(move_log_files(src, "/tmp"), 0);
    rmdir(src);
}

TEST_F(UsbLogMainTest, MoveLogFilesWithFile) {
    char src[] = "/tmp/usb_test_move_src2_XXXXXX";
    char dst[] = "/tmp/usb_test_move_dst2_XXXXXX";
    ASSERT_NE(mkdtemp(src), nullptr);
    ASSERT_NE(mkdtemp(dst), nullptr);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/testfile.log", src);
    FILE* f = fopen(filepath, "w");
    ASSERT_NE(f, nullptr);
    fputs("logdata", f);
    fclose(f);

    EXPECT_EQ(move_log_files(src, dst), 0);

    char dstfile[512];
    snprintf(dstfile, sizeof(dstfile), "%s/testfile.log", dst);
    EXPECT_EQ(access(dstfile, F_OK), 0);

    unlink(dstfile);
    rmdir(src);
    rmdir(dst);
}

/* --- Direct tests for cleanup_temporary_files --- */

TEST_F(UsbLogMainTest, CleanupTempFilesNull) {
    EXPECT_EQ(cleanup_temporary_files(nullptr), -1);
}

TEST_F(UsbLogMainTest, CleanupTempFilesRemoveFails) {
    g_mock_remove_dir_result = false;
    EXPECT_EQ(cleanup_temporary_files("/tmp/some_dir"), -2);
}

TEST_F(UsbLogMainTest, CleanupTempFilesSuccess) {
    g_mock_remove_dir_result = true;
    EXPECT_EQ(cleanup_temporary_files("/tmp/some_dir"), 0);
}

/* --- Direct tests for create_temporary_directory --- */

TEST_F(UsbLogMainTest, CreateTempDirNullFileName) {
    char buf[256];
    EXPECT_EQ(create_temporary_directory(nullptr, buf, sizeof(buf)), -1);
}

TEST_F(UsbLogMainTest, CreateTempDirNullBuffer) {
    EXPECT_EQ(create_temporary_directory("test", nullptr, 256), -1);
}

TEST_F(UsbLogMainTest, CreateTempDirZeroBufferSize) {
    char buf[256];
    EXPECT_EQ(create_temporary_directory("test", buf, 0), -1);
}

TEST_F(UsbLogMainTest, CreateTempDirCreateDirFails) {
    g_mockFileOperations = new ::testing::NiceMock<MockFileOperations>();
    ON_CALL(*g_mockFileOperations, create_directory(::testing::_))
        .WillByDefault(::testing::Return(false));

    char buf[256];
    EXPECT_EQ(create_temporary_directory("testfile", buf, sizeof(buf)), 3);
}

/* --- Direct tests for create_usb_log_archive --- */

TEST_F(UsbLogMainTest, CreateArchiveNullSource) {
    EXPECT_EQ(create_usb_log_archive(nullptr, "/tmp/a.tgz", "AA:BB"), -1);
}

TEST_F(UsbLogMainTest, CreateArchiveNullPath) {
    EXPECT_EQ(create_usb_log_archive("/tmp", nullptr, "AA:BB"), -1);
}

TEST_F(UsbLogMainTest, CreateArchiveNullMac) {
    EXPECT_EQ(create_usb_log_archive("/tmp", "/tmp/a.tgz", nullptr), -1);
}

TEST_F(UsbLogMainTest, CreateArchiveNonexistentSource) {
    EXPECT_EQ(create_usb_log_archive("/nonexistent", "/tmp/a.tgz", "AA:BB"), -2);
}

TEST_F(UsbLogMainTest, CreateArchiveCreateFails) {
    g_mock_create_archive_result = -1;
    EXPECT_EQ(create_usb_log_archive("/tmp", "/tmp/a.tgz", "AA:BB"), 3);
}

TEST_F(UsbLogMainTest, MainArgumentValidationTest) {
    char* test_argv[] = {(char*)"usblogupload", (char*)"/tmp/test_usb"};
    EXPECT_EQ(validate_input_parameters(2, test_argv), 0);
    (void)test_argv;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    if (g_mockFileOperations) {
        delete g_mockFileOperations;
        g_mockFileOperations = nullptr;
    }

    return result;
}
