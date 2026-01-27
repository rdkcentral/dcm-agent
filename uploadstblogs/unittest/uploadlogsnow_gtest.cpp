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
 *
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <cstdlib>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"
#include "uploadlogsnow.h"

// Mock only application-specific functions, not standard library functions
extern "C" {

// Mock functions for uploadlogsnow module dependencies
bool remove_directory(const char* path);
int add_timestamp_to_files_uploadlogsnow(const char* dir_path);
bool copy_file(const char* src, const char* dest);
bool create_directory(const char* path);
bool file_exists(const char* path);
int copy_files_to_dcm_path(const char* src_path, const char* dest_path);
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir);
void decide_paths(RuntimeContext* ctx, SessionState* session);
bool execute_upload_cycle(RuntimeContext* ctx, SessionState* session);

// Global test state variables
static bool g_copy_file_should_fail = false;
static bool g_create_directory_should_fail = false;
static bool g_file_exists_return_value = true;
static bool g_remove_directory_should_fail = false;
static bool g_add_timestamp_should_fail = false;
static bool g_create_archive_should_fail = false;
static bool g_execute_upload_cycle_return_value = true;
static int g_copy_files_return_count = 3;

// Mock implementations for uploadlogsnow module dependencies
bool copy_file(const char* src, const char* dest) {
    return g_copy_file_should_fail ? false : true;
}

bool create_directory(const char* path) {
    return g_create_directory_should_fail ? false : true;
}

bool file_exists(const char* path) {
    return g_file_exists_return_value ? true : false;
}

bool remove_directory(const char* path) {
    return g_remove_directory_should_fail ? false : true;
}

int add_timestamp_to_files_uploadlogsnow(const char* dir_path) {
    return g_add_timestamp_should_fail ? -1 : 0;
}

int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir) {
    if (g_create_archive_should_fail) return -1;
    
    // Simulate setting archive filename - ensure it's safe
    if (session) {
        strncpy(session->archive_file, "test_archive.tar.gz", sizeof(session->archive_file) - 1);
        session->archive_file[sizeof(session->archive_file) - 1] = '\0';
    }
    return 0;
}

void decide_paths(RuntimeContext* ctx, SessionState* session) {
    // Mock implementation - just set session state
    if (session) {
        session->strategy = STRAT_ONDEMAND;
        session->primary = PATH_DIRECT;
    }
}

bool execute_upload_cycle(RuntimeContext* ctx, SessionState* session) {
    if (!ctx || !session) return false;
    return g_execute_upload_cycle_return_value;
}

int copy_files_to_dcm_path(const char* src_path, const char* dest_path) {
    if (!src_path || !dest_path) return -1;
    if (g_copy_file_should_fail) return -1;
    return g_copy_files_return_count;
}

} // extern "C"

namespace {

class UploadLogsNowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all mock state
        g_copy_file_should_fail = false;
        g_create_directory_should_fail = false;
        g_file_exists_return_value = true;
        g_remove_directory_should_fail = false;
        g_add_timestamp_should_fail = false;
        g_create_archive_should_fail = false;
        g_execute_upload_cycle_return_value = true;
        g_copy_files_return_count = 3;
        
        // Create a temporary test directory
        test_log_dir = std::string("/tmp/uploadlogsnow_test_") + std::to_string(getpid());
        
        // Initialize test context with safe paths
        memset(&ctx, 0, sizeof(ctx));
        strncpy(ctx.log_path, test_log_dir.c_str(), sizeof(ctx.log_path) - 1);
        strcpy(ctx.dcm_log_path, "");
        ctx.uploadlogsnow_mode = true;
    }

    void TearDown() override {
        // Clean up test directory if it was created
        if (!test_log_dir.empty()) {
            std::string cleanup_cmd = "rm -rf " + test_log_dir;
            system(cleanup_cmd.c_str());
        }
    }

    RuntimeContext ctx;
    std::string test_log_dir;
};

// Test cases for execute_uploadlogsnow_workflow

TEST_F(UploadLogsNowTest, ExecuteWorkflow_NullContext) {
    // Test null context parameter
    int result = execute_uploadlogsnow_workflow(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_Success) {
    // Test successful workflow execution with all mocks returning success
    g_execute_upload_cycle_return_value = true;
    g_copy_files_return_count = 3; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should succeed
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_WithCustomDcmLogPath) {
    strcpy(ctx.dcm_log_path, "/custom/dcm/path");
    g_copy_files_return_count = 3; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should succeed with custom DCM log path
    // The function should use the provided DCM path or create a default one
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CreateDirectoryFails) {
    g_create_directory_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result); // Should fail due to directory creation failure
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CopyFilesFails) {
    g_copy_file_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result); // Should fail due to file copy failure
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_TimestampAdditionFails) {
    g_add_timestamp_should_fail = true;
    g_copy_files_return_count = 3; // Some files copied
    
    // Timestamp failure should not stop the workflow (it continues)
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should still succeed despite timestamp failure
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CreateArchiveFails) {
    g_create_archive_should_fail = true;
    g_copy_files_return_count = 3; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result); // Should fail due to archive creation failure
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_ArchiveFileNotFound) {
    g_file_exists_return_value = false;
    g_copy_files_return_count = 3; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result); // Should fail when archive file doesn't exist after creation
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_UploadFails) {
    g_execute_upload_cycle_return_value = false;
    g_copy_files_return_count = 3; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result); // Should fail when upload fails
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_NoFilesToUpload) {
    g_copy_files_return_count = 0; // No files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should succeed but with no files to process
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CleanupFails) {
    g_remove_directory_should_fail = true;
    g_copy_files_return_count = 3; // Some files copied
    
    // Cleanup failure should not affect the overall result if upload succeeded
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should still succeed despite cleanup failure
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_EmptyLogPath) {
    memset(ctx.log_path, 0, sizeof(ctx.log_path)); // Empty log path
    g_copy_files_return_count = 3; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should handle empty log path gracefully
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_LongPathHandling) {
    // Test workflow with very long DCM path to ensure proper path handling
    strcpy(ctx.dcm_log_path, "/very/long/path/that/might/cause/issues/in/archive/path/construction/and/should/be/handled/properly");
    g_copy_files_return_count = 2; // Some files copied
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should handle long paths correctly
}

// Integration-style tests that test actual workflow with mocked dependencies
TEST_F(UploadLogsNowTest, IntegrationTest_SuccessfulWorkflow) {
    // Configure all mocks for success
    g_copy_files_return_count = 5; // Multiple files copied
    g_execute_upload_cycle_return_value = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(0, result); // Should complete successfully
}

TEST_F(UploadLogsNowTest, IntegrationTest_CascadingFailures) {
    // Test various failure scenarios one by one
    
    // First test: directory creation fails (early failure)
    g_create_directory_should_fail = true;
    int result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result);
    
    // Reset and test copy failure
    SetUp(); // Reset all mocks
    g_copy_file_should_fail = true;
    result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result);
    
    // Reset and test archive creation failure
    SetUp(); // Reset all mocks
    g_create_archive_should_fail = true;
    g_copy_files_return_count = 3; // Some files copied
    result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result);
    
    // Reset and test upload failure
    SetUp(); // Reset all mocks
    g_execute_upload_cycle_return_value = false;
    g_copy_files_return_count = 3; // Some files copied
    result = execute_uploadlogsnow_workflow(&ctx);
    EXPECT_EQ(-1, result);
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
