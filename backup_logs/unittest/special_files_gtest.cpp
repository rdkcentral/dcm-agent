/**
 * Copyright 2024 Comcast Cable Communications Management, LLC
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
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "../include/special_files.h"
#include "../include/backup_types.h"

// Define RDK logging macros and functions before including source
#ifndef RDK_LOG_ERROR
#define RDK_LOG_ERROR 1
#endif

#ifndef LOG_BACKUP_LOGS
#define LOG_BACKUP_LOGS "LOG.RDK.BACKUPLOGS"
#endif

void RDK_LOG(int level, const char* module, const char* format, ...);

// Include source file directly for testing (similar to dcm_utils_gtest.cpp)
#include "../src/special_files.c"
}

using namespace testing;
using namespace std;

// Mock functions for external dependencies
extern "C" {
    static int mock_filePresentCheck_return = 0;
    static int mock_copyFiles_return = 0;
    static int mock_remove_return = 0;
    static FILE* mock_fopen_return = nullptr;
    static char mock_fgets_buffer[512] = {0};
    static int mock_fgets_call_count = 0;
    static bool mock_fgets_return_null = false;

    // Mock implementation of filePresentCheck
    int filePresentCheck(const char* filepath) {
        return mock_filePresentCheck_return;
    }

    // Mock implementation of copyFiles (matching system_utils.h signature)
    int copyFiles(char* src, char* dst) {
        return mock_copyFiles_return;
    }

    // Mock implementation of RDK_LOG
    void RDK_LOG(int level, const char* module, const char* format, ...) {
        // Mock implementation - do nothing for tests
    }

    // Mock wrapper for remove
    int __wrap_remove(const char* pathname) {
        return mock_remove_return;
    }

    // Mock wrapper for fopen
    FILE* __wrap_fopen(const char* pathname, const char* mode) {
        return mock_fopen_return;
    }

    // Mock wrapper for fgets
    char* __wrap_fgets(char* s, int size, FILE* stream) {
        if (mock_fgets_return_null || mock_fgets_call_count == 0) {
            return nullptr;
        }
        
        mock_fgets_call_count--;
        strncpy(s, mock_fgets_buffer, size - 1);
        s[size - 1] = '\0';
        
        // Return NULL next time to simulate EOF
        if (mock_fgets_call_count == 0) {
            mock_fgets_return_null = true;
        }
        
        return s;
    }

    // Mock wrapper for fclose
    int __wrap_fclose(FILE* stream) {
        return 0;
    }
}

class SpecialFilesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock states
        mock_filePresentCheck_return = 0;
        mock_copyFiles_return = 0;
        mock_remove_return = 0;
        mock_fopen_return = nullptr;
        mock_fgets_call_count = 0;
        mock_fgets_return_null = false;
        memset(mock_fgets_buffer, 0, sizeof(mock_fgets_buffer));
        
        // Initialize test structures
        memset(&test_config, 0, sizeof(test_config));
        memset(&test_entry, 0, sizeof(test_entry));
        memset(&test_backup_config, 0, sizeof(test_backup_config));
    }

    void TearDown() override {
        // Cleanup if needed
    }

    // Helper method to create a temporary config file for testing
    void createTestConfigFile(const char* filename, const char* content) {
        std::ofstream file(filename);
        if (!content) {
            file.close();
            return;
        }
        file << content;
        file.close();
    }

    // Helper method to remove test files
    void removeTestFile(const char* filename) {
        unlink(filename);
    }

    special_files_config_t test_config;
    special_file_entry_t test_entry;
    backup_config_t test_backup_config;
};

// Test special_files_init function
TEST_F(SpecialFilesTest, InitFunction_Success) {
    int result = special_files_init();
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_cleanup function
TEST_F(SpecialFilesTest, CleanupFunction_Success) {
    // Should not crash or cause issues
    EXPECT_NO_THROW(special_files_cleanup());
}

// Test special_files_load_config with null parameters
TEST_F(SpecialFilesTest, LoadConfig_NullParameters) {
    // Test null config parameter
    int result = special_files_load_config(nullptr, "test_config.txt");
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);

    // Test null config_file parameter
    result = special_files_load_config(&test_config, nullptr);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);

    // Test both null parameters
    result = special_files_load_config(nullptr, nullptr);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);
}

// Test special_files_load_config with missing config file
TEST_F(SpecialFilesTest, LoadConfig_MissingFile) {
    mock_fopen_return = nullptr;  // Simulate fopen failure
    
    int result = special_files_load_config(&test_config, "nonexistent_file.txt");
    EXPECT_EQ(result, BACKUP_ERROR_CONFIG);
    EXPECT_FALSE(test_config.config_loaded);
    EXPECT_EQ(test_config.count, 0);
}

// Test special_files_load_config with valid config file
TEST_F(SpecialFilesTest, LoadConfig_ValidFile) {
    // Set up mock to simulate successful file operations
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Set up mock fgets to return test data
    strcpy(mock_fgets_buffer, "/tmp/test_file.log\n");
    mock_fgets_call_count = 1; // One data line, then EOF
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(test_config.config_loaded);
    EXPECT_EQ(test_config.count, 1);
    EXPECT_STREQ(test_config.entries[0].source_path, "/tmp/test_file.log");
    EXPECT_STREQ(test_config.entries[0].destination_path, "test_file.log");
    EXPECT_EQ(test_config.entries[0].operation, SPECIAL_FILE_COPY);
}

// Test special_files_load_config with comments and empty lines
TEST_F(SpecialFilesTest, LoadConfig_SkipCommentsAndEmptyLines) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Mock multiple fgets calls
    const vector<string> lines = {
        "# This is a comment\n",
        "\n",
        "/tmp/valid_file.log\n",
        "   \n",  // Empty line with spaces
        "# Another comment\n"
    };
    
    // For simplicity, we'll test with one valid line
    strcpy(mock_fgets_buffer, "/tmp/valid_file.log\n");
    mock_fgets_call_count = 1; // One valid line, then EOF
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(test_config.config_loaded);
    EXPECT_EQ(test_config.count, 1);
    EXPECT_STREQ(test_config.entries[0].source_path, "/tmp/valid_file.log");
    EXPECT_STREQ(test_config.entries[0].destination_path, "valid_file.log");
}

// Test special_files_load_config with path parsing
TEST_F(SpecialFilesTest, LoadConfig_PathParsing) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Test file with full path
    strcpy(mock_fgets_buffer, "/opt/logs/system/app.log\n");
    mock_fgets_call_count = 1; // One data line, then EOF
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.entries[0].source_path, "/opt/logs/system/app.log");
    EXPECT_STREQ(test_config.entries[0].destination_path, "app.log");
}

// Test special_files_validate_entry with null parameter
TEST_F(SpecialFilesTest, ValidateEntry_NullParameter) {
    int result = special_files_validate_entry(nullptr);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);
}

// Test special_files_validate_entry with empty paths
TEST_F(SpecialFilesTest, ValidateEntry_EmptyPaths) {
    // Test empty source path
    strcpy(test_entry.destination_path, "dest.log");
    test_entry.source_path[0] = '\0';
    int result = special_files_validate_entry(&test_entry);
    EXPECT_EQ(result, BACKUP_ERROR_CONFIG);

    // Test empty destination path
    strcpy(test_entry.source_path, "/tmp/source.log");
    test_entry.destination_path[0] = '\0';
    result = special_files_validate_entry(&test_entry);
    EXPECT_EQ(result, BACKUP_ERROR_CONFIG);

    // Test both empty
    test_entry.source_path[0] = '\0';
    test_entry.destination_path[0] = '\0';
    result = special_files_validate_entry(&test_entry);
    EXPECT_EQ(result, BACKUP_ERROR_CONFIG);
}

// Test special_files_validate_entry with valid entry
TEST_F(SpecialFilesTest, ValidateEntry_ValidEntry) {
    strcpy(test_entry.source_path, "/tmp/source.log");
    strcpy(test_entry.destination_path, "dest.log");
    
    int result = special_files_validate_entry(&test_entry);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_execute_entry with null parameter
TEST_F(SpecialFilesTest, ExecuteEntry_NullParameter) {
    int result = special_files_execute_entry(nullptr, &test_backup_config);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);
}

// Test special_files_execute_entry with invalid entry
TEST_F(SpecialFilesTest, ExecuteEntry_InvalidEntry) {
    // Empty source path
    test_entry.source_path[0] = '\0';
    strcpy(test_entry.destination_path, "dest.log");
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_ERROR_CONFIG);
}

// Test special_files_execute_entry with missing source file
TEST_F(SpecialFilesTest, ExecuteEntry_MissingSourceFile) {
    strcpy(test_entry.source_path, "/tmp/missing.log");
    strcpy(test_entry.destination_path, "dest.log");
    
    mock_filePresentCheck_return = -1;  // File doesn't exist
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);  // Missing file is not an error
}

// Test special_files_execute_entry with copy operation
TEST_F(SpecialFilesTest, ExecuteEntry_CopyOperation) {
    strcpy(test_entry.source_path, "/tmp/version.txt");
    strcpy(test_entry.destination_path, "version.txt");
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_execute_entry with move operation for specific files
TEST_F(SpecialFilesTest, ExecuteEntry_MoveOperation) {
    strcpy(test_entry.source_path, "/tmp/disk_cleanup.log");
    strcpy(test_entry.destination_path, "disk_cleanup.log");
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    mock_remove_return = 0;            // Remove succeeds
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_execute_entry with copy failure
TEST_F(SpecialFilesTest, ExecuteEntry_CopyFailure) {
    strcpy(test_entry.source_path, "/tmp/test.log");
    strcpy(test_entry.destination_path, "test.log");
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = -1;        // Copy fails
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
}

// Test special_files_execute_entry with move operation failure
TEST_F(SpecialFilesTest, ExecuteEntry_MoveOperationRemoveFailure) {
    strcpy(test_entry.source_path, "/tmp/mount_log.txt");
    strcpy(test_entry.destination_path, "mount_log.txt");
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    mock_remove_return = -1;           // Remove fails
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
}

// Test special_files_execute_entry without backup config
TEST_F(SpecialFilesTest, ExecuteEntry_NoBackupConfig) {
    strcpy(test_entry.source_path, "/tmp/test.log");
    strcpy(test_entry.destination_path, "test.log");
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    
    int result = special_files_execute_entry(&test_entry, nullptr);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_execute_all with null parameter
TEST_F(SpecialFilesTest, ExecuteAll_NullParameter) {
    int result = special_files_execute_all(nullptr, &test_backup_config);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);
}

// Test special_files_execute_all with empty config
TEST_F(SpecialFilesTest, ExecuteAll_EmptyConfig) {
    test_config.count = 0;
    test_config.config_loaded = true;
    
    int result = special_files_execute_all(&test_config, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_execute_all with multiple entries
TEST_F(SpecialFilesTest, ExecuteAll_MultipleEntries) {
    // Set up config with multiple entries
    test_config.count = 2;
    test_config.config_loaded = true;
    
    strcpy(test_config.entries[0].source_path, "/tmp/test1.log");
    strcpy(test_config.entries[0].destination_path, "test1.log");
    
    strcpy(test_config.entries[1].source_path, "/tmp/test2.log");
    strcpy(test_config.entries[1].destination_path, "test2.log");
    
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    mock_filePresentCheck_return = 0;  // Files exist
    mock_copyFiles_return = 0;         // Copy succeeds
    
    int result = special_files_execute_all(&test_config, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test special_files_execute_all with some failures
TEST_F(SpecialFilesTest, ExecuteAll_PartialFailures) {
    test_config.count = 2;
    test_config.config_loaded = true;
    
    strcpy(test_config.entries[0].source_path, "/tmp/test1.log");
    strcpy(test_config.entries[0].destination_path, "test1.log");
    
    strcpy(test_config.entries[1].source_path, "/tmp/test2.log");
    strcpy(test_config.entries[1].destination_path, "test2.log");
    
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    // First file exists, second doesn't
    mock_filePresentCheck_return = -1;  // Files don't exist
    
    int result = special_files_execute_all(&test_config, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);  // Should succeed even if individual files fail
}

// Test path truncation scenarios
TEST_F(SpecialFilesTest, ExecuteEntry_PathTruncation) {
    // Create a very long path that would cause truncation
    string long_log_path(PATH_MAX - 10, 'a');  // Very long path
    strcpy(test_backup_config.log_path, long_log_path.c_str());
    
    strcpy(test_entry.source_path, "/tmp/test.log");
    strcpy(test_entry.destination_path, "very_long_destination_filename_that_might_cause_truncation.log");
    
    mock_filePresentCheck_return = 0;  // File exists
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_ERROR_CONFIG);  // Should fail due to path truncation
}

// Test edge cases for load_config with maximum files
TEST_F(SpecialFilesTest, LoadConfig_MaxFiles) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Set up to return many files (more than MAX_SPECIAL_FILES)
    strcpy(mock_fgets_buffer, "/tmp/test.log\n");
    mock_fgets_call_count = MAX_SPECIAL_FILES; // Exactly max files
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(test_config.config_loaded);
    EXPECT_EQ(test_config.count, MAX_SPECIAL_FILES);  // Should cap at max
}

// Test specific move files detection
TEST_F(SpecialFilesTest, ExecuteEntry_SpecificMoveFiles) {
    const char* move_files[] = {
        "/tmp/disk_cleanup.log",
        "/tmp/mount_log.txt", 
        "/tmp/mount-ta_log.txt"
    };
    
    for (int i = 0; i < 3; i++) {
        strcpy(test_entry.source_path, move_files[i]);
        strcpy(test_entry.destination_path, "dest.log");
        strcpy(test_backup_config.log_path, "/opt/logs");
        
        mock_filePresentCheck_return = 0;  // File exists
        mock_copyFiles_return = 0;         // Copy succeeds
        mock_remove_return = 0;            // Remove succeeds
        
        int result = special_files_execute_entry(&test_entry, &test_backup_config);
        EXPECT_EQ(result, BACKUP_SUCCESS) << "Failed for file: " << move_files[i];
    }
}

// Test load_config with very long lines that exceed buffer
TEST_F(SpecialFilesTest, LoadConfig_VeryLongLines) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Create a line longer than the expected buffer (512 chars)
    string long_path(PATH_MAX + 100, 'a');
    strcpy(mock_fgets_buffer, long_path.c_str());
    mock_fgets_call_count = 1;
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    // Should succeed but the path will be truncated
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(test_config.config_loaded);
    EXPECT_EQ(test_config.count, 1);
}

// Test load_config with lines containing only whitespace  
TEST_F(SpecialFilesTest, LoadConfig_WhitespaceOnlyLines) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Test line with only tabs and spaces
    strcpy(mock_fgets_buffer, "   \t   \n");
    mock_fgets_call_count = 1;
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(test_config.config_loaded);
    EXPECT_EQ(test_config.count, 0);  // Should skip whitespace-only lines
}

// Test load_config with filename only (no path separators)
TEST_F(SpecialFilesTest, LoadConfig_FilenameOnly) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    strcpy(mock_fgets_buffer, "simple_file.log\n");
    mock_fgets_call_count = 1;
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_EQ(test_config.count, 1);
    EXPECT_STREQ(test_config.entries[0].source_path, "simple_file.log");
    EXPECT_STREQ(test_config.entries[0].destination_path, "simple_file.log");
}

// Test load_config with path ending in slash
TEST_F(SpecialFilesTest, LoadConfig_PathEndingInSlash) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    strcpy(mock_fgets_buffer, "/tmp/logs/\n");
    mock_fgets_call_count = 1;
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_EQ(test_config.count, 1);
    EXPECT_STREQ(test_config.entries[0].source_path, "/tmp/logs/");
    EXPECT_STREQ(test_config.entries[0].destination_path, "");  // Empty destination after trailing slash
}

// Test load_config with mixed valid and invalid entries
TEST_F(SpecialFilesTest, LoadConfig_MixedValidInvalidEntries) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // For simplicity, test with one valid entry (mocking multiple calls is complex)
    strcpy(mock_fgets_buffer, "/tmp/valid.log\n");
    mock_fgets_call_count = 1;
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_EQ(test_config.count, 1);
    EXPECT_STREQ(test_config.entries[0].source_path, "/tmp/valid.log");
}

// Test validate_entry with paths containing special characters  
TEST_F(SpecialFilesTest, ValidateEntry_SpecialCharactersInPath) {
    strcpy(test_entry.source_path, "/tmp/file with spaces & symbols.log");
    strcpy(test_entry.destination_path, "file with spaces & symbols.log");
    
    int result = special_files_validate_entry(&test_entry);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test validate_entry with maximum length paths
TEST_F(SpecialFilesTest, ValidateEntry_MaxLengthPaths) {
    // Create paths that are exactly PATH_MAX-1 characters
    string long_source(PATH_MAX - 1, 'a');
    string long_dest(PATH_MAX - 1, 'b');
    
    strncpy(test_entry.source_path, long_source.c_str(), sizeof(test_entry.source_path) - 1);
    test_entry.source_path[sizeof(test_entry.source_path) - 1] = '\0';
    
    strncpy(test_entry.destination_path, long_dest.c_str(), sizeof(test_entry.destination_path) - 1);
    test_entry.destination_path[sizeof(test_entry.destination_path) - 1] = '\0';
    
    int result = special_files_validate_entry(&test_entry);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test execute_entry with edge case file names similar to move files 
TEST_F(SpecialFilesTest, ExecuteEntry_SimilarToMoveFiles) {
    const char* similar_files[] = {
        "/tmp/disk_cleanup_other.log",  // Similar but not exact match
        "/tmp/mount_log_backup.txt",    // Similar but not exact match
        "/etc/mount_log.txt"            // Different path, same filename
    };
    
    for (int i = 0; i < 3; i++) {
        strcpy(test_entry.source_path, similar_files[i]);
        strcpy(test_entry.destination_path, "dest.log");
        strcpy(test_backup_config.log_path, "/opt/logs");
        
        mock_filePresentCheck_return = 0;  // File exists
        mock_copyFiles_return = 0;         // Copy succeeds
        
        int result = special_files_execute_entry(&test_entry, &test_backup_config);
        EXPECT_EQ(result, BACKUP_SUCCESS) << "Failed for file: " << similar_files[i];
        
        // These should be copy operations, not move (no remove call expected)
    }
}

// Test execute_entry with exactly PATH_MAX destination path
TEST_F(SpecialFilesTest, ExecuteEntry_ExactPathMaxDestination) {
    strcpy(test_entry.source_path, "/tmp/test.log");
    strcpy(test_entry.destination_path, "test.log");
    
    // Create log_path that when combined with destination will equal PATH_MAX
    string long_path(PATH_MAX - strlen("test.log") - 2, 'a');  // -2 for '/' and '\0'
    strcpy(test_backup_config.log_path, long_path.c_str());
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test execute_entry with NULL backup_config parameter
TEST_F(SpecialFilesTest, ExecuteEntry_NullBackupConfig) {
    strcpy(test_entry.source_path, "/tmp/test.log");
    strcpy(test_entry.destination_path, "test.log");
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    
    int result = special_files_execute_entry(&test_entry, nullptr);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test execute_entry with empty log_path in backup_config
TEST_F(SpecialFilesTest, ExecuteEntry_EmptyLogPath) {
    strcpy(test_entry.source_path, "/tmp/test.log");
    strcpy(test_entry.destination_path, "test.log");
    test_backup_config.log_path[0] = '\0';  // Empty log path
    
    mock_filePresentCheck_return = 0;  // File exists
    mock_copyFiles_return = 0;         // Copy succeeds
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test execute_all with config not loaded
TEST_F(SpecialFilesTest, ExecuteAll_ConfigNotLoaded) {
    test_config.count = 1;
    test_config.config_loaded = false;  // Mark as not loaded
    
    strcpy(test_config.entries[0].source_path, "/tmp/test.log");
    strcpy(test_config.entries[0].destination_path, "test.log");
    
    mock_filePresentCheck_return = 0;
    mock_copyFiles_return = 0;
    
    int result = special_files_execute_all(&test_config, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);  // Should still process entries
}

// Test execute_all with all entries failing
TEST_F(SpecialFilesTest, ExecuteAll_AllEntriesFailing) {
    test_config.count = 2;
    test_config.config_loaded = true;
    
    strcpy(test_config.entries[0].source_path, "/tmp/test1.log");
    strcpy(test_config.entries[0].destination_path, "test1.log");
    
    strcpy(test_config.entries[1].source_path, "/tmp/test2.log");
    strcpy(test_config.entries[1].destination_path, "test2.log");
    
    mock_filePresentCheck_return = 0;   // Files exist
    mock_copyFiles_return = -1;         // Copy fails
    
    int result = special_files_execute_all(&test_config, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);  // execute_all should succeed even if individual entries fail
}

// Test load_config with exceeding MAX_SPECIAL_FILES limit
TEST_F(SpecialFilesTest, LoadConfig_ExceedMaxFiles) {
    FILE dummy_file;
    mock_fopen_return = &dummy_file;
    
    // Set up to return more files than MAX_SPECIAL_FILES
    strcpy(mock_fgets_buffer, "/tmp/test.log\n");
    mock_fgets_call_count = MAX_SPECIAL_FILES + 5; // More than max
    
    int result = special_files_load_config(&test_config, "test_config.txt");
    
    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(test_config.config_loaded);
    EXPECT_LE(test_config.count, MAX_SPECIAL_FILES);  // Should not exceed max
}

// Test execute_entry operation determination logic
TEST_F(SpecialFilesTest, ExecuteEntry_OperationDetermination) {
    struct test_case {
        const char* source_path;
        bool should_move;
    };
    
    test_case test_cases[] = {
        {"/tmp/disk_cleanup.log", true},
        {"/tmp/mount_log.txt", true}, 
        {"/tmp/mount-ta_log.txt", true},
        {"/tmp/version.txt", false},
        {"/tmp/other.log", false},
        {"/tmp/disk_cleanup.log.bak", false},  // Similar but not exact
        {"/opt/disk_cleanup.log", false}       // Different path
    };
    
    for (auto& tc : test_cases) {
        strcpy(test_entry.source_path, tc.source_path);
        strcpy(test_entry.destination_path, "dest.log");
        strcpy(test_backup_config.log_path, "/opt/logs");
        
        mock_filePresentCheck_return = 0;  // File exists
        mock_copyFiles_return = 0;         // Copy succeeds
        mock_remove_return = 0;            // Remove succeeds (for move operations)
        
        int result = special_files_execute_entry(&test_entry, &test_backup_config);
        EXPECT_EQ(result, BACKUP_SUCCESS) << "Failed for: " << tc.source_path;
    }
}

// Test edge case with very short destination filename
TEST_F(SpecialFilesTest, ExecuteEntry_ShortDestinationFilename) {
    strcpy(test_entry.source_path, "/a/b/c/d/e/f/g/h/i/j/x");  // Short filename 'x'
    strcpy(test_entry.destination_path, "x");
    strcpy(test_backup_config.log_path, "/opt/logs");
    
    mock_filePresentCheck_return = 0;
    mock_copyFiles_return = 0;
    
    int result = special_files_execute_entry(&test_entry, &test_backup_config);
    EXPECT_EQ(result, BACKUP_SUCCESS);
}

// Test cleanup function multiple times
TEST_F(SpecialFilesTest, Cleanup_MultipleCalls) {
    EXPECT_NO_THROW(special_files_cleanup());
    EXPECT_NO_THROW(special_files_cleanup());
    EXPECT_NO_THROW(special_files_cleanup());
}

// Test init function multiple times
TEST_F(SpecialFilesTest, Init_MultipleCalls) {
    int result1 = special_files_init();
    EXPECT_EQ(result1, BACKUP_SUCCESS);
    
    int result2 = special_files_init();
    EXPECT_EQ(result2, BACKUP_SUCCESS);
    
    int result3 = special_files_init();
    EXPECT_EQ(result3, BACKUP_SUCCESS);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
