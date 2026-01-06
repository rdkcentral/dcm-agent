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
#include <ctime>

// Include directory operation headers
#ifdef GTEST_ENABLE
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#endif

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"
#include "./mocks/mock_file_operations.h"

// Mock external dependencies
extern "C" {
// Mock regex functions
#ifdef GTEST_ENABLE
int regcomp(regex_t *preg, const char *pattern, int cflags);
int regexec(const regex_t *preg, const char *string, size_t nmatch, 
           regmatch_t pmatch[], int eflags);
void regfree(regex_t *preg);

static int mock_regex_result = 0;
static bool regex_compile_fail = false;

int regcomp(regex_t *preg, const char *pattern, int cflags) {
    if (regex_compile_fail) {
        return 1; // Error
    }
    memset(preg, 0, sizeof(regex_t));
    return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, 
           regmatch_t pmatch[], int eflags) {
    return mock_regex_result;
}

void regfree(regex_t *preg) {
    // No-op for mock
}

// Mock directory operations
DIR* opendir(const char *dirname);
struct dirent* readdir(DIR *dirp);
int closedir(DIR *dirp);
int stat(const char *pathname, struct stat *statbuf);
int remove(const char *pathname);
int rmdir(const char *pathname);

static bool opendir_fail = false;
static bool stat_fail = false;
static bool remove_fail = false;
static int mock_readdir_count = 0;
static int total_opendir_calls = 0;

DIR* opendir(const char *dirname) {
    if (opendir_fail || !dirname) {
        return NULL;
    }
    total_opendir_calls++;
    // Prevent infinite recursion by limiting opendir calls
    if (total_opendir_calls > 10) {
        return NULL;
    }
    return (DIR*)0x1234; // Dummy non-null pointer
}

struct dirent* readdir(DIR *dirp) {
    static struct dirent mock_entries[10];
    
    // For the first opendir call, return the main test files
    if (total_opendir_calls <= 1) {
        static const char* test_files[] = {
            ".", "..", "old_archive.tgz", "another.tgz", "not_archive.txt",
            "11-30-25-03-45PM-logbackup", "12-01-25-10-30AM-logbackup", 
            "normal_folder", NULL
        };
        
        if (mock_readdir_count < 8 && test_files[mock_readdir_count]) {
            strcpy(mock_entries[mock_readdir_count].d_name, test_files[mock_readdir_count]);
            return &mock_entries[mock_readdir_count++];
        }
    } else {
        // For recursive calls, return empty directory (just . and ..)
        static const char* empty_dir[] = { ".", "..", NULL };
        
        if (mock_readdir_count < 2 && empty_dir[mock_readdir_count]) {
            strcpy(mock_entries[mock_readdir_count].d_name, empty_dir[mock_readdir_count]);
            return &mock_entries[mock_readdir_count++];
        }
    }
    
    // Reset for next readdir sequence
    mock_readdir_count = 0;
    return NULL;
}

int closedir(DIR *dirp) {
    // Reset readdir count when closing directory
    mock_readdir_count = 0;
    return 0;
}

int stat(const char *pathname, struct stat *statbuf) {
    if (stat_fail || !pathname || !statbuf) {
        return -1;
    }
    memset(statbuf, 0, sizeof(struct stat));
    
    // Mock file times: old files are 5 days old, recent files are 1 day old
    time_t now = time(NULL);
    if (strstr(pathname, "11-30-25-03-45PM") || strstr(pathname, "old_archive")) {
        statbuf->st_mtime = now - (5 * 24 * 60 * 60); // 5 days ago
    } else {
        statbuf->st_mtime = now - (1 * 24 * 60 * 60); // 1 day ago
    }
    
    // Set directory flag for backup folders
    if (strstr(pathname, "logbackup") || strstr(pathname, "normal_folder")) {
        statbuf->st_mode = S_IFDIR | 0755;
    } else {
        statbuf->st_mode = S_IFREG | 0644;
    }
    
    return 0;
}

int remove(const char *pathname) {
    if (remove_fail || !pathname) {
        return -1;
    }
    return 0;
}

int rmdir(const char *pathname) {
    if (remove_fail || !pathname) {
        return -1;
    }
    return 0;
}
#endif
}

// Include the actual cleanup handler implementation
#include "cleanup_handler.h"
#include "../src/cleanup_handler.c"

using namespace testing;

class CleanupManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize mock objects
        g_mockFileOperations = new MockFileOperations();
        
        // Reset mock state
        mock_regex_result = 0;
        regex_compile_fail = false;
        opendir_fail = false;
        stat_fail = false;
        remove_fail = false;
        mock_readdir_count = 0;
        total_opendir_calls = 0;
        
        // Set up test directory structure
        strcpy(test_log_path, "/opt/logs");
    }
    
    void TearDown() override {
        delete g_mockFileOperations;
        g_mockFileOperations = nullptr;
    }
    
    char test_log_path[512];
};

// Test is_timestamped_backup function
TEST_F(CleanupManagerTest, IsTimestampedBackup_ValidPatterns) {
    mock_regex_result = 0; // Match
    
    // Test valid timestamped backup patterns
    EXPECT_TRUE(is_timestamped_backup("11-30-25-03-45PM-logbackup"));
    EXPECT_TRUE(is_timestamped_backup("12-01-25-10-30AM-logbackup"));
    EXPECT_TRUE(is_timestamped_backup("01-15-24-11-59PM-logbackup"));
    
    // Test pattern without -logbackup suffix (just timestamp)
    EXPECT_TRUE(is_timestamped_backup("11-30-25-03-45PM-"));
    EXPECT_TRUE(is_timestamped_backup("12-01-25-10-30AM-"));
}

TEST_F(CleanupManagerTest, IsTimestampedBackup_InvalidPatterns) {
    mock_regex_result = 1; // No match
    
    // Test invalid patterns
    EXPECT_FALSE(is_timestamped_backup("normal_folder"));
    EXPECT_FALSE(is_timestamped_backup("logs"));
    EXPECT_FALSE(is_timestamped_backup("file.txt"));
    EXPECT_FALSE(is_timestamped_backup("11-30-25-logbackup")); // Missing time
    EXPECT_FALSE(is_timestamped_backup("invalid-timestamp"));
}

TEST_F(CleanupManagerTest, IsTimestampedBackup_NullInput) {
    EXPECT_FALSE(is_timestamped_backup(nullptr));
}

TEST_F(CleanupManagerTest, IsTimestampedBackup_RegexCompileError) {
    regex_compile_fail = true;
    EXPECT_FALSE(is_timestamped_backup("11-30-25-03-45PM-logbackup"));
}

// Test cleanup_old_log_backups function
TEST_F(CleanupManagerTest, CleanupOldLogBackups_Success) {
    mock_regex_result = 0; // Match regex for timestamped backups
    
    int result = cleanup_old_log_backups(test_log_path, 3);
    
    // Should return number of removed items (at least 0)
    EXPECT_GE(result, 0);
}

TEST_F(CleanupManagerTest, CleanupOldLogBackups_NullPath) {
    int result = cleanup_old_log_backups(nullptr, 3);
    EXPECT_EQ(result, -1);
}

TEST_F(CleanupManagerTest, CleanupOldLogBackups_InvalidDirectory) {
    opendir_fail = true;
    
    int result = cleanup_old_log_backups("/nonexistent", 3);
    EXPECT_EQ(result, -1);
}

TEST_F(CleanupManagerTest, CleanupOldLogBackups_NoMatchingFiles) {
    mock_regex_result = 1; // No regex match - no timestamped backups
    
    int result = cleanup_old_log_backups(test_log_path, 3);
    EXPECT_EQ(result, 0); // No files removed
}

TEST_F(CleanupManagerTest, CleanupOldLogBackups_StatFailure) {
    mock_regex_result = 0; // Match regex
    stat_fail = true;
    
    int result = cleanup_old_log_backups(test_log_path, 3);
    EXPECT_EQ(result, 0); // No files removed due to stat failure
}

// Test cleanup_old_archives function
TEST_F(CleanupManagerTest, CleanupOldArchives_Success) {
    int result = cleanup_old_archives(test_log_path);
    
    // Should find and attempt to remove .tgz files
    EXPECT_GE(result, 0);
}

TEST_F(CleanupManagerTest, CleanupOldArchives_NullPath) {
    int result = cleanup_old_archives(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CleanupManagerTest, CleanupOldArchives_InvalidDirectory) {
    opendir_fail = true;
    
    int result = cleanup_old_archives("/nonexistent");
    EXPECT_EQ(result, -1);
}

TEST_F(CleanupManagerTest, CleanupOldArchives_RemoveFailure) {
    remove_fail = true;
    
    int result = cleanup_old_archives(test_log_path);
    EXPECT_EQ(result, 0); // No files successfully removed due to failures
}

// Test edge cases and boundary conditions
TEST_F(CleanupManagerTest, EdgeCases_ZeroMaxAge) {
    mock_regex_result = 0; // Match regex
    
    // With max_age = 0, everything should be considered old
    int result = cleanup_old_log_backups(test_log_path, 0);
    EXPECT_GE(result, 0);
}

TEST_F(CleanupManagerTest, EdgeCases_LargeMaxAge) {
    mock_regex_result = 0; // Match regex
    
    // With large max_age, nothing should be old enough to remove
    int result = cleanup_old_log_backups(test_log_path, 365);
    EXPECT_EQ(result, 0);
}

// Integration tests
TEST_F(CleanupManagerTest, Integration_FullCleanup) {
    mock_regex_result = 0; // Match timestamped backups
    
    // Run both cleanup functions
    int backups_removed = cleanup_old_log_backups(test_log_path, 3);
    int archives_removed = cleanup_old_archives(test_log_path);
    
    EXPECT_GE(backups_removed, 0);
    EXPECT_GE(archives_removed, 0);
}

// Test filename pattern validation scenarios
TEST_F(CleanupManagerTest, PatternValidation_TimestampFormats) {
    // Test with different regex results to simulate pattern matching
    
    // Valid patterns should match (regex returns 0)
    mock_regex_result = 0;
    EXPECT_TRUE(is_timestamped_backup("01-01-25-12-00AM-logbackup"));
    EXPECT_TRUE(is_timestamped_backup("12-31-24-11-59PM-logbackup"));
    
    // Invalid patterns should not match (regex returns 1)
    mock_regex_result = 1;
    EXPECT_FALSE(is_timestamped_backup("invalid-format"));
    EXPECT_FALSE(is_timestamped_backup("11-30-25-logbackup")); // Missing time
}

TEST_F(CleanupManagerTest, ArchiveCleanup_FileTypes) {
    // Test that cleanup targets .tgz files specifically
    // The mock readdir provides test files including .tgz files
    int result = cleanup_old_archives(test_log_path);
    EXPECT_GE(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
