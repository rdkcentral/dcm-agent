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
#include <iostream>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

// Include system headers for types before extern "C"
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>

// Mock external dependencies
extern "C" {
// Mock system functions that we need to control for testing
DIR* opendir(const char *name);
int closedir(DIR *dirp);
struct dirent* readdir(DIR *dirp);
int stat(const char *pathname, struct stat *statbuf);

// Mock external module functions
bool dir_exists(const char* path);
bool copy_file(const char* src, const char* dest);
}

#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 0
#endif

// Mock state
static bool mock_dir_exists_result = true;
static bool mock_copy_file_result = true;
static int mock_opendir_fail = false;
static int mock_readdir_call_count = 0;
static int mock_file_count = 3;
static struct dirent mock_entries[10];
static int mock_entry_index = 0;

// Mock call tracking variables
static int mock_dir_exists_calls = 0;
static int mock_copy_file_calls = 0;
static int mock_opendir_calls = 0;
static int mock_closedir_calls = 0;
static int mock_readdir_calls = 0;

// Mock implementations
bool dir_exists(const char* path) {
    mock_dir_exists_calls++;
    return mock_dir_exists_result;
}

bool copy_file(const char* src, const char* dest) {
    mock_copy_file_calls++;
    return mock_copy_file_result;
}

DIR* opendir(const char *name) {
    mock_opendir_calls++;
    if (mock_opendir_fail) {
        return nullptr;
    }
    return (DIR*)0x12345678; // Mock pointer
}

int closedir(DIR *dirp) {
    mock_closedir_calls++;
    return 0;
}

struct dirent* readdir(DIR *dirp) {
    mock_readdir_calls++;
    if (mock_entry_index >= mock_file_count) {
        return nullptr; // End of directory
    }
    return &mock_entries[mock_entry_index++];
}

int stat(const char *pathname, struct stat *statbuf) {
    if (!statbuf) return -1;
    // Mock stat - just fill with some dummy data
    statbuf->st_mode = S_IFREG; // Regular file
    statbuf->st_mtime = 1234567890; // Mock timestamp
    return 0;
}

// Include the actual log collector implementation
#include "log_collector.h"
#include "../src/log_collector.c"

using namespace testing;
using namespace std;

class LogCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        mock_dir_exists_result = true;
        mock_copy_file_result = true;
        mock_opendir_fail = false;
        mock_readdir_call_count = 0;
        mock_file_count = 3;
        mock_entry_index = 0;

        // Reset call tracking
        mock_dir_exists_calls = 0;
        mock_copy_file_calls = 0;
        mock_opendir_calls = 0;
        mock_closedir_calls = 0;
        mock_readdir_calls = 0;

        // Set up default test context
        strcpy(test_ctx.paths.log_path, "/opt/logs");
        strcpy(test_ctx.paths.prev_log_path, "/opt/logs/PreviousLogs");
        strcpy(test_ctx.paths.dri_log_path, "/opt/logs/dri");
        strcpy(test_ctx.device.device_type, "gateway");
        test_ctx.settings.include_pcap = false;
        test_ctx.settings.include_dri = false;

        // Set up default test session
        test_session.strategy = STRAT_DCM;
        test_session.direct_attempts = 1;
        test_session.codebig_attempts = 0;
        test_session.used_fallback = false;
        test_session.success = false;

        // Setup mock directory entries
        setupMockEntries();
    }

    void setupMockEntries() {
        // Entry 0: Regular log file
        mock_entries[0].d_type = DT_REG;
        strcpy(mock_entries[0].d_name, "messages.log");

        // Entry 1: Text file
        mock_entries[1].d_type = DT_REG;
        strcpy(mock_entries[1].d_name, "system.txt");

        // Entry 2: Non-log file (should be skipped)
        mock_entries[2].d_type = DT_REG;
        strcpy(mock_entries[2].d_name, "config.conf");

        // Entry 3: Directory (should be skipped)
        mock_entries[3].d_type = DT_DIR;
        strcpy(mock_entries[3].d_name, "subdir");

        // Entry 4: Rotated log file
        mock_entries[4].d_type = DT_REG;
        strcpy(mock_entries[4].d_name, "debug.log.1");
    }

    void TearDown() override {}

    RuntimeContext test_ctx;
    SessionState test_session;
};

// Test should_collect_file function
TEST_F(LogCollectorTest, ShouldCollectFile_LogFile) {
    EXPECT_TRUE(should_collect_file("messages.log"));
    EXPECT_TRUE(should_collect_file("system.log.1"));
    EXPECT_TRUE(should_collect_file("debug.log.0"));
}

TEST_F(LogCollectorTest, ShouldCollectFile_TextFile) {
    EXPECT_TRUE(should_collect_file("output.txt"));
    EXPECT_TRUE(should_collect_file("info.txt.2"));
    EXPECT_TRUE(should_collect_file("data.txt.old"));
}

TEST_F(LogCollectorTest, ShouldCollectFile_NonLogFile) {
    EXPECT_FALSE(should_collect_file("config.conf"));
    EXPECT_FALSE(should_collect_file("binary.bin"));
    EXPECT_FALSE(should_collect_file("image.png"));
}

TEST_F(LogCollectorTest, ShouldCollectFile_SpecialCases) {
    EXPECT_FALSE(should_collect_file(nullptr));
    EXPECT_FALSE(should_collect_file(""));
    EXPECT_FALSE(should_collect_file("."));
    EXPECT_FALSE(should_collect_file(".."));
}

// Test collect_previous_logs function
TEST_F(LogCollectorTest, CollectPreviousLogs_Success) {
    mock_file_count = 2; // Only log and txt files

    int result = collect_previous_logs("/opt/logs/PreviousLogs", "/tmp/dest");

    EXPECT_EQ(result, 2); // Should collect 2 files
    EXPECT_EQ(mock_dir_exists_calls, 2); // Called twice: once in collect_previous_logs, once in collect_files_from_dir
    EXPECT_EQ(mock_opendir_calls, 1);
    EXPECT_EQ(mock_closedir_calls, 1);
    EXPECT_EQ(mock_copy_file_calls, 2);
}

TEST_F(LogCollectorTest, CollectPreviousLogs_NullParameters) {
    int result1 = collect_previous_logs(nullptr, "/tmp/dest");
    int result2 = collect_previous_logs("/opt/logs/PreviousLogs", nullptr);

    EXPECT_EQ(result1, -1);
    EXPECT_EQ(result2, -1);
}

TEST_F(LogCollectorTest, CollectPreviousLogs_DirectoryNotExists) {
    mock_dir_exists_result = false;

    int result = collect_previous_logs("/nonexistent", "/tmp/dest");

    EXPECT_EQ(result, 0); // Should return 0 when directory doesn't exist
    EXPECT_EQ(mock_dir_exists_calls, 1);
    EXPECT_EQ(mock_opendir_calls, 0); // Should not try to open
}

TEST_F(LogCollectorTest, CollectPreviousLogs_OpendirFails) {
    mock_opendir_fail = true;

    int result = collect_previous_logs("/opt/logs/PreviousLogs", "/tmp/dest");

    EXPECT_EQ(result, -1);
    EXPECT_EQ(mock_opendir_calls, 1);
    EXPECT_EQ(mock_closedir_calls, 0);
}

TEST_F(LogCollectorTest, CollectPreviousLogs_CopyFailure) {
    mock_copy_file_result = false;
    mock_file_count = 2;

    int result = collect_previous_logs("/opt/logs/PreviousLogs", "/tmp/dest");

    EXPECT_EQ(result, 0); // No files successfully copied
    EXPECT_EQ(mock_copy_file_calls, 2); // Should still try to copy both files
}

// Test collect_pcap_logs function
TEST_F(LogCollectorTest, CollectPcapLogs_Enabled) {
    test_ctx.settings.include_pcap = true;
    strcpy(test_ctx.paths.log_path, "/opt/logs");

    // Setup PCAP files
    strcpy(mock_entries[0].d_name, "capture.pcap");
    strcpy(mock_entries[1].d_name, "network.pcap.gz");
    mock_file_count = 2;

    int result = collect_pcap_logs(&test_ctx, "/tmp/dest");

    EXPECT_GE(result, 0); // Should not fail
    EXPECT_EQ(mock_opendir_calls, 1);
}

TEST_F(LogCollectorTest, CollectPcapLogs_Disabled) {
    test_ctx.settings.include_pcap = false;

    int result = collect_pcap_logs(&test_ctx, "/tmp/dest");

    EXPECT_EQ(result, 0); // Should return 0 when disabled
    EXPECT_EQ(mock_opendir_calls, 0); // Should not open directory
}

TEST_F(LogCollectorTest, CollectPcapLogs_NullContext) {
    int result = collect_pcap_logs(nullptr, "/tmp/dest");

    EXPECT_EQ(result, -1); // Should handle null context
}

// Test collect_dri_logs function
TEST_F(LogCollectorTest, CollectDriLogs_Enabled) {
    test_ctx.settings.include_dri = true;
    strcpy(test_ctx.paths.dri_log_path, "/opt/logs/dri");

    // Setup DRI files
    strcpy(mock_entries[0].d_name, "dri_data.log");
    strcpy(mock_entries[1].d_name, "dri_debug.txt");
    mock_file_count = 2;

    int result = collect_dri_logs(&test_ctx, "/tmp/dest");

    EXPECT_GE(result, 0); // Should not fail
    EXPECT_EQ(mock_opendir_calls, 1);
}

TEST_F(LogCollectorTest, CollectDriLogs_Disabled) {
    test_ctx.settings.include_dri = false;

    int result = collect_dri_logs(&test_ctx, "/tmp/dest");

    EXPECT_EQ(result, 0); // Should return 0 when disabled
    EXPECT_EQ(mock_opendir_calls, 0); // Should not open directory
}

TEST_F(LogCollectorTest, CollectDriLogs_NullContext) {
    int result = collect_dri_logs(nullptr, "/tmp/dest");

    EXPECT_EQ(result, -1); // Should handle null context
}

// Test main collect_logs function
TEST_F(LogCollectorTest, CollectLogs_BasicCollection) {
    mock_file_count = 2; // Log and txt files

    int result = collect_logs(&test_ctx, &test_session, "/tmp/dest");

    EXPECT_GE(result, 0); // Should not fail
    EXPECT_GE(mock_opendir_calls, 1); // Should open at least main log directory
}

TEST_F(LogCollectorTest, CollectLogs_WithPreviousLogs) {
    mock_file_count = 2;
    strcpy(test_ctx.paths.prev_log_path, "/opt/logs/PreviousLogs");

    int result = collect_logs(&test_ctx, &test_session, "/tmp/dest");

    EXPECT_GE(result, 0);
    EXPECT_EQ(mock_opendir_calls, 1); // Only opens main log directory, not previous logs
}

TEST_F(LogCollectorTest, CollectLogs_WithPcapAndDri) {
    test_ctx.settings.include_pcap = true;
    test_ctx.settings.include_dri = true;
    mock_file_count = 2;

    int result = collect_logs(&test_ctx, &test_session, "/tmp/dest");

    EXPECT_GE(result, 0);
    EXPECT_EQ(mock_opendir_calls, 1); // Only opens main log directory
}

TEST_F(LogCollectorTest, CollectLogs_NullParameters) {
    int result1 = collect_logs(nullptr, &test_session, "/tmp/dest");
    int result2 = collect_logs(&test_ctx, nullptr, "/tmp/dest");
    int result3 = collect_logs(&test_ctx, &test_session, nullptr);

    EXPECT_EQ(result1, -1);
    EXPECT_EQ(result2, -1);
    EXPECT_EQ(result3, -1);
}

TEST_F(LogCollectorTest, CollectLogs_EmptyDirectory) {
    mock_file_count = 0; // No files in directory

    int result = collect_logs(&test_ctx, &test_session, "/tmp/dest");

    EXPECT_EQ(result, 0); // Should return 0 for empty directory
    EXPECT_EQ(mock_copy_file_calls, 0); // No files to copy
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    cout << "Starting Log Collector Unit Tests" << endl;
    return RUN_ALL_TESTS();
}
