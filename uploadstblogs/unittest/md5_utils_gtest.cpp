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
#include <fstream>
#include <cctype>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

// Include the source file to test internal functions
extern "C" {
#include "../src/md5_utils.c"
}

using namespace testing;
using namespace std;

class MD5UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any test files
        unlink("/tmp/md5_test_file.txt");
        unlink("/tmp/empty_test_file.txt");
        unlink("/tmp/sha256_test_file.txt");
    }

    void TearDown() override {
        // Clean up test files
        unlink("/tmp/md5_test_file.txt");
        unlink("/tmp/empty_test_file.txt");
        unlink("/tmp/sha256_test_file.txt");
    }
};

// Helper function to create test files
void CreateTestFile(const char* filename, const char* content) {
    std::ofstream ofs(filename);
    ofs << content;
}

// Test base64_encode function (internal static function)
TEST_F(MD5UtilsTest, Base64Encode_BasicTest) {
    // Test data: "Hello" -> "SGVsbG8="
    unsigned char input[] = "Hello";
    char output[16];
    
    EXPECT_TRUE(base64_encode(input, 5, output, sizeof(output)));
    EXPECT_STREQ(output, "SGVsbG8=");
}

TEST_F(MD5UtilsTest, Base64Encode_EmptyInput) {
    unsigned char input[] = "";
    char output[16];
    
    EXPECT_TRUE(base64_encode(input, 0, output, sizeof(output)));
    EXPECT_STREQ(output, "");
}

TEST_F(MD5UtilsTest, Base64Encode_BufferTooSmall) {
    unsigned char input[] = "Hello World";
    char output[8]; // Too small
    
    EXPECT_FALSE(base64_encode(input, 11, output, sizeof(output)));
}

TEST_F(MD5UtilsTest, Base64Encode_SingleByte) {
    unsigned char input[] = "A";
    char output[8];
    
    EXPECT_TRUE(base64_encode(input, 1, output, sizeof(output)));
    EXPECT_STREQ(output, "QQ==");
}

TEST_F(MD5UtilsTest, Base64Encode_TwoBytes) {
    unsigned char input[] = "AB";
    char output[8];
    
    EXPECT_TRUE(base64_encode(input, 2, output, sizeof(output)));
    EXPECT_STREQ(output, "QUI=");
}

TEST_F(MD5UtilsTest, Base64Encode_ThreeBytes) {
    unsigned char input[] = "ABC";
    char output[8];
    
    EXPECT_TRUE(base64_encode(input, 3, output, sizeof(output)));
    EXPECT_STREQ(output, "QUJD");
}

// Test calculate_file_md5 function
TEST_F(MD5UtilsTest, CalculateFileMD5_NullFilepath) {
    char md5_output[32];
    EXPECT_FALSE(calculate_file_md5(nullptr, md5_output, sizeof(md5_output)));
}

TEST_F(MD5UtilsTest, CalculateFileMD5_NullOutput) {
    EXPECT_FALSE(calculate_file_md5("/tmp/test.txt", nullptr, 32));
}

TEST_F(MD5UtilsTest, CalculateFileMD5_BufferTooSmall) {
    char md5_output[10]; // Too small for MD5 base64 (needs 25 chars)
    EXPECT_FALSE(calculate_file_md5("/tmp/test.txt", md5_output, sizeof(md5_output)));
}

TEST_F(MD5UtilsTest, CalculateFileMD5_FileNotExist) {
    char md5_output[32];
    EXPECT_FALSE(calculate_file_md5("/tmp/nonexistent_file.txt", md5_output, sizeof(md5_output)));
}

TEST_F(MD5UtilsTest, CalculateFileMD5_EmptyFile) {
    CreateTestFile("/tmp/empty_test_file.txt", "");
    char md5_output[32];
    
    EXPECT_TRUE(calculate_file_md5("/tmp/empty_test_file.txt", md5_output, sizeof(md5_output)));
    
    // MD5 of empty file is d41d8cd98f00b204e9800998ecf8427e
    // Base64 encoded: 1B2M2Y8AsgTpgAmY7PhCfg==
    EXPECT_STREQ(md5_output, "1B2M2Y8AsgTpgAmY7PhCfg==");
}

TEST_F(MD5UtilsTest, CalculateFileMD5_SimpleContent) {
    CreateTestFile("/tmp/md5_test_file.txt", "Hello World");
    char md5_output[32];
    
    EXPECT_TRUE(calculate_file_md5("/tmp/md5_test_file.txt", md5_output, sizeof(md5_output)));
    
    // MD5 of "Hello World" is b10a8db164e0754105b7a99be72e3fe5
    // Base64 encoded: sQqNsWTgdUEFt6mb5y4/5Q==
    EXPECT_STREQ(md5_output, "sQqNsWTgdUEFt6mb5y4/5Q==");
}

TEST_F(MD5UtilsTest, CalculateFileMD5_LargeFile) {
    // Create a file with repeated content to test buffer reading
    const char* content = "This is a test file with some content that will be repeated multiple times to test the buffer reading functionality of the MD5 calculation. ";
    std::string large_content;
    for (int i = 0; i < 100; i++) {
        large_content += content;
    }
    
    CreateTestFile("/tmp/md5_test_file.txt", large_content.c_str());
    char md5_output[32];
    
    EXPECT_TRUE(calculate_file_md5("/tmp/md5_test_file.txt", md5_output, sizeof(md5_output)));
    
    // Should return some base64 encoded MD5 (exact value depends on content)
    EXPECT_GT(strlen(md5_output), 20); // Base64 MD5 should be 24 chars + null
    EXPECT_LT(strlen(md5_output), 32);
    
    // Verify it's proper base64 format (ending with = or ==)
    size_t len = strlen(md5_output);
    EXPECT_TRUE(md5_output[len-1] == '=' || md5_output[len-2] == '=');
}

TEST_F(MD5UtilsTest, CalculateFileMD5_ConsistentResults) {
    CreateTestFile("/tmp/md5_test_file.txt", "Consistent test data");
    char md5_output1[32];
    char md5_output2[32];
    
    // Calculate MD5 twice and ensure results are the same
    EXPECT_TRUE(calculate_file_md5("/tmp/md5_test_file.txt", md5_output1, sizeof(md5_output1)));
    EXPECT_TRUE(calculate_file_md5("/tmp/md5_test_file.txt", md5_output2, sizeof(md5_output2)));
    
    EXPECT_STREQ(md5_output1, md5_output2);
}

TEST_F(MD5UtilsTest, CalculateFileMD5_MinimalBuffer) {
    CreateTestFile("/tmp/md5_test_file.txt", "test");
    char md5_output[25]; // Exactly 24 chars + null terminator
    
    EXPECT_TRUE(calculate_file_md5("/tmp/md5_test_file.txt", md5_output, sizeof(md5_output)));
    EXPECT_EQ(strlen(md5_output), 24);
}

// Test edge cases for base64 encoding with different padding scenarios
TEST_F(MD5UtilsTest, Base64Encode_PaddingScenarios) {
    unsigned char input1[] = {0x14, 0xfb, 0x9c, 0x03, 0xd9, 0x7e}; // 6 bytes, no padding needed
    unsigned char input2[] = {0x14, 0xfb, 0x9c, 0x03, 0xd9}; // 5 bytes, one = padding
    unsigned char input3[] = {0x14, 0xfb, 0x9c, 0x03}; // 4 bytes, two = padding
    
    char output1[16], output2[16], output3[16];
    
    EXPECT_TRUE(base64_encode(input1, 6, output1, sizeof(output1)));
    EXPECT_TRUE(base64_encode(input2, 5, output2, sizeof(output2)));
    EXPECT_TRUE(base64_encode(input3, 4, output3, sizeof(output3)));
    
    // Check padding rules:
    // 6 bytes -> 8 chars, no padding
    // 5 bytes -> 8 chars, one = padding  
    // 4 bytes -> 8 chars, two = padding
    EXPECT_EQ(strlen(output1), 8);
    EXPECT_EQ(strlen(output2), 8); 
    EXPECT_EQ(strlen(output3), 8);
    
    // 6 bytes (multiple of 3) should have no padding
    EXPECT_EQ(strchr(output1, '='), nullptr);
    
    // 5 bytes should have one = padding
    EXPECT_NE(strchr(output2, '='), nullptr);
    EXPECT_EQ(output2[7], '=');  // Last char should be =
    EXPECT_NE(output2[6], '=');  // Second to last should not be =
    
    // 4 bytes should have two = padding  
    EXPECT_NE(strchr(output3, '='), nullptr);
    EXPECT_EQ(output3[6], '=');  // Second to last char should be =
    EXPECT_EQ(output3[7], '=');  // Last char should be =
}

// Test binary data with null bytes
TEST_F(MD5UtilsTest, Base64Encode_BinaryData) {
    unsigned char input[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFE};
    char output[16];
    
    EXPECT_TRUE(base64_encode(input, 8, output, sizeof(output)));
    
    // Should encode without issues even with null bytes
    EXPECT_GT(strlen(output), 0);
    EXPECT_EQ(strlen(output), 12); // 8 bytes -> 12 base64 chars (including padding)
}

// Test calculate_file_sha256 function
TEST_F(MD5UtilsTest, CalculateFileSHA256_NullFilepath) {
    char sha256_output[65];
    EXPECT_FALSE(calculate_file_sha256(nullptr, sha256_output, sizeof(sha256_output)));
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_NullOutput) {
    EXPECT_FALSE(calculate_file_sha256("/tmp/test.txt", nullptr, 65));
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_BufferTooSmall) {
    char sha256_output[32]; // Too small for SHA256 hex (needs 65 chars: 64 hex + null)
    EXPECT_FALSE(calculate_file_sha256("/tmp/test.txt", sha256_output, sizeof(sha256_output)));
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_BufferExactlyTooSmall) {
    char sha256_output[64]; // Exactly too small (missing space for null terminator)
    EXPECT_FALSE(calculate_file_sha256("/tmp/test.txt", sha256_output, sizeof(sha256_output)));
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_FileNotExist) {
    char sha256_output[65];
    EXPECT_FALSE(calculate_file_sha256("/tmp/nonexistent_file.txt", sha256_output, sizeof(sha256_output)));
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_EmptyFile) {
    CreateTestFile("/tmp/empty_test_file.txt", "");
    char sha256_output[65];
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/empty_test_file.txt", sha256_output, sizeof(sha256_output)));
    
    // SHA256 of empty file is e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    EXPECT_STREQ(sha256_output, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(strlen(sha256_output), 64); // Should be exactly 64 hex characters
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_SimpleContent) {
    CreateTestFile("/tmp/md5_test_file.txt", "Hello World");
    char sha256_output[65];
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    
    // SHA256 of "Hello World" is a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e
    EXPECT_STREQ(sha256_output, "a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e");
    EXPECT_EQ(strlen(sha256_output), 64);
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_SingleByte) {
    CreateTestFile("/tmp/md5_test_file.txt", "A");
    char sha256_output[65];
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    
    // SHA256 of "A" is 559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd
    EXPECT_STREQ(sha256_output, "559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd");
    EXPECT_EQ(strlen(sha256_output), 64);
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_MultipleCalls) {
    CreateTestFile("/tmp/md5_test_file.txt", "Consistent test data");
    char sha256_output1[65];
    char sha256_output2[65];
    
    // Calculate SHA256 twice and ensure results are the same
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output1, sizeof(sha256_output1)));
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output2, sizeof(sha256_output2)));
    
    EXPECT_STREQ(sha256_output1, sha256_output2);
    EXPECT_EQ(strlen(sha256_output1), 64);
    EXPECT_EQ(strlen(sha256_output2), 64);
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_LargeFile) {
    // Create a file with repeated content to test buffer reading (8192 byte buffer)
    const char* content = "This is a test file with some content that will be repeated multiple times to test the buffer reading functionality of the SHA256 calculation. ";
    std::string large_content;
    for (int i = 0; i < 100; i++) { // About 14KB of data
        large_content += content;
    }
    
    CreateTestFile("/tmp/md5_test_file.txt", large_content.c_str());
    char sha256_output[65];
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    
    // Should return 64 hex characters
    EXPECT_EQ(strlen(sha256_output), 64);
    
    // Verify all characters are valid hex (0-9, a-f)
    for (int i = 0; i < 64; i++) {
        EXPECT_TRUE((sha256_output[i] >= '0' && sha256_output[i] <= '9') || 
                   (sha256_output[i] >= 'a' && sha256_output[i] <= 'f'))
            << "Invalid hex character at position " << i << ": " << sha256_output[i];
    }
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_VeryLargeFile) {
    // Create a file larger than buffer to test multiple read iterations
    const char* content = "Large file test content with various characters: 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()";
    std::string very_large_content;
    for (int i = 0; i < 200; i++) { // About 20KB of data (> 8KB buffer)
        very_large_content += content;
    }
    
    CreateTestFile("/tmp/md5_test_file.txt", very_large_content.c_str());
    char sha256_output[65];
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    EXPECT_EQ(strlen(sha256_output), 64);
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_BinaryContent) {
    // Create a file with binary content including null bytes
    const unsigned char binary_content[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0xFC, 
                                            0x7F, 0x80, 0x81, 0x82, 0x00, 0x00, 0x00, 0x00};
    
    std::ofstream ofs("/tmp/md5_test_file.txt", std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(binary_content), sizeof(binary_content));
    ofs.close();
    
    char sha256_output[65];
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    EXPECT_EQ(strlen(sha256_output), 64);
    
    // Verify it's a valid hex string
    for (int i = 0; i < 64; i++) {
        EXPECT_TRUE(isxdigit(sha256_output[i])) << "Invalid hex digit at position " << i;
    }
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_MinimalBuffer) {
    CreateTestFile("/tmp/md5_test_file.txt", "test");
    char sha256_output[65]; // Exactly 64 chars + null terminator
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    EXPECT_EQ(strlen(sha256_output), 64);
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_LargerBuffer) {
    CreateTestFile("/tmp/md5_test_file.txt", "test");
    char sha256_output[128]; // Larger than needed
    
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output, sizeof(sha256_output)));
    EXPECT_EQ(strlen(sha256_output), 64);
}

TEST_F(MD5UtilsTest, CalculateFileSHA256_DifferentContent_DifferentHashes) {
    CreateTestFile("/tmp/md5_test_file.txt", "content1");
    char sha256_output1[65];
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output1, sizeof(sha256_output1)));
    
    CreateTestFile("/tmp/md5_test_file.txt", "content2");
    char sha256_output2[65];
    EXPECT_TRUE(calculate_file_sha256("/tmp/md5_test_file.txt", sha256_output2, sizeof(sha256_output2)));
    
    // Different content should produce different hashes
    EXPECT_STRNE(sha256_output1, sha256_output2);
    EXPECT_EQ(strlen(sha256_output1), 64);
    EXPECT_EQ(strlen(sha256_output2), 64);
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
