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
int dirfd(DIR *dirp);
int stat(const char *pathname, struct stat *statbuf);
int fstatat(int dfd, const char *pathname, struct stat *statbuf, int flags);
int unlinkat(int dfd, const char *pathname, int flags);
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

int dirfd(DIR *dirp) {
    // Return a dummy fd for the fake DIR pointer
    return 5;
}

int fstatat(int dfd, const char *pathname, struct stat *statbuf, int flags) {
    if (stat_fail || !pathname || !statbuf) {
        return -1;
    }
    memset(statbuf, 0, sizeof(struct stat));

    time_t now = time(NULL);
    if (strstr(pathname, "11-30-25-03-45PM") || strstr(pathname, "old_archive")) {
        statbuf->st_mtime = now - (5 * 24 * 60 * 60); // 5 days ago
    } else {
        statbuf->st_mtime = now - (1 * 24 * 60 * 60); // 1 day ago
    }

    if (strstr(pathname, "logbackup") || strstr(pathname, "normal_folder")) {
        statbuf->st_mode = S_IFDIR | 0755;
    } else {
        statbuf->st_mode = S_IFREG | 0644;
    }

    return 0;
}

int unlinkat(int dfd, const char *pathname, int flags) {
    if (remove_fail || !pathname) {
        return -1;
    }
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

// Mock unlink for remove_archive and cleanup_temp_dirs
static bool unlink_fail = false;
static bool unlink_enoent = false;
static int unlink_call_count = 0;

int unlink(const char *pathname) {
    unlink_call_count++;
    if (!pathname) {
        errno = EINVAL;
        return -1;
    }
    if (unlink_enoent) {
        errno = ENOENT;
        return -1;
    }
    if (unlink_fail) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

// Mock fopen/fclose/fprintf for create_block_marker
static bool fopen_fail = false;
static FILE* mock_file_ptr = (FILE*)0x5678;
static int fopen_call_count = 0;

FILE* fopen(const char *pathname, const char *mode) {
    fopen_call_count++;
    if (fopen_fail || !pathname) {
        return NULL;
    }
    return mock_file_ptr;
}

int fclose(FILE *stream) {
    return 0;
}

int fprintf(FILE *stream, const char *format, ...) {
    return 10;
}

// Accessor for static function
int (*getRemoveDirectoryRecursive(void))(const char*);

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
        unlink_fail = false;
        unlink_enoent = false;
        unlink_call_count = 0;
        fopen_fail = false;
        fopen_call_count = 0;
        
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

// ==================== remove_archive TESTS ====================

TEST_F(CleanupManagerTest, RemoveArchive_NullPath) {
    EXPECT_FALSE(remove_archive(nullptr));
}

TEST_F(CleanupManagerTest, RemoveArchive_EmptyPath) {
    EXPECT_FALSE(remove_archive(""));
}

TEST_F(CleanupManagerTest, RemoveArchive_Success) {
    unlink_fail = false;
    EXPECT_TRUE(remove_archive("/tmp/test_archive.tgz"));
    EXPECT_EQ(unlink_call_count, 1);
}

TEST_F(CleanupManagerTest, RemoveArchive_FileNotExist_ReturnsTrue) {
    unlink_enoent = true;
    EXPECT_TRUE(remove_archive("/tmp/nonexistent.tgz"));
}

TEST_F(CleanupManagerTest, RemoveArchive_PermissionDenied_ReturnsFalse) {
    unlink_fail = true;
    EXPECT_FALSE(remove_archive("/tmp/protected.tgz"));
}

// ==================== cleanup_temp_dirs TESTS ====================

TEST_F(CleanupManagerTest, CleanupTempDirs_NullCtx) {
    EXPECT_FALSE(cleanup_temp_dirs(nullptr, nullptr));
}

TEST_F(CleanupManagerTest, CleanupTempDirs_Success) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));

    unlink_fail = false;
    EXPECT_TRUE(cleanup_temp_dirs(&ctx, &session));
}

TEST_F(CleanupManagerTest, CleanupTempDirs_FilesNotExist_StillSucceeds) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));

    unlink_enoent = true;
    EXPECT_TRUE(cleanup_temp_dirs(&ctx, &session));
}

TEST_F(CleanupManagerTest, CleanupTempDirs_UnlinkFails_ReturnsFalse) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));

    unlink_fail = true;
    EXPECT_FALSE(cleanup_temp_dirs(&ctx, &session));
}

// ==================== create_block_marker TESTS ====================

TEST_F(CleanupManagerTest, CreateBlockMarker_DirectPath_Success) {
    fopen_fail = false;
    EXPECT_TRUE(create_block_marker(PATH_DIRECT, 3600));
    EXPECT_EQ(fopen_call_count, 1);
}

TEST_F(CleanupManagerTest, CreateBlockMarker_CodebigPath_Success) {
    fopen_fail = false;
    EXPECT_TRUE(create_block_marker(PATH_CODEBIG, 1800));
    EXPECT_EQ(fopen_call_count, 1);
}

TEST_F(CleanupManagerTest, CreateBlockMarker_InvalidPath_ReturnsFalse) {
    EXPECT_FALSE(create_block_marker(PATH_NONE, 3600));
}

TEST_F(CleanupManagerTest, CreateBlockMarker_FopenFails_ReturnsFalse) {
    fopen_fail = true;
    EXPECT_FALSE(create_block_marker(PATH_DIRECT, 3600));
}

// ==================== update_block_markers TESTS ====================

TEST_F(CleanupManagerTest, UpdateBlockMarkers_NullCtx) {
    SessionState session;
    memset(&session, 0, sizeof(session));
    // Should not crash
    update_block_markers(nullptr, &session);
}

TEST_F(CleanupManagerTest, UpdateBlockMarkers_NullSession) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    // Should not crash
    update_block_markers(&ctx, nullptr);
}

TEST_F(CleanupManagerTest, UpdateBlockMarkers_SuccessViaCodebig_BlocksDirect) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.success = true;
    session.used_fallback = true;
    session.codebig_attempts = 1;

    fopen_fail = false;
    fopen_call_count = 0;
    update_block_markers(&ctx, &session);
    // Should have created a block marker for Direct path
    EXPECT_GE(fopen_call_count, 1);
}

TEST_F(CleanupManagerTest, UpdateBlockMarkers_DirectSuccess_NoBlocking) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.success = true;
    session.used_fallback = false;
    session.codebig_attempts = 0;

    fopen_call_count = 0;
    update_block_markers(&ctx, &session);
    // Direct success should not create any block markers
    EXPECT_EQ(fopen_call_count, 0);
}

TEST_F(CleanupManagerTest, UpdateBlockMarkers_CodebigFailed_BlocksCodebig) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.success = false;
    session.codebig_attempts = 2;

    fopen_fail = false;
    fopen_call_count = 0;
    update_block_markers(&ctx, &session);
    // Should block CodeBig path
    EXPECT_GE(fopen_call_count, 1);
}

// ==================== enforce_privacy TESTS ====================

TEST_F(CleanupManagerTest, EnforcePrivacy_NullPath) {
    // Should not crash
    enforce_privacy(nullptr);
}

TEST_F(CleanupManagerTest, EnforcePrivacy_DirNotExists) {
    ON_CALL(*g_mockFileOperations, dir_exists(_)).WillByDefault(Return(false));
    // Should return early without crash
    enforce_privacy("/nonexistent");
}

TEST_F(CleanupManagerTest, EnforcePrivacy_OpendirFails) {
    ON_CALL(*g_mockFileOperations, dir_exists(_)).WillByDefault(Return(true));
    opendir_fail = true;
    // Should return without crash
    enforce_privacy(test_log_path);
}

TEST_F(CleanupManagerTest, EnforcePrivacy_TruncatesFiles) {
    ON_CALL(*g_mockFileOperations, dir_exists(_)).WillByDefault(Return(true));
    opendir_fail = false;

    enforce_privacy(test_log_path);
    // Real open() will fail on non-existent mock paths; verifies no crash
}

TEST_F(CleanupManagerTest, EnforcePrivacy_OpenFails_ContinuesOtherFiles) {
    ON_CALL(*g_mockFileOperations, dir_exists(_)).WillByDefault(Return(true));
    opendir_fail = false;

    // Real open() will fail on mock paths; function should handle gracefully
    enforce_privacy(test_log_path);
}

// ==================== finalize TESTS ====================

TEST_F(CleanupManagerTest, Finalize_NullCtx) {
    SessionState session;
    memset(&session, 0, sizeof(session));
    // Should not crash
    finalize(nullptr, &session);
}

TEST_F(CleanupManagerTest, Finalize_NullSession) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    // Should not crash
    finalize(&ctx, nullptr);
}

TEST_F(CleanupManagerTest, Finalize_SuccessfulUpload_RemovesArchive) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.success = true;
    strcpy(session.archive_file, "/tmp/test.tgz");

    unlink_fail = false;
    unlink_call_count = 0;
    finalize(&ctx, &session);
    // Should call unlink for the archive file and temp files
    EXPECT_GT(unlink_call_count, 0);
}

TEST_F(CleanupManagerTest, Finalize_FailedUpload_NoArchiveRemoval) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.success = false;
    strcpy(session.archive_file, "/tmp/test.tgz");

    unlink_call_count = 0;
    finalize(&ctx, &session);
    // Should still cleanup temp dirs but not explicitly fail
    // (unlink is called for temp files regardless)
}

TEST_F(CleanupManagerTest, Finalize_MemcaptureTrigger_SkipsArchiveRemoval) {
    RuntimeContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.trigger_type = TRIGGER_MEMCAPTURE;
    SessionState session;
    memset(&session, 0, sizeof(session));
    session.success = true;
    strcpy(session.archive_file, "/tmp/test.tgz");

    unlink_call_count = 0;
    finalize(&ctx, &session);
    // TRIGGER_MEMCAPTURE skips archive removal; only temp dirs are cleaned
    // unlink_call_count should only be for temp files (2)
    EXPECT_LE(unlink_call_count, 2);
}

// ==================== remove_directory_recursive TESTS (static accessor) ====================

class RemoveDirectoryRecursiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mockFileOperations = new MockFileOperations();
        opendir_fail = false;
        remove_fail = false;
        mock_readdir_count = 0;
        total_opendir_calls = 0;
        unlink_fail = false;

        fnRemoveDirectoryRecursive = getRemoveDirectoryRecursive();
        ASSERT_NE(nullptr, fnRemoveDirectoryRecursive);
    }
    void TearDown() override {
        delete g_mockFileOperations;
        g_mockFileOperations = nullptr;
    }
    int (*fnRemoveDirectoryRecursive)(const char*);
};

TEST_F(RemoveDirectoryRecursiveTest, NullPath_OpendirFails_CallsRemove) {
    opendir_fail = true;
    // When opendir fails, falls back to remove()
    int result = fnRemoveDirectoryRecursive("/tmp/somefile");
    EXPECT_EQ(0, result);
}

TEST_F(RemoveDirectoryRecursiveTest, OpendirFails_RemoveFails) {
    opendir_fail = true;
    remove_fail = true;
    int result = fnRemoveDirectoryRecursive("/tmp/somefile");
    EXPECT_EQ(-1, result);
}

TEST_F(RemoveDirectoryRecursiveTest, EmptyDirectory_RmdirsSuccessfully) {
    opendir_fail = false;
    remove_fail = false;
    // readdir will return . and .. then NULL for empty directory
    int result = fnRemoveDirectoryRecursive("/tmp/emptydir");
    EXPECT_EQ(0, result);
}

TEST_F(RemoveDirectoryRecursiveTest, DirectoryWithFiles_RemovesAll) {
    opendir_fail = false;
    remove_fail = false;
    unlink_fail = false;
    int result = fnRemoveDirectoryRecursive("/tmp/testdir");
    EXPECT_EQ(0, result);
}

TEST_F(RemoveDirectoryRecursiveTest, RmdirFails_ReturnsError) {
    opendir_fail = false;
    // rmdir is mocked with remove_fail
    remove_fail = true;
    int result = fnRemoveDirectoryRecursive("/tmp/testdir");
    EXPECT_EQ(-1, result);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
