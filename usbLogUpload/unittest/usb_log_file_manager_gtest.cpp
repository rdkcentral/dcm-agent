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
 * @file usb_log_file_manager_gtest.cpp
 * @brief Google Test unit tests for USB log upload file manager module
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "../../uploadstblogs/unittest/mocks/mock_file_operations.h"

extern "C" {
#include "usb_log_file_manager.h"
}

/**
 * @brief Utility function to recursively remove directory and contents
 */
static int remove_directory_recursive(const char *path) {
    if (ftw(path, [](const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
        if (typeflag == FTW_DP)
            rmdir(fpath);
        else if (typeflag == FTW_F || typeflag == FTW_SL)
            unlink(fpath);
        return 0;
    }, 20) != 0) {
        return -1;
    }
    return rmdir(path);
}

/**
 * @brief Test fixture for USB log file manager module tests
 */
class UsbLogFileManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for each test case
        test_usb_path = "/tmp/test_usb_" + std::to_string(getpid());
        test_temp_path = "/tmp/test_temp_" + std::to_string(getpid());

        // Create test directories
        mkdir(test_usb_path.c_str(), 0755);
        mkdir(test_temp_path.c_str(), 0755);
    }

    void TearDown() override {
        // Cleanup for each test case
        remove_directory_recursive(test_usb_path.c_str());
        remove_directory_recursive(test_temp_path.c_str());
    }

    std::string test_usb_path;
    std::string test_temp_path;
};

/**
 * @brief Test USB log directory creation with valid path
 */
TEST_F(UsbLogFileManagerTest, CreateUsbLogDirectorySuccessTest) {
    std::string usb_log_dir = test_usb_path + "/logs";

    // Directory should not exist yet
    EXPECT_FALSE(access(usb_log_dir.c_str(), F_OK) == 0);

    // Create directory should succeed
    EXPECT_EQ(create_usb_log_directory(usb_log_dir.c_str()), 0);

}

/**
 * @brief Test USB log directory creation when directory already exists
 */
TEST_F(UsbLogFileManagerTest, CreateUsbLogDirectoryAlreadyExistsTest) {
    // Directory already exists from SetUp
    EXPECT_EQ(create_usb_log_directory(test_usb_path.c_str()), 0);
}

/**
 * @brief Test USB log directory creation with NULL path
 */
TEST_F(UsbLogFileManagerTest, CreateUsbLogDirectoryNullPathTest) {
    EXPECT_LT(create_usb_log_directory(nullptr), 0);
}

/**
 * @brief Test log file movement with valid files
 */
TEST_F(UsbLogFileManagerTest, MoveLogFilesSuccessTest) {
    std::string source_dir = test_usb_path + "/source";
    std::string dest_dir = test_usb_path + "/dest";

    mkdir(source_dir.c_str(), 0755);
    mkdir(dest_dir.c_str(), 0755);

    // Create test files in source directory
    std::string test_file1 = source_dir + "/test1.log";
    std::string test_file2 = source_dir + "/test2.log";

    FILE* f1 = fopen(test_file1.c_str(), "w");
    FILE* f2 = fopen(test_file2.c_str(), "w");
    fprintf(f1, "Test log content 1");
    fprintf(f2, "Test log content 2");
    fclose(f1);
    fclose(f2);

    // Move files
    EXPECT_EQ(move_log_files(source_dir.c_str(), dest_dir.c_str()), 0);

    // Files should now be in destination
    EXPECT_TRUE(access((dest_dir + "/test1.log").c_str(), F_OK) == 0);
    EXPECT_TRUE(access((dest_dir + "/test2.log").c_str(), F_OK) == 0);

    // Files should not be in source
    EXPECT_FALSE(access(test_file1.c_str(), F_OK) == 0);
    EXPECT_FALSE(access(test_file2.c_str(), F_OK) == 0);
}

/**
 * @brief Test log file movement with empty source directory
 */
TEST_F(UsbLogFileManagerTest, MoveLogFilesEmptySourceTest) {
    std::string source_dir = test_usb_path + "/empty_source";
    std::string dest_dir = test_usb_path + "/dest";

    mkdir(source_dir.c_str(), 0755);
    mkdir(dest_dir.c_str(), 0755);

    // Move from empty directory should succeed with no files moved
    EXPECT_EQ(move_log_files(source_dir.c_str(), dest_dir.c_str()), 0);
}

/**
 * @brief Test log file movement with NULL source path
 */
TEST_F(UsbLogFileManagerTest, MoveLogFilesNullSourceTest) {
    std::string dest_dir = test_usb_path + "/dest";
    mkdir(dest_dir.c_str(), 0755);

    EXPECT_LT(move_log_files(nullptr, dest_dir.c_str()), 0);
}

/**
 * @brief Test log file movement with NULL destination path
 */
TEST_F(UsbLogFileManagerTest, MoveLogFilesNullDestTest) {
    std::string source_dir = test_usb_path + "/source";
    mkdir(source_dir.c_str(), 0755);

    EXPECT_LT(move_log_files(source_dir.c_str(), nullptr), 0);
}

/**
 * @brief Test log file movement with non-existent source directory
 */
TEST_F(UsbLogFileManagerTest, MoveLogFilesNonExistentSourceTest) {
    std::string source_dir = test_usb_path + "/nonexistent";
    std::string dest_dir = test_usb_path + "/dest";

    mkdir(dest_dir.c_str(), 0755);

    // Should fail when source directory doesn't exist
    EXPECT_LT(move_log_files(source_dir.c_str(), dest_dir.c_str()), 0);
}

/**
 * @brief Test temporary file cleanup with valid directory
 */
TEST_F(UsbLogFileManagerTest, CleanupTemporaryFilesSuccessTest) {
    std::string temp_cleanup_dir = test_temp_path + "/cleanup_test";
    mkdir(temp_cleanup_dir.c_str(), 0755);

    // Create some test files
    std::string test_file = temp_cleanup_dir + "/test.log";
    FILE* f = fopen(test_file.c_str(), "w");
    fprintf(f, "Test content");
    fclose(f);

    // Directory should exist
    EXPECT_TRUE(access(temp_cleanup_dir.c_str(), F_OK) == 0);

    // Cleanup should succeed
    EXPECT_EQ(cleanup_temporary_files(temp_cleanup_dir.c_str()), 0);

    // Directory should be removed
    EXPECT_FALSE(access(temp_cleanup_dir.c_str(), F_OK) == 0);
}

/**
 * @brief Test temporary file cleanup with NULL path
 */
TEST_F(UsbLogFileManagerTest, CleanupTemporaryFilesNullPathTest) {
    EXPECT_LT(cleanup_temporary_files(nullptr), 0);
}

/**
 * @brief Test temporary file cleanup with non-existent directory
 */
TEST_F(UsbLogFileManagerTest, CleanupTemporaryFilesNonExistentTest) {
    std::string nonexistent_path = test_temp_path + "/nonexistent";

    // Should fail when directory doesn't exist
    EXPECT_LT(cleanup_temporary_files(nonexistent_path.c_str()), 0);
}

/**
 * @brief Test temporary directory creation with valid input
 */
TEST_F(UsbLogFileManagerTest, CreateTemporaryDirectorySuccessTest) {
    char temp_dir_path[256];
    const char* file_name = "test_usb_logs";

    // Create should succeed
    int result = create_temporary_directory(file_name, temp_dir_path, sizeof(temp_dir_path));
    EXPECT_EQ(result, 0);

    // Verify directory was created
    EXPECT_TRUE(access(temp_dir_path, F_OK) == 0);

    // Cleanup
    remove_directory_recursive(temp_dir_path);
}

/**
 * @brief Test temporary directory creation with NULL buffer
 */
TEST_F(UsbLogFileManagerTest, CreateTemporaryDirectoryNullBufferTest) {
    EXPECT_LT(create_temporary_directory("test", nullptr, 256), 0);
}

/**
 * @brief Test temporary directory creation with insufficient buffer size
 */
TEST_F(UsbLogFileManagerTest, CreateTemporaryDirectorySmallBufferTest) {
    char temp_dir_path[5];  // Too small

    // Should fail with insufficient buffer
    EXPECT_LT(create_temporary_directory("someverylongfilenamethatshouldneverfit",
                                         temp_dir_path, sizeof(temp_dir_path)), 0);
}

/**
 * @brief Test temporary directory creation with NULL file name
 */
TEST_F(UsbLogFileManagerTest, CreateTemporaryDirectoryNullFileNameTest) {
    char temp_dir_path[256];

    EXPECT_LT(create_temporary_directory(nullptr, temp_dir_path, sizeof(temp_dir_path)), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    // Ensure global mock is cleaned up
    if (g_mockFileOperations) {
        delete g_mockFileOperations;
        g_mockFileOperations = nullptr;
    }

    return result;
}
