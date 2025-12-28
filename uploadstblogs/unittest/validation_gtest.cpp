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
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <memory>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"
#include "./mocks/mock_rdk_utils.h"
#include "./mocks/mock_file_operations.h"

// Include the source file to test internal functions
extern "C" {
#include "../src/validation.c"
}

using namespace testing;
using namespace std;

class ValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mockRdkUtils = new MockRdkUtils();
        g_mockFileOperations = new MockFileOperations();
        memset(&ctx, 0, sizeof(RuntimeContext));
        
        // Set up default paths in context
        strcpy(ctx.log_path, "/opt/logs");
        strcpy(ctx.prev_log_path, "/opt/logs/PreviousLogs");
        strcpy(ctx.temp_dir, "/tmp");
        strcpy(ctx.archive_path, "/tmp");
        strcpy(ctx.telemetry_path, "/opt/.telemetry");
        strcpy(ctx.dcm_log_path, "/tmp/DCM");
    }

    void TearDown() override {
        // Clean up test files
        unlink("/tmp/test_binary");
        unlink("/tmp/test_config.conf");
        system("rmdir /tmp/test_dir 2>/dev/null");
        
        delete g_mockRdkUtils;
        g_mockRdkUtils = nullptr;
        delete g_mockFileOperations;
        g_mockFileOperations = nullptr;
    }

    RuntimeContext ctx;
};

// Helper functions
void CreateTestFile(const char* filename, const char* content = "") {
    std::ofstream ofs(filename);
    ofs << content;
    chmod(filename, 0755); // Make executable if it's a binary
}

void CreateTestDir(const char* dirname) {
    mkdir(dirname, 0755);
}

// Test validate_directories function
TEST_F(ValidationTest, ValidateDirectories_NullContext) {
    EXPECT_FALSE(validate_directories(nullptr));
}

TEST_F(ValidationTest, ValidateDirectories_Success) {
    // Set up mock expectations for existing directories
    EXPECT_CALL(*g_mockFileOperations, dir_exists(_))
        .WillRepeatedly(Return(true));
    
    EXPECT_TRUE(validate_directories(&ctx));
}

TEST_F(ValidationTest, ValidateDirectories_MissingDirectory) {
    // Set up mock to return false for PREV_LOG_PATH (critical directory)
    EXPECT_CALL(*g_mockFileOperations, dir_exists(_))
        .WillRepeatedly(Return(false));
    
    EXPECT_FALSE(validate_directories(&ctx));
}

// Test validate_configuration function
TEST_F(ValidationTest, ValidateConfiguration_Success) {
    // Set up mock expectations for configuration files
    EXPECT_CALL(*g_mockFileOperations, file_exists(_))
        .WillRepeatedly(Return(true));
    
    EXPECT_TRUE(validate_configuration());
}

TEST_F(ValidationTest, ValidateConfiguration_MissingFiles) {
    // Set up mock to simulate missing configuration files
    EXPECT_CALL(*g_mockFileOperations, file_exists(_))
        .WillRepeatedly(Return(false));
    
    EXPECT_FALSE(validate_configuration());
}

// Test validate_codebig_access function
TEST_F(ValidationTest, ValidateCodebigAccess_Basic) {
    // Note: This function checks for CodeBig configuration and network access
    // The result depends on the test environment
    validate_codebig_access(); // Just verify it doesn't crash
}

// Test validate_system function - main validation entry point
TEST_F(ValidationTest, ValidateSystem_NullContext) {
    EXPECT_FALSE(validate_system(nullptr));
}

TEST_F(ValidationTest, ValidateSystem_Success) {
    // Mock all dependencies to return success
    EXPECT_CALL(*g_mockFileOperations, dir_exists(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockFileOperations, file_exists(_))
        .WillRepeatedly(Return(true));
    
    validate_system(&ctx); // Just verify it doesn't crash
}

// Test edge cases and error conditions
TEST_F(ValidationTest, ValidateSystem_DirectoryValidationFails) {
    // Set all paths to non-existent directories
    strcpy(ctx.log_path, "/nonexistent/path1");
    strcpy(ctx.prev_log_path, "/nonexistent/path2");
    strcpy(ctx.temp_dir, "/nonexistent/path3");
    strcpy(ctx.dcm_log_path, "/nonexistent/path4");
    
    EXPECT_FALSE(validate_system(&ctx));
}

// Test directory validation with specific paths
TEST_F(ValidationTest, ValidateDirectories_AllRequiredPaths) {
    // Test that all required paths are checked
    // Use /tmp for temp_dir since it actually exists and is writable
    // (validate_directories calls access() to check writeability)
    strcpy(ctx.log_path, "/tmp/test_log");
    strcpy(ctx.prev_log_path, "/tmp/test_prev");
    strcpy(ctx.temp_dir, "/tmp");  // Must be real and writable
    strcpy(ctx.archive_path, "/tmp/test_archive");
    strcpy(ctx.telemetry_path, "/tmp/test_telemetry");
    strcpy(ctx.dcm_log_path, "/tmp/test_dcm");
    
    // Mock all directories to exist
    EXPECT_CALL(*g_mockFileOperations, dir_exists(_))
        .WillRepeatedly(Return(true));
    
    EXPECT_TRUE(validate_directories(&ctx));
}

TEST_F(ValidationTest, ValidateDirectories_EmptyPaths) {
    // Test with empty paths - validation should succeed as empty paths are skipped
    memset(&ctx.paths, 0, sizeof(ctx.paths));
    
    // Mock doesn't matter since empty paths are not checked
    EXPECT_TRUE(validate_directories(&ctx));
}

// Integration tests
TEST_F(ValidationTest, FullValidation_MinimalEnvironment) {
    // Set up minimal valid environment
    strcpy(ctx.log_path, "/tmp");
    strcpy(ctx.prev_log_path, "/tmp");
    strcpy(ctx.temp_dir, "/tmp");
    strcpy(ctx.archive_path, "/tmp");
    strcpy(ctx.telemetry_path, "/tmp");
    strcpy(ctx.dcm_log_path, "/tmp");
    
    // Mock all directories to exist
    EXPECT_CALL(*g_mockFileOperations, dir_exists(_))
        .WillRepeatedly(Return(true));
    
    // Should pass directory validation at minimum
    EXPECT_TRUE(validate_directories(&ctx));
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

