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
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>

#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

#ifndef RDK_API_SUCCESS
#define RDK_API_SUCCESS 0
#endif
#ifndef RDK_API_FAILURE
#define RDK_API_FAILURE -1
#endif

// Mock common_utilities functions used by file_operations.c
extern "C" {

static bool g_filePresentCheck_result = true;
static bool g_folderCheck_result = true;
static bool g_createDir_result = true;
static bool g_removeFile_result = true;
static bool g_emptyFolder_result = true;
static bool g_copyFiles_result = true;
static int g_getFileSize_result = 1024;

int filePresentCheck(const char* filepath) {
    return g_filePresentCheck_result ? RDK_API_SUCCESS : RDK_API_FAILURE;
}

int folderCheck(char* dirpath) {
    return g_folderCheck_result ? 1 : 0;
}

int createDir(char* dirpath) {
    // Actually create the dir for integration tests
    if (g_createDir_result) {
        mkdir(dirpath, 0755);
        return RDK_API_SUCCESS;
    }
    return RDK_API_FAILURE;
}

int removeFile(char* filepath) {
    if (g_removeFile_result) {
        unlink(filepath);
        return RDK_API_SUCCESS;
    }
    return RDK_API_FAILURE;
}

int emptyFolder(char* dirpath) {
    if (!g_emptyFolder_result) return RDK_API_FAILURE;
    // Simple mock: remove files in dir
    DIR* dir = opendir(dirpath);
    if (!dir) return RDK_API_FAILURE;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
        unlink(path);
    }
    closedir(dir);
    return RDK_API_SUCCESS;
}

int copyFiles(char* src, char* dest) {
    return g_copyFiles_result ? RDK_API_SUCCESS : RDK_API_FAILURE;
}

int getFileSize(const char* filepath) {
    return g_getFileSize_result;
}

} // extern "C"

// Prevent system_utils.h from being included
#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H
#endif

#include "file_operations.h"
#include "../src/file_operations.c"

using namespace testing;

class FileOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_filePresentCheck_result = true;
        g_folderCheck_result = true;
        g_createDir_result = true;
        g_removeFile_result = true;
        g_emptyFolder_result = true;
        g_copyFiles_result = true;
        g_getFileSize_result = 1024;

        test_dir = std::string("/tmp/fileops_test_") + std::to_string(getpid());
        mkdir(test_dir.c_str(), 0755);
    }

    void TearDown() override {
        std::string cmd = "rm -rf " + test_dir;
        system(cmd.c_str());
    }

    void CreateFile(const char* name, const char* content = "test") {
        std::string path = test_dir + "/" + name;
        FILE* fp = fopen(path.c_str(), "w");
        if (fp) { fprintf(fp, "%s", content); fclose(fp); }
    }

    std::string test_dir;
};

// ==================== file_exists TESTS ====================

TEST_F(FileOperationsTest, FileExists_NullPath) {
    EXPECT_FALSE(file_exists(nullptr));
}

TEST_F(FileOperationsTest, FileExists_EmptyPath) {
    EXPECT_FALSE(file_exists(""));
}

TEST_F(FileOperationsTest, FileExists_Success) {
    g_filePresentCheck_result = true;
    EXPECT_TRUE(file_exists("/tmp/some_file"));
}

TEST_F(FileOperationsTest, FileExists_NotFound) {
    g_filePresentCheck_result = false;
    EXPECT_FALSE(file_exists("/tmp/missing"));
}

// ==================== dir_exists TESTS ====================

TEST_F(FileOperationsTest, DirExists_NullPath) {
    EXPECT_FALSE(dir_exists(nullptr));
}

TEST_F(FileOperationsTest, DirExists_EmptyPath) {
    EXPECT_FALSE(dir_exists(""));
}

TEST_F(FileOperationsTest, DirExists_Success) {
    g_folderCheck_result = true;
    EXPECT_TRUE(dir_exists("/tmp"));
}

TEST_F(FileOperationsTest, DirExists_NotFound) {
    g_folderCheck_result = false;
    EXPECT_FALSE(dir_exists("/nonexistent"));
}

// ==================== join_path TESTS ====================

TEST_F(FileOperationsTest, JoinPath_NullBuffer) {
    EXPECT_FALSE(join_path(nullptr, 256, "/tmp", "file.txt"));
}

TEST_F(FileOperationsTest, JoinPath_NullDir) {
    char buf[256];
    EXPECT_FALSE(join_path(buf, sizeof(buf), nullptr, "file.txt"));
}

TEST_F(FileOperationsTest, JoinPath_NullFilename) {
    char buf[256];
    EXPECT_FALSE(join_path(buf, sizeof(buf), "/tmp", nullptr));
}

TEST_F(FileOperationsTest, JoinPath_Success) {
    char buf[256];
    EXPECT_TRUE(join_path(buf, sizeof(buf), "/tmp", "file.txt"));
    EXPECT_STREQ("/tmp/file.txt", buf);
}

TEST_F(FileOperationsTest, JoinPath_TrailingSlash) {
    char buf[256];
    EXPECT_TRUE(join_path(buf, sizeof(buf), "/tmp/", "file.txt"));
    EXPECT_STREQ("/tmp/file.txt", buf);
}

TEST_F(FileOperationsTest, JoinPath_BufferTooSmall) {
    char buf[5];
    EXPECT_FALSE(join_path(buf, sizeof(buf), "/tmp", "file.txt"));
}

// ==================== create_directory TESTS ====================

TEST_F(FileOperationsTest, CreateDirectory_NullPath) {
    EXPECT_FALSE(create_directory(nullptr));
}

TEST_F(FileOperationsTest, CreateDirectory_EmptyPath) {
    EXPECT_FALSE(create_directory(""));
}

TEST_F(FileOperationsTest, CreateDirectory_AlreadyExists) {
    g_folderCheck_result = true;
    EXPECT_TRUE(create_directory("/tmp"));
}

TEST_F(FileOperationsTest, CreateDirectory_CreateFails) {
    g_folderCheck_result = false;
    g_createDir_result = false;
    EXPECT_FALSE(create_directory("/tmp/fail_dir"));
}

// ==================== remove_file TESTS ====================

TEST_F(FileOperationsTest, RemoveFile_NullPath) {
    EXPECT_FALSE(remove_file(nullptr));
}

TEST_F(FileOperationsTest, RemoveFile_EmptyPath) {
    EXPECT_FALSE(remove_file(""));
}

TEST_F(FileOperationsTest, RemoveFile_FileNotExists) {
    g_filePresentCheck_result = false;
    EXPECT_TRUE(remove_file("/tmp/already_gone"));
}

TEST_F(FileOperationsTest, RemoveFile_Success) {
    g_filePresentCheck_result = true;
    g_removeFile_result = true;
    EXPECT_TRUE(remove_file("/tmp/somefile"));
}

TEST_F(FileOperationsTest, RemoveFile_Failure) {
    g_filePresentCheck_result = true;
    g_removeFile_result = false;
    EXPECT_FALSE(remove_file("/tmp/locked"));
}

// ==================== remove_directory TESTS ====================

TEST_F(FileOperationsTest, RemoveDirectory_NullPath) {
    EXPECT_FALSE(remove_directory(nullptr));
}

TEST_F(FileOperationsTest, RemoveDirectory_EmptyPath) {
    EXPECT_FALSE(remove_directory(""));
}

TEST_F(FileOperationsTest, RemoveDirectory_NotExists) {
    g_folderCheck_result = false;
    EXPECT_TRUE(remove_directory("/tmp/gone"));
}

TEST_F(FileOperationsTest, RemoveDirectory_EmptyFolderFails) {
    g_folderCheck_result = true;
    g_emptyFolder_result = false;
    EXPECT_FALSE(remove_directory("/tmp/stuck"));
}

// ==================== copy_file TESTS ====================

TEST_F(FileOperationsTest, CopyFile_NullSrc) {
    EXPECT_FALSE(copy_file(nullptr, "/tmp/dest"));
}

TEST_F(FileOperationsTest, CopyFile_NullDest) {
    EXPECT_FALSE(copy_file("/tmp/src", nullptr));
}

TEST_F(FileOperationsTest, CopyFile_EmptySrc) {
    EXPECT_FALSE(copy_file("", "/tmp/dest"));
}

TEST_F(FileOperationsTest, CopyFile_EmptyDest) {
    EXPECT_FALSE(copy_file("/tmp/src", ""));
}

TEST_F(FileOperationsTest, CopyFile_Success) {
    g_copyFiles_result = true;
    EXPECT_TRUE(copy_file("/tmp/src", "/tmp/dest"));
}

TEST_F(FileOperationsTest, CopyFile_Failure) {
    g_copyFiles_result = false;
    EXPECT_FALSE(copy_file("/tmp/src", "/tmp/dest"));
}

// ==================== get_file_size TESTS ====================

TEST_F(FileOperationsTest, GetFileSize_NullPath) {
    EXPECT_EQ(-1, get_file_size(nullptr));
}

TEST_F(FileOperationsTest, GetFileSize_EmptyPath) {
    EXPECT_EQ(-1, get_file_size(""));
}

TEST_F(FileOperationsTest, GetFileSize_Success) {
    g_getFileSize_result = 2048;
    EXPECT_EQ(2048L, get_file_size("/tmp/test"));
}

TEST_F(FileOperationsTest, GetFileSize_Error) {
    g_getFileSize_result = -1;
    EXPECT_EQ(-1L, get_file_size("/tmp/missing"));
}

// ==================== is_directory_empty TESTS ====================

TEST_F(FileOperationsTest, IsDirEmpty_NullPath) {
    EXPECT_FALSE(is_directory_empty(nullptr));
}

TEST_F(FileOperationsTest, IsDirEmpty_EmptyPath) {
    EXPECT_FALSE(is_directory_empty(""));
}

TEST_F(FileOperationsTest, IsDirEmpty_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_FALSE(is_directory_empty("/nonexistent"));
}

TEST_F(FileOperationsTest, IsDirEmpty_EmptyDir) {
    g_folderCheck_result = true;
    EXPECT_TRUE(is_directory_empty(test_dir.c_str()));
}

TEST_F(FileOperationsTest, IsDirEmpty_NonEmptyDir) {
    g_folderCheck_result = true;
    CreateFile("something.txt");
    EXPECT_FALSE(is_directory_empty(test_dir.c_str()));
}

// ==================== has_log_files TESTS ====================

TEST_F(FileOperationsTest, HasLogFiles_NullPath) {
    EXPECT_FALSE(has_log_files(nullptr));
}

TEST_F(FileOperationsTest, HasLogFiles_EmptyPath) {
    EXPECT_FALSE(has_log_files(""));
}

TEST_F(FileOperationsTest, HasLogFiles_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_FALSE(has_log_files("/nonexistent"));
}

TEST_F(FileOperationsTest, HasLogFiles_NoLogFiles) {
    g_folderCheck_result = true;
    CreateFile("data.bin");
    CreateFile("config.conf");
    EXPECT_FALSE(has_log_files(test_dir.c_str()));
}

TEST_F(FileOperationsTest, HasLogFiles_HasTxtFile) {
    g_folderCheck_result = true;
    CreateFile("system.txt");
    EXPECT_TRUE(has_log_files(test_dir.c_str()));
}

TEST_F(FileOperationsTest, HasLogFiles_HasLogFile) {
    g_folderCheck_result = true;
    CreateFile("messages.log");
    EXPECT_TRUE(has_log_files(test_dir.c_str()));
}

// ==================== write_file TESTS ====================

TEST_F(FileOperationsTest, WriteFile_NullPath) {
    EXPECT_FALSE(write_file(nullptr, "content"));
}

TEST_F(FileOperationsTest, WriteFile_EmptyPath) {
    EXPECT_FALSE(write_file("", "content"));
}

TEST_F(FileOperationsTest, WriteFile_NullContent) {
    EXPECT_FALSE(write_file("/tmp/test", nullptr));
}

TEST_F(FileOperationsTest, WriteFile_Success) {
    std::string path = test_dir + "/write_test.txt";
    EXPECT_TRUE(write_file(path.c_str(), "hello world"));

    FILE* fp = fopen(path.c_str(), "r");
    ASSERT_NE(nullptr, fp);
    char buf[64] = {0};
    fgets(buf, sizeof(buf), fp);
    fclose(fp);
    EXPECT_STREQ("hello world", buf);
}

// ==================== read_file TESTS ====================

TEST_F(FileOperationsTest, ReadFile_NullPath) {
    char buf[64];
    EXPECT_EQ(-1, read_file(nullptr, buf, sizeof(buf)));
}

TEST_F(FileOperationsTest, ReadFile_NullBuffer) {
    EXPECT_EQ(-1, read_file("/tmp/file", nullptr, 64));
}

TEST_F(FileOperationsTest, ReadFile_ZeroBufferSize) {
    char buf[64];
    EXPECT_EQ(-1, read_file("/tmp/file", buf, 0));
}

TEST_F(FileOperationsTest, ReadFile_Success) {
    std::string path = test_dir + "/read_test.txt";
    FILE* fp = fopen(path.c_str(), "w");
    ASSERT_NE(nullptr, fp);
    fprintf(fp, "test content");
    fclose(fp);

    char buf[64] = {0};
    int bytes = read_file(path.c_str(), buf, sizeof(buf));
    EXPECT_GT(bytes, 0);
    EXPECT_STREQ("test content", buf);
}

TEST_F(FileOperationsTest, ReadFile_FileNotFound) {
    char buf[64];
    EXPECT_EQ(-1, read_file("/tmp/nonexistent_xyz_abc", buf, sizeof(buf)));
}

// ==================== add_timestamp_to_files TESTS ====================

TEST_F(FileOperationsTest, AddTimestamp_NullPath) {
    EXPECT_EQ(-1, add_timestamp_to_files(nullptr));
}

TEST_F(FileOperationsTest, AddTimestamp_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(-1, add_timestamp_to_files("/nonexistent"));
}

TEST_F(FileOperationsTest, AddTimestamp_EmptyDir) {
    g_folderCheck_result = true;
    EXPECT_EQ(0, add_timestamp_to_files(test_dir.c_str()));
}

TEST_F(FileOperationsTest, AddTimestamp_RenamesFiles) {
    g_folderCheck_result = true;
    CreateFile("app.log");
    CreateFile("system.txt");

    EXPECT_EQ(0, add_timestamp_to_files(test_dir.c_str()));

    // Verify original files are gone (renamed)
    std::string orig1 = test_dir + "/app.log";
    std::string orig2 = test_dir + "/system.txt";
    EXPECT_NE(0, access(orig1.c_str(), F_OK));
    EXPECT_NE(0, access(orig2.c_str(), F_OK));
}

TEST_F(FileOperationsTest, AddTimestamp_SkipsBakFiles) {
    g_folderCheck_result = true;
    CreateFile("bak1_something");
    CreateFile("bak2_other");
    CreateFile("normal.log");

    EXPECT_EQ(0, add_timestamp_to_files(test_dir.c_str()));

    // bak files should still exist with original names
    std::string bak1 = test_dir + "/bak1_something";
    std::string bak2 = test_dir + "/bak2_other";
    EXPECT_EQ(0, access(bak1.c_str(), F_OK));
    EXPECT_EQ(0, access(bak2.c_str(), F_OK));
}

// ==================== remove_timestamp_from_files TESTS ====================

TEST_F(FileOperationsTest, RemoveTimestamp_NullPath) {
    EXPECT_EQ(-1, remove_timestamp_from_files(nullptr));
}

TEST_F(FileOperationsTest, RemoveTimestamp_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(-1, remove_timestamp_from_files("/nonexistent"));
}

TEST_F(FileOperationsTest, RemoveTimestamp_RoundTrip) {
    g_folderCheck_result = true;
    CreateFile("test.log");
    CreateFile("app.txt");

    // Add timestamps
    EXPECT_EQ(0, add_timestamp_to_files(test_dir.c_str()));

    // Remove timestamps
    EXPECT_EQ(0, remove_timestamp_from_files(test_dir.c_str()));

    // Original filenames should be restored
    std::string f1 = test_dir + "/test.log";
    std::string f2 = test_dir + "/app.txt";
    EXPECT_EQ(0, access(f1.c_str(), F_OK));
    EXPECT_EQ(0, access(f2.c_str(), F_OK));
}

// ==================== add_timestamp_to_files_uploadlogsnow TESTS ====================

TEST_F(FileOperationsTest, AddTimestampULN_NullPath) {
    EXPECT_EQ(-1, add_timestamp_to_files_uploadlogsnow(nullptr));
}

TEST_F(FileOperationsTest, AddTimestampULN_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(-1, add_timestamp_to_files_uploadlogsnow("/nonexistent"));
}

TEST_F(FileOperationsTest, AddTimestampULN_SkipsRebootLog) {
    g_folderCheck_result = true;
    CreateFile("reboot.log");
    CreateFile("normal.log");

    EXPECT_EQ(0, add_timestamp_to_files_uploadlogsnow(test_dir.c_str()));

    // reboot.log should remain unchanged
    std::string reboot = test_dir + "/reboot.log";
    EXPECT_EQ(0, access(reboot.c_str(), F_OK));

    // normal.log should be renamed
    std::string normal = test_dir + "/normal.log";
    EXPECT_NE(0, access(normal.c_str(), F_OK));
}

TEST_F(FileOperationsTest, AddTimestampULN_SkipsABLReason) {
    g_folderCheck_result = true;
    CreateFile("ABLReason.txt");
    CreateFile("app.log");

    EXPECT_EQ(0, add_timestamp_to_files_uploadlogsnow(test_dir.c_str()));

    std::string abl = test_dir + "/ABLReason.txt";
    EXPECT_EQ(0, access(abl.c_str(), F_OK));
}

TEST_F(FileOperationsTest, AddTimestampULN_SkipsAlreadyTimestamped) {
    g_folderCheck_result = true;
    // File with existing AM/PM timestamp pattern
    CreateFile("01-01-25-10-30AM-app.log");

    EXPECT_EQ(0, add_timestamp_to_files_uploadlogsnow(test_dir.c_str()));

    // Should remain unchanged
    std::string ts_file = test_dir + "/01-01-25-10-30AM-app.log";
    EXPECT_EQ(0, access(ts_file.c_str(), F_OK));
}

// ==================== move_directory_contents TESTS ====================

TEST_F(FileOperationsTest, MoveDirectoryContents_NullSrc) {
    EXPECT_EQ(-1, move_directory_contents(nullptr, "/tmp/dest"));
}

TEST_F(FileOperationsTest, MoveDirectoryContents_NullDest) {
    EXPECT_EQ(-1, move_directory_contents("/tmp/src", nullptr));
}

TEST_F(FileOperationsTest, MoveDirectoryContents_SrcNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(-1, move_directory_contents("/nonexistent", "/tmp/dest"));
}

TEST_F(FileOperationsTest, MoveDirectoryContents_Success) {
    g_folderCheck_result = true;
    CreateFile("moveme.log");

    std::string dest = test_dir + "_dest";
    mkdir(dest.c_str(), 0755);

    EXPECT_EQ(0, move_directory_contents(test_dir.c_str(), dest.c_str()));

    // File should be in dest now
    std::string moved = dest + "/moveme.log";
    EXPECT_EQ(0, access(moved.c_str(), F_OK));

    // Cleanup dest
    std::string cmd = "rm -rf " + dest;
    system(cmd.c_str());
}

// ==================== clean_directory TESTS ====================

TEST_F(FileOperationsTest, CleanDirectory_NullPath) {
    EXPECT_EQ(-1, clean_directory(nullptr));
}

TEST_F(FileOperationsTest, CleanDirectory_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(-1, clean_directory("/nonexistent"));
}

TEST_F(FileOperationsTest, CleanDirectory_Success) {
    g_folderCheck_result = true;
    g_emptyFolder_result = true;
    CreateFile("junk.tmp");

    EXPECT_EQ(0, clean_directory(test_dir.c_str()));
}

TEST_F(FileOperationsTest, CleanDirectory_Failure) {
    g_folderCheck_result = true;
    g_emptyFolder_result = false;
    EXPECT_EQ(-1, clean_directory(test_dir.c_str()));
}

// ==================== clear_old_packet_captures TESTS ====================

TEST_F(FileOperationsTest, ClearPacketCaptures_NullPath) {
    EXPECT_EQ(-1, clear_old_packet_captures(nullptr));
}

TEST_F(FileOperationsTest, ClearPacketCaptures_DirNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(-1, clear_old_packet_captures("/nonexistent"));
}

TEST_F(FileOperationsTest, ClearPacketCaptures_RemovesPcapFiles) {
    g_folderCheck_result = true;
    g_filePresentCheck_result = true;
    g_removeFile_result = true;
    CreateFile("capture.pcap");
    CreateFile("other.pcap");
    CreateFile("keep.log");

    EXPECT_EQ(0, clear_old_packet_captures(test_dir.c_str()));

    // .pcap files should be removed, .log should remain
    std::string pcap1 = test_dir + "/capture.pcap";
    std::string pcap2 = test_dir + "/other.pcap";
    std::string log = test_dir + "/keep.log";
    EXPECT_NE(0, access(pcap1.c_str(), F_OK));
    EXPECT_NE(0, access(pcap2.c_str(), F_OK));
    EXPECT_EQ(0, access(log.c_str(), F_OK));
}

TEST_F(FileOperationsTest, ClearPacketCaptures_NoPcapFiles) {
    g_folderCheck_result = true;
    CreateFile("data.log");
    EXPECT_EQ(0, clear_old_packet_captures(test_dir.c_str()));
}

// ==================== remove_old_directories TESTS ====================

TEST_F(FileOperationsTest, RemoveOldDirs_NullBasePath) {
    EXPECT_EQ(-1, remove_old_directories(nullptr, "pattern", 3));
}

TEST_F(FileOperationsTest, RemoveOldDirs_NullPattern) {
    EXPECT_EQ(-1, remove_old_directories("/tmp", nullptr, 3));
}

TEST_F(FileOperationsTest, RemoveOldDirs_NegativeDays) {
    EXPECT_EQ(-1, remove_old_directories("/tmp", "pat", -1));
}

TEST_F(FileOperationsTest, RemoveOldDirs_BaseNotExists) {
    g_folderCheck_result = false;
    EXPECT_EQ(0, remove_old_directories("/nonexistent", "pat", 3));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
