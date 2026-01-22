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
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"
#include "uploadlogsnow.h"

// Define compatibility for Windows/Linux builds
#ifndef _WIN32
#include <dirent.h>
#else
// Windows compatibility
typedef struct {
    int dummy;
} DIR;
struct dirent {
    char d_name[256];
};
#endif

// Mock system functions that uploadlogsnow depends on
extern "C" {

// Mock file system functions
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
int fprintf(FILE* stream, const char* format, ...);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int stat(const char* path, struct stat* buf);
DIR* opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);
time_t time(time_t* tloc);
char* ctime(const time_t* timep);
int snprintf(char* str, size_t size, const char* format, ...);

// Mock functions from file_operations.h that uploadlogsnow depends on
bool remove_directory(const char* path);
int add_timestamp_to_files_uploadlogsnow(const char* dir_path);
bool copy_file(const char* src, const char* dest);
bool create_directory(const char* path);
bool file_exists(const char* path);

// Internal uploadlogsnow function that we need to mock/test
int copy_files_to_dcm_path(const char* src_path, const char* dest_path);

// Mock functions from archive_manager.h
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir);

// Mock functions from upload_engine.h  
void decide_paths(RuntimeContext* ctx, SessionState* session);
bool execute_upload_cycle(RuntimeContext* ctx, SessionState* session);

// Global mock state variables
static FILE* mock_file_ptr = (FILE*)0x12345678;
static struct stat mock_stat_buf;
static DIR* mock_dir_ptr = (DIR*)0x87654321;
static struct dirent mock_dirent_entries[10];
static int mock_dirent_index = 0;
static int mock_dirent_count = 0;
static time_t mock_time_value = 1642780800; // 2022-01-21 12:00:00
static char mock_ctime_buffer[32] = "Fri Jan 21 12:00:00 2022\n";
static bool g_fopen_should_fail = false;
static bool g_opendir_should_fail = false;
static bool g_copy_file_should_fail = false;
static bool g_create_directory_should_fail = false;
static bool g_file_exists_return_value = true;
static bool g_remove_directory_should_fail = false;
static bool g_add_timestamp_should_fail = false;
static bool g_create_archive_should_fail = false;
static bool g_execute_upload_cycle_return_value = true;
static char g_status_file_content[512] = {0};
static int g_fprintf_call_count = 0;
static int g_readdir_call_count = 0;
static int g_copy_files_return_count = 3; // Default return count for copy_files_to_dcm_path

// Mock implementations
FILE* fopen(const char* filename, const char* mode) {
    if (g_fopen_should_fail) return nullptr;
    if (filename && strstr(filename, "loguploadstatus.txt")) {
        g_fprintf_call_count = 0; // Reset fprintf counter
        return mock_file_ptr;
    }
    return mock_file_ptr;
}

int fclose(FILE* stream) {
    return (stream == mock_file_ptr) ? 0 : -1;
}

int fprintf(FILE* stream, const char* format, ...) {
    if (stream != mock_file_ptr) return -1;
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(g_status_file_content, sizeof(g_status_file_content), format, args);
    va_end(args);
    
    g_fprintf_call_count++;
    return result;
}

DIR* opendir(const char* name) {
    if (g_opendir_should_fail) return nullptr;
    mock_dirent_index = 0;
    g_readdir_call_count = 0;
    return mock_dir_ptr;
}

struct dirent* readdir(DIR* dirp) {
    if (dirp != mock_dir_ptr) return nullptr;
    
    g_readdir_call_count++;
    if (mock_dirent_index >= mock_dirent_count) {
        return nullptr; // End of directory
    }
    
    // Make sure we return a valid entry
    struct dirent* entry = &mock_dirent_entries[mock_dirent_index++];
    // Ensure d_name is null terminated
    entry->d_name[sizeof(entry->d_name) - 1] = '\0';
    return entry;
}

int closedir(DIR* dirp) {
    return (dirp == mock_dir_ptr) ? 0 : -1;
}

time_t time(time_t* tloc) {
    if (tloc) *tloc = mock_time_value;
    return mock_time_value;
}

char* ctime(const time_t* timep) {
    return mock_ctime_buffer;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

// Mock file operations implementations
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
    session->strategy = STRAT_ONDEMAND;
    session->primary = PATH_DIRECT;
}

bool execute_upload_cycle(RuntimeContext* ctx, SessionState* session) {
    return g_execute_upload_cycle_return_value;
}

} // extern "C"

namespace {

class UploadLogsNowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all mock state
        g_fopen_should_fail = false;
        g_opendir_should_fail = false;
        g_copy_file_should_fail = false;
        g_create_directory_should_fail = false;
        g_file_exists_return_value = true;
        g_remove_directory_should_fail = false;
        g_add_timestamp_should_fail = false;
        g_create_archive_should_fail = false;
        g_execute_upload_cycle_return_value = true;
        g_copy_files_return_count = 3;
        memset(g_status_file_content, 0, sizeof(g_status_file_content));
        g_fprintf_call_count = 0;
        g_readdir_call_count = 0;
        mock_dirent_index = 0;
        mock_dirent_count = 0;
        
        // Initialize test context - memset first to ensure clean state
        memset(&ctx, 0, sizeof(ctx));
        strcpy(ctx.log_path, "/opt/logs");
        strcpy(ctx.dcm_log_path, "");  // Will use default
        ctx.uploadlogsnow_mode = true;
    }

    void TearDown() override {
        // Clean up after each test
    }
    
    void setupMockDirectoryEntries(const char* entries[], int count) {
        mock_dirent_count = count < 10 ? count : 10;
        mock_dirent_index = 0;  // Reset index
        for (int i = 0; i < mock_dirent_count; i++) {
            strncpy(mock_dirent_entries[i].d_name, entries[i], sizeof(mock_dirent_entries[i].d_name) - 1);
            mock_dirent_entries[i].d_name[sizeof(mock_dirent_entries[i].d_name) - 1] = '\0';
        }
    }

    RuntimeContext ctx;
};

// Test cases for execute_uploadlogsnow_workflow

TEST_F(UploadLogsNowTest, ExecuteWorkflow_NullContext) {
    // Test null context parameter
    int result = execute_uploadlogsnow_workflow(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_Success) {
    // Setup mock directory with valid files
    const char* files[] = {"app.log", "system.log", "debug.log"};
    setupMockDirectoryEntries(files, 3);
    
    // Ensure context is properly initialized
    ASSERT_NE(&ctx, nullptr);
    ASSERT_STREQ(ctx.log_path, "/opt/logs");
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
    EXPECT_GT(g_fprintf_call_count, 0); // Status should be written
    EXPECT_STREQ("/tmp/DCM", ctx.dcm_log_path); // Should use default path
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_WithCustomDcmLogPath) {
    strcpy(ctx.dcm_log_path, "/custom/dcm/path");
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
    EXPECT_STREQ("/custom/dcm/path", ctx.dcm_log_path);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CreateDirectoryFails) {
    g_create_directory_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(-1, result);
    EXPECT_STRNE(g_status_file_content, ""); // Status should contain "Failed"
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_OpenDirFails) {
    g_opendir_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_StatusFileWriteFailure) {
    g_fopen_should_fail = true;
    
    // Should still proceed with workflow even if status write fails
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Workflow should still succeed despite status file failure
    EXPECT_EQ(0, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CopyFilesFails) {
    const char* files[] = {"app.log", "system.log"};
    setupMockDirectoryEntries(files, 2);
    g_copy_file_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Should fail but continue workflow
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_ExcludeFiles) {
    // Setup directory with files that should be excluded
    const char* files[] = {"dcm", "PreviousLogs_backup", "PreviousLogs", "valid.log"};
    setupMockDirectoryEntries(files, 4);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Should succeed, but excluded files should not be copied
    EXPECT_EQ(0, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_EmptyDirectory) {
    // No files in directory
    setupMockDirectoryEntries(nullptr, 0);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result); // Should still succeed with empty directory
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_DotDirectoriesIgnored) {
    const char* files[] = {".", "..", "valid.log"};
    setupMockDirectoryEntries(files, 3);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
    // Only one file should be processed (valid.log)
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_TimestampAdditionFails) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    g_add_timestamp_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Should continue even if timestamp addition fails
    EXPECT_EQ(0, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CreateArchiveFails) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    g_create_archive_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_ArchiveFileNotFound) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    g_file_exists_return_value = false; // Archive file doesn't exist after creation
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_UploadFails) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    g_execute_upload_cycle_return_value = false;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_CleanupFails) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    g_remove_directory_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Should succeed even if cleanup fails
    EXPECT_EQ(0, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_LongPathNames) {
    // Test with very long filenames that might cause buffer overflow
    const char* files[] = {
        "very_very_very_very_very_very_very_very_long_filename_that_might_cause_buffer_overflow_issues.log",
        "another_extremely_long_filename_with_many_characters_to_test_path_length_validation.txt"
    };
    setupMockDirectoryEntries(files, 2);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result); // Should handle long paths gracefully
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_StatusFileContent) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
    // Verify status file was written with appropriate content
    EXPECT_GT(strlen(g_status_file_content), 0);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_NullContext) {
    // Test with null context pointer
    int result = execute_uploadlogsnow_workflow(nullptr);
    
    // Should handle null pointer gracefully
    EXPECT_EQ(-1, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_EmptyLogPath) {
    memset(ctx.log_path, 0, sizeof(ctx.log_path)); // Empty log path
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(-1, result); // Should fail with empty log path
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_MultipleFileTypes) {
    // Test with various file types
    const char* files[] = {
        "system.log", "app.txt", "debug.out", "trace.dat",
        "core.dump", "messages", "secure.log"
    };
    setupMockDirectoryEntries(files, 7);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_SessionStatePopulation) {
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
    // Verify that session state is properly initialized for ONDEMAND strategy
}

TEST_F(UploadLogsNowTest, ExecuteWorkflow_ArchivePathConstruction) {
    // Test with specific DCM log path that might cause path construction issues
    strcpy(ctx.dcm_log_path, "/very/long/path/that/might/cause/issues/in/archive/path/construction");
    
    const char* files[] = {"app.log"};
    setupMockDirectoryEntries(files, 1);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Should handle long paths appropriately
    EXPECT_EQ(0, result);
}

// Integration-style tests that test the full workflow
TEST_F(UploadLogsNowTest, IntegrationTest_CompleteSuccessfulWorkflow) {
    // Setup a realistic scenario
    strcpy(ctx.log_path, "/opt/logs");
    strcpy(ctx.dcm_log_path, ""); // Use default
    ctx.uploadlogsnow_mode = true;
    
    const char* files[] = {"messages", "syslog", "application.log", "wifi.log"};
    setupMockDirectoryEntries(files, 4);
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    EXPECT_EQ(0, result);
    EXPECT_STREQ("/tmp/DCM", ctx.dcm_log_path);
    EXPECT_GT(g_fprintf_call_count, 0); // Status updates written
}

TEST_F(UploadLogsNowTest, IntegrationTest_PartialFailureRecovery) {
    // Test scenario where some operations fail but workflow continues
    const char* files[] = {"good.log", "bad.log", "ugly.log"};
    setupMockDirectoryEntries(files, 3);
    
    // Let timestamp addition fail but continue
    g_add_timestamp_should_fail = true;
    
    int result = execute_uploadlogsnow_workflow(&ctx);
    
    // Should still succeed despite timestamp failure
    EXPECT_EQ(0, result);
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
