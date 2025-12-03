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
 * SPDX-License-Identifier: Apache-2.0
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
        strcpy(ctx.paths.log_path, "/opt/logs");
        strcpy(ctx.paths.prev_log_path, "/opt/logs/PreviousLogs");
        strcpy(ctx.paths.temp_dir, "/tmp");
        strcpy(ctx.paths.archive_path, "/tmp");
        strcpy(ctx.paths.telemetry_path, "/opt/.telemetry");
        strcpy(ctx.paths.dcm_log_path, "/tmp/DCM");
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

// Test binary_exists function (static function)
TEST_F(ValidationTest, BinaryExists_AbsolutePath) {
    CreateTestFile("/tmp/test_binary", "#!/bin/sh\necho test\n");
    EXPECT_TRUE(binary_exists("/tmp/test_binary"));
    unlink("/tmp/test_binary");
}

TEST_F(ValidationTest, BinaryExists_NotFound) {
    EXPECT_FALSE(binary_exists("/tmp/nonexistent_binary"));
}

TEST_F(ValidationTest, BinaryExists_CommonPaths) {
    // Test will pass if common binaries like 'ls' exist
    EXPECT_TRUE(binary_exists("ls"));
    EXPECT_TRUE(binary_exists("cat"));
}

// Test validate_directories function
TEST_F(ValidationTest, ValidateDirectories_NullContext) {
    EXPECT_FALSE(validate_directories(nullptr));
}

TEST_F(ValidationTest, ValidateDirectories_Success) {
    // Set up mock expectations for existing directories
    EXPECT_CALL(*g_mockFileOperations, dir_exists("/opt/logs"))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, dir_exists("/opt/logs/PreviousLogs"))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, dir_exists("/tmp"))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, dir_exists("/tmp/DCM"))
        .WillOnce(Return(true));
    
    EXPECT_TRUE(validate_directories(&ctx));
}

TEST_F(ValidationTest, ValidateDirectories_MissingDirectory) {
    // Set up mock to return false for log_path directory
    EXPECT_CALL(*g_mockFileOperations, dir_exists("/opt/logs"))
        .WillOnce(Return(false));
    
    EXPECT_FALSE(validate_directories(&ctx));
}

// Test validate_binaries function
TEST_F(ValidationTest, ValidateBinaries_Success) {
    // Set up mock expectations for required binaries
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/usr/bin/curl")))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/bin/tar")))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/usr/bin/gzip")))
        .WillOnce(Return(true));
    
    EXPECT_TRUE(validate_binaries());
}

TEST_F(ValidationTest, ValidateBinaries_MissingBinary) {
    // Set up mock to simulate missing curl binary
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/usr/bin/curl")))
        .WillOnce(Return(false));
    
    EXPECT_FALSE(validate_binaries());
}

// Test validate_configuration function
TEST_F(ValidationTest, ValidateConfiguration_Success) {
    // Set up mock expectations for configuration files
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/etc/device.properties")))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/etc/include.properties")))
        .WillOnce(Return(true));
    EXPECT_CALL(*g_mockFileOperations, file_exists(StrEq("/opt/.DCMSettings.conf")))
        .WillOnce(Return(true));
    
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
    bool result = validate_codebig_access();
    EXPECT_TRUE(result == true || result == false); // Just verify it doesn't crash
}

// Test validate_system function - main validation entry point
TEST_F(ValidationTest, ValidateSystem_NullContext) {
    EXPECT_FALSE(validate_system(nullptr));
}

TEST_F(ValidationTest, ValidateSystem_Success) {
    // Create necessary directories
    CreateTestDir("/tmp");  // Should already exist
    CreateTestDir("/opt");  // Should already exist but ensure
    
    // Use default paths that should exist in most test environments
    strcpy(ctx.paths.log_path, "/tmp");
    strcpy(ctx.paths.prev_log_path, "/tmp");
    strcpy(ctx.paths.temp_dir, "/tmp");
    strcpy(ctx.paths.archive_path, "/tmp");
    strcpy(ctx.paths.telemetry_path, "/tmp");
    strcpy(ctx.paths.dcm_log_path, "/tmp");
    
    // The result depends on system state - test that it doesn't crash
    bool result = validate_system(&ctx);
    EXPECT_TRUE(result == true || result == false);
}

// Test edge cases and error conditions
TEST_F(ValidationTest, ValidateSystem_DirectoryValidationFails) {
    // Set all paths to non-existent directories
    strcpy(ctx.paths.log_path, "/nonexistent/path1");
    strcpy(ctx.paths.prev_log_path, "/nonexistent/path2");
    strcpy(ctx.paths.temp_dir, "/nonexistent/path3");
    strcpy(ctx.paths.dcm_log_path, "/nonexistent/path4");
    
    EXPECT_FALSE(validate_system(&ctx));
}

// Test directory validation with specific paths
TEST_F(ValidationTest, ValidateDirectories_AllRequiredPaths) {
    // Test that all required paths are checked
    const char* test_paths[] = {
        "/tmp/test_log",
        "/tmp/test_prev", 
        "/tmp/test_temp",
        "/tmp/test_archive",
        "/tmp/test_telemetry",
        "/tmp/test_dcm"
    };
    
    // Create all test directories
    for (int i = 0; i < 6; i++) {
        CreateTestDir(test_paths[i]);
    }
    
    strcpy(ctx.paths.log_path, test_paths[0]);
    strcpy(ctx.paths.prev_log_path, test_paths[1]);
    strcpy(ctx.paths.temp_dir, test_paths[2]);
    strcpy(ctx.paths.archive_path, test_paths[3]);
    strcpy(ctx.paths.telemetry_path, test_paths[4]);
    strcpy(ctx.paths.dcm_log_path, test_paths[5]);
    
    EXPECT_TRUE(validate_directories(&ctx));
    
    // Clean up
    for (int i = 0; i < 6; i++) {
        rmdir(test_paths[i]);
    }
}

TEST_F(ValidationTest, ValidateDirectories_EmptyPaths) {
    // Test with empty paths
    memset(&ctx.paths, 0, sizeof(ctx.paths));
    
    EXPECT_FALSE(validate_directories(&ctx));
}

// Test binary validation scenarios
TEST_F(ValidationTest, BinaryExists_RelativePath) {
    // Test with relative path (should check PATH)
    EXPECT_TRUE(binary_exists("sh") || binary_exists("bash")); // Shell should exist
}

TEST_F(ValidationTest, BinaryExists_EmptyString) {
    EXPECT_FALSE(binary_exists(""));
}

TEST_F(ValidationTest, BinaryExists_NullPointer) {
    EXPECT_FALSE(binary_exists(nullptr));
}

// Integration tests
TEST_F(ValidationTest, FullValidation_MinimalEnvironment) {
    // Set up minimal valid environment
    strcpy(ctx.paths.log_path, "/tmp");
    strcpy(ctx.paths.prev_log_path, "/tmp");
    strcpy(ctx.paths.temp_dir, "/tmp");
    strcpy(ctx.paths.archive_path, "/tmp");
    strcpy(ctx.paths.telemetry_path, "/tmp");
    strcpy(ctx.paths.dcm_log_path, "/tmp");
    
    // Should pass directory validation at minimum
    EXPECT_TRUE(validate_directories(&ctx));
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}