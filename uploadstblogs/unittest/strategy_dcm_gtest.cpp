/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
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
 */

/**
 * @file strategy_dcm_gtest.cpp
 * @brief Google Test implementation for strategy_dcm.c
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "uploadstblogs_types.h"
#include "strategy_handler.h"

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH 256
#endif

// External function declarations needed by strategy_dcm.c
bool dir_exists(const char* dirpath);
int add_timestamp_to_files(const char* dirpath);
int collect_pcap_logs(RuntimeContext* ctx, const char* target_dir);
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir);
int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path);
int clear_old_packet_captures(const char* log_path);
bool remove_directory(const char* dirpath);
bool join_path(char* buffer, size_t buffer_size, const char* dir, const char* filename);

// Mock sleep function to avoid delays in tests
unsigned int sleep(unsigned int seconds);

// Declaration for the DCM strategy handler
extern const StrategyHandler dcm_strategy_handler;
}

// Mock implementations for external functions
static bool g_mock_dir_exists = true;
static int g_mock_add_timestamp_result = 0;
static int g_mock_collect_pcap_result = 0;
static int g_mock_create_archive_result = 0;
static int g_mock_upload_archive_result = 0;
static int g_mock_clear_packet_captures_result = 0;
static bool g_mock_remove_directory_result = true;

// Call tracking
static int g_add_timestamp_call_count = 0;
static int g_collect_pcap_call_count = 0;
static int g_create_archive_call_count = 0;
static int g_upload_archive_call_count = 0;
static int g_clear_packet_captures_call_count = 0;
static int g_remove_directory_call_count = 0;
static int g_sleep_call_count = 0;
static unsigned int g_last_sleep_seconds = 0;

// Parameter tracking
static char g_last_timestamp_dir[MAX_PATH_LENGTH];
static char g_last_pcap_target_dir[MAX_PATH_LENGTH];
static char g_last_archive_source_dir[MAX_PATH_LENGTH];
static char g_last_upload_archive_path[MAX_PATH_LENGTH];
static char g_last_clear_log_path[MAX_PATH_LENGTH];
static char g_last_remove_directory[MAX_PATH_LENGTH];

bool dir_exists(const char* dirpath) {
    return g_mock_dir_exists;
}

int add_timestamp_to_files(const char* dirpath) {
    g_add_timestamp_call_count++;
    strncpy(g_last_timestamp_dir, dirpath, sizeof(g_last_timestamp_dir) - 1);
    return g_mock_add_timestamp_result;
}

int collect_pcap_logs(RuntimeContext* ctx, const char* target_dir) {
    g_collect_pcap_call_count++;
    strncpy(g_last_pcap_target_dir, target_dir, sizeof(g_last_pcap_target_dir) - 1);
    return g_mock_collect_pcap_result;
}

int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir) {
    g_create_archive_call_count++;
    strncpy(g_last_archive_source_dir, source_dir, sizeof(g_last_archive_source_dir) - 1);
    return g_mock_create_archive_result;
}

int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path) {
    g_upload_archive_call_count++;
    strncpy(g_last_upload_archive_path, archive_path, sizeof(g_last_upload_archive_path) - 1);
    
    // Simulate execute_upload_cycle behavior: set session->success based on result
    if (session && g_mock_upload_archive_result == 0) {
        session->success = true;
    } else if (session) {
        session->success = false;
    }
    
    return g_mock_upload_archive_result;
}

int clear_old_packet_captures(const char* log_path) {
    g_clear_packet_captures_call_count++;
    strncpy(g_last_clear_log_path, log_path, sizeof(g_last_clear_log_path) - 1);
    return g_mock_clear_packet_captures_result;
}

bool remove_directory(const char* dirpath) {
    g_remove_directory_call_count++;
    strncpy(g_last_remove_directory, dirpath, sizeof(g_last_remove_directory) - 1);
    return g_mock_remove_directory_result;
}

unsigned int sleep(unsigned int seconds) {
    g_sleep_call_count++;
    g_last_sleep_seconds = seconds;
    // Return immediately instead of sleeping in tests
    return 0;
}

bool join_path(char* buffer, size_t buffer_size, const char* dir, const char* filename) {
    if (!buffer || !dir || !filename) {
        return false;
    }
    
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(filename);
    
    // Check if directory path ends with a slash
    bool has_trailing_slash = (dir_len > 0 && dir[dir_len - 1] == '/');
    bool needs_separator = !has_trailing_slash;
    
    // Calculate required size
    size_t required = dir_len + (needs_separator ? 1 : 0) + file_len + 1;
    
    if (required > buffer_size) {
        return false;
    }
    
    // Build the path
    strcpy(buffer, dir);
    if (needs_separator) {
        strcat(buffer, "/");
    }
    strcat(buffer, filename);
    
    return true;
}

// Include the actual implementation for testing
#ifdef GTEST_ENABLE
#include "../src/strategy_dcm.c"
#endif

// Test fixture class
class StrategyDcmTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock states
        g_mock_dir_exists = true;
        g_mock_add_timestamp_result = 0;
        g_mock_collect_pcap_result = 0;
        g_mock_create_archive_result = 0;
        g_mock_upload_archive_result = 0;
        g_mock_clear_packet_captures_result = 0;
        g_mock_remove_directory_result = true;
        
        // Reset call counters
        g_add_timestamp_call_count = 0;
        g_collect_pcap_call_count = 0;
        g_create_archive_call_count = 0;
        g_upload_archive_call_count = 0;
        g_clear_packet_captures_call_count = 0;
        g_remove_directory_call_count = 0;
        g_sleep_call_count = 0;
        g_last_sleep_seconds = 0;
        
        // Clear parameter tracking
        memset(g_last_timestamp_dir, 0, sizeof(g_last_timestamp_dir));
        memset(g_last_pcap_target_dir, 0, sizeof(g_last_pcap_target_dir));
        memset(g_last_archive_source_dir, 0, sizeof(g_last_archive_source_dir));
        memset(g_last_upload_archive_path, 0, sizeof(g_last_upload_archive_path));
        memset(g_last_clear_log_path, 0, sizeof(g_last_clear_log_path));
        memset(g_last_remove_directory, 0, sizeof(g_last_remove_directory));
        
        // Create DCMSettings.conf with upload enabled for tests
        FILE* fp = fopen("/tmp/DCMSettings.conf", "w");
        if (fp) {
            fprintf(fp, "urn:settings:LogUploadSettings:upload=true\n");
            fclose(fp);
        }
        
        // Initialize test context
        memset(&ctx, 0, sizeof(ctx));
        strcpy(ctx.dcm_log_path, "/tmp/dcm_logs");
        strcpy(ctx.log_path, "/tmp/logs");
        ctx.flag = true;
        ctx.include_pcap = false;
        
        // Initialize test session
        memset(&session, 0, sizeof(session));
        strcpy(session.archive_file, "test_archive.tar.gz");
        session.success = false;
    }
    
    void TearDown() override {
        // Clean up test file
        remove("/tmp/DCMSettings.conf");
    }
    
    // Test data
    RuntimeContext ctx;
    SessionState session;
};

// Tests for DCM Strategy Handler Structure
TEST_F(StrategyDcmTest, StrategyHandler_Structure) {
    // Verify handler structure is properly defined
    EXPECT_TRUE(dcm_strategy_handler.setup_phase != nullptr);
    EXPECT_TRUE(dcm_strategy_handler.archive_phase != nullptr);
    EXPECT_TRUE(dcm_strategy_handler.upload_phase != nullptr);
    EXPECT_TRUE(dcm_strategy_handler.cleanup_phase != nullptr);
}

// Tests for dcm_setup function
TEST_F(StrategyDcmTest, Setup_Success) {
    int result = dcm_strategy_handler.setup_phase(&ctx, &session);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_add_timestamp_call_count, 1);
    EXPECT_STREQ(g_last_timestamp_dir, ctx.dcm_log_path);
}

TEST_F(StrategyDcmTest, Setup_DcmLogPathNotExists) {
    g_mock_dir_exists = false;
    
    int result = dcm_strategy_handler.setup_phase(&ctx, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_add_timestamp_call_count, 0);
}

TEST_F(StrategyDcmTest, Setup_UploadFlagFalse) {
    // Write upload=false to DCMSettings.conf
    FILE* fp = fopen("/tmp/DCMSettings.conf", "w");
    if (fp) {
        fprintf(fp, "urn:settings:LogUploadSettings:upload=false\n");
        fclose(fp);
    }
    
    int result = dcm_strategy_handler.setup_phase(&ctx, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_add_timestamp_call_count, 0);
}

TEST_F(StrategyDcmTest, Setup_AddTimestampFails) {
    g_mock_add_timestamp_result = -1;
    
    int result = dcm_strategy_handler.setup_phase(&ctx, &session);
    
    // Should still succeed even if timestamp addition fails
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_add_timestamp_call_count, 1);
}

TEST_F(StrategyDcmTest, Setup_NullContext) {
    int result = dcm_strategy_handler.setup_phase(nullptr, &session);
    
    // Should fail gracefully with null context
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_add_timestamp_call_count, 0); // No operations should be performed
}

TEST_F(StrategyDcmTest, Setup_NullSession) {
    int result = dcm_strategy_handler.setup_phase(&ctx, nullptr);
    
    // Should still work as setup doesn't use session directly
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_add_timestamp_call_count, 1);
}

// Tests for dcm_archive function  
TEST_F(StrategyDcmTest, Archive_Success_NoPcap) {
    int result = dcm_strategy_handler.archive_phase(&ctx, &session);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_collect_pcap_call_count, 0);
    EXPECT_EQ(g_create_archive_call_count, 1);
    EXPECT_EQ(g_sleep_call_count, 1);
    EXPECT_EQ(g_last_sleep_seconds, 60);
    EXPECT_STREQ(g_last_archive_source_dir, ctx.dcm_log_path);
}

TEST_F(StrategyDcmTest, Archive_Success_WithPcap) {
    ctx.include_pcap = true;
    g_mock_collect_pcap_result = 2; // 2 PCAP files collected
    
    int result = dcm_strategy_handler.archive_phase(&ctx, &session);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_collect_pcap_call_count, 1);
    EXPECT_STREQ(g_last_pcap_target_dir, ctx.dcm_log_path);
    EXPECT_EQ(g_create_archive_call_count, 1);
    EXPECT_EQ(g_sleep_call_count, 1);
    EXPECT_EQ(g_last_sleep_seconds, 60);
}

TEST_F(StrategyDcmTest, Archive_CreateArchiveFails) {
    g_mock_create_archive_result = -1;
    
    int result = dcm_strategy_handler.archive_phase(&ctx, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_create_archive_call_count, 1);
    EXPECT_EQ(g_sleep_call_count, 0); // No sleep when archive creation fails
}

TEST_F(StrategyDcmTest, Archive_PcapCollectionNone) {
    ctx.include_pcap = true;
    g_mock_collect_pcap_result = 0; // No PCAP files found
    
    int result = dcm_strategy_handler.archive_phase(&ctx, &session);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_collect_pcap_call_count, 1);
    EXPECT_EQ(g_create_archive_call_count, 1);
    EXPECT_EQ(g_sleep_call_count, 1);
    EXPECT_EQ(g_last_sleep_seconds, 60);
}

TEST_F(StrategyDcmTest, Archive_NullContext) {
    int result = dcm_strategy_handler.archive_phase(nullptr, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_collect_pcap_call_count, 0);
    EXPECT_EQ(g_create_archive_call_count, 0);
}

TEST_F(StrategyDcmTest, Archive_NullSession) {
    int result = dcm_strategy_handler.archive_phase(&ctx, nullptr);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_collect_pcap_call_count, 0);
    EXPECT_EQ(g_create_archive_call_count, 0);
}

// Tests for dcm_upload function
TEST_F(StrategyDcmTest, Upload_Success) {
    int result = dcm_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_upload_archive_call_count, 1);
    EXPECT_TRUE(session.success);
    EXPECT_EQ(g_clear_packet_captures_call_count, 0); // No PCAP clearing
    
    // Check constructed archive path
    char expected_path[MAX_PATH_LENGTH];
    snprintf(expected_path, sizeof(expected_path), "%s/%s", 
             ctx.dcm_log_path, session.archive_file);
    EXPECT_STREQ(g_last_upload_archive_path, expected_path);
}

TEST_F(StrategyDcmTest, Upload_Success_WithPcapClearing) {
    ctx.include_pcap = true;
    
    int result = dcm_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(session.success);
    EXPECT_EQ(g_clear_packet_captures_call_count, 1);
    EXPECT_STREQ(g_last_clear_log_path, ctx.log_path);
}

TEST_F(StrategyDcmTest, Upload_Failure) {
    g_mock_upload_archive_result = -1;
    
    int result = dcm_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_FALSE(session.success);
    EXPECT_EQ(g_upload_archive_call_count, 1);
}

TEST_F(StrategyDcmTest, Upload_LongArchivePath) {
    // Create a very long DCM log path to test buffer overflow protection
    // Make the combined path (dcm_log_path + "/" + archive_file) exceed MAX_PATH_LENGTH
    char long_path[MAX_PATH_LENGTH - 50]; // Leave room for filename and separator
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    strcpy(ctx.dcm_log_path, long_path);
    
    // Create a filename that, when combined with the path, exceeds MAX_PATH_LENGTH
    char long_filename[100]; // This plus the path will exceed MAX_PATH_LENGTH
    memset(long_filename, 'b', sizeof(long_filename) - 1);
    long_filename[sizeof(long_filename) - 1] = '\0';
    
    // Use strncpy to safely copy the filename
    strncpy(session.archive_file, long_filename, sizeof(session.archive_file) - 1);
    session.archive_file[sizeof(session.archive_file) - 1] = '\0';
    
    int result = dcm_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_upload_archive_call_count, 0);
}

TEST_F(StrategyDcmTest, Upload_EmptyArchiveFile) {
    strcpy(session.archive_file, "");
    
    int result = dcm_strategy_handler.upload_phase(&ctx, &session);
    
    // Should still work with empty filename
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_upload_archive_call_count, 1);
}

TEST_F(StrategyDcmTest, Upload_NullContext) {
    int result = dcm_strategy_handler.upload_phase(nullptr, &session);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_upload_archive_call_count, 0);
}

TEST_F(StrategyDcmTest, Upload_NullSession) {
    int result = dcm_strategy_handler.upload_phase(&ctx, nullptr);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_upload_archive_call_count, 0);
}

// Tests for dcm_cleanup function
TEST_F(StrategyDcmTest, Cleanup_Success_UploadSuccess) {
    int result = dcm_strategy_handler.cleanup_phase(&ctx, &session, true);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_remove_directory_call_count, 1);
    EXPECT_STREQ(g_last_remove_directory, ctx.dcm_log_path);
}

TEST_F(StrategyDcmTest, Cleanup_Success_UploadFailed) {
    int result = dcm_strategy_handler.cleanup_phase(&ctx, &session, false);
    
    // Should still clean up even if upload failed
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_remove_directory_call_count, 1);
    EXPECT_STREQ(g_last_remove_directory, ctx.dcm_log_path);
}

TEST_F(StrategyDcmTest, Cleanup_DcmLogPathNotExists) {
    g_mock_dir_exists = false;
    
    int result = dcm_strategy_handler.cleanup_phase(&ctx, &session, true);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_remove_directory_call_count, 0); // Should not try to remove
}

TEST_F(StrategyDcmTest, Cleanup_RemoveDirectoryFails) {
    g_mock_remove_directory_result = false;
    
    int result = dcm_strategy_handler.cleanup_phase(&ctx, &session, true);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_remove_directory_call_count, 1);
}

TEST_F(StrategyDcmTest, Cleanup_NullContext) {
    int result = dcm_strategy_handler.cleanup_phase(nullptr, &session, true);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(g_remove_directory_call_count, 0);
}

TEST_F(StrategyDcmTest, Cleanup_NullSession) {
    int result = dcm_strategy_handler.cleanup_phase(&ctx, nullptr, true);
    
    // Should still work as cleanup doesn't require session parameter
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_remove_directory_call_count, 1);
}

// Integration tests combining multiple phases
TEST_F(StrategyDcmTest, Integration_CompleteWorkflow_Success) {
    // Test complete DCM workflow
    int setup_result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(setup_result, 0);
    
    int archive_result = dcm_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(archive_result, 0);
    
    int upload_result = dcm_strategy_handler.upload_phase(&ctx, &session);
    EXPECT_EQ(upload_result, 0);
    EXPECT_TRUE(session.success);
    
    int cleanup_result = dcm_strategy_handler.cleanup_phase(&ctx, &session, session.success);
    EXPECT_EQ(cleanup_result, 0);
    
    // Verify all functions were called
    EXPECT_EQ(g_add_timestamp_call_count, 1);
    EXPECT_EQ(g_create_archive_call_count, 1);
    EXPECT_EQ(g_upload_archive_call_count, 1);
    EXPECT_EQ(g_remove_directory_call_count, 1);
}

TEST_F(StrategyDcmTest, Integration_CompleteWorkflow_WithPcap) {
    ctx.include_pcap = true;
    g_mock_collect_pcap_result = 3;
    
    // Test complete DCM workflow with PCAP
    int setup_result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(setup_result, 0);
    
    int archive_result = dcm_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(archive_result, 0);
    
    int upload_result = dcm_strategy_handler.upload_phase(&ctx, &session);
    EXPECT_EQ(upload_result, 0);
    
    int cleanup_result = dcm_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(cleanup_result, 0);
    
    // Verify PCAP-related calls
    EXPECT_EQ(g_collect_pcap_call_count, 1);
    EXPECT_EQ(g_clear_packet_captures_call_count, 1);
}

TEST_F(StrategyDcmTest, Integration_WorkflowFailure_SetupFails) {
    // Write upload=false to DCMSettings.conf - Setup will fail
    FILE* fp = fopen("/tmp/DCMSettings.conf", "w");
    if (fp) {
        fprintf(fp, "urn:settings:LogUploadSettings:upload=false\n");
        fclose(fp);
    }
    
    int setup_result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(setup_result, -1);
    
    // Even if setup fails, other phases might still be called in real implementation
    int cleanup_result = dcm_strategy_handler.cleanup_phase(&ctx, &session, false);
    EXPECT_EQ(cleanup_result, 0);
}

TEST_F(StrategyDcmTest, Integration_WorkflowFailure_UploadFails) {
    g_mock_upload_archive_result = -1;
    
    int setup_result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(setup_result, 0);
    
    int archive_result = dcm_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(archive_result, 0);
    
    int upload_result = dcm_strategy_handler.upload_phase(&ctx, &session);
    EXPECT_EQ(upload_result, -1);
    EXPECT_FALSE(session.success);
    
    // Cleanup should still happen even if upload fails
    int cleanup_result = dcm_strategy_handler.cleanup_phase(&ctx, &session, session.success);
    EXPECT_EQ(cleanup_result, 0);
}

// Edge case tests
TEST_F(StrategyDcmTest, EdgeCase_EmptyDcmLogPath) {
    strcpy(ctx.dcm_log_path, "");
    
    int setup_result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(setup_result, 0); // dir_exists("") might return true
    
    int cleanup_result = dcm_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(cleanup_result, 0);
}

TEST_F(StrategyDcmTest, EdgeCase_VeryLongPaths) {
    char long_path[MAX_PATH_LENGTH];
    memset(long_path, 'a', sizeof(long_path) - 2);
    long_path[sizeof(long_path) - 2] = '\0';
    strcpy(ctx.dcm_log_path, long_path);
    
    int setup_result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(setup_result, 0);
    
    // Test that long paths are handled correctly in parameter passing
    EXPECT_EQ(g_add_timestamp_call_count, 1);
    EXPECT_EQ(strncmp(g_last_timestamp_dir, long_path, strlen(long_path)), 0);
}

TEST_F(StrategyDcmTest, EdgeCase_MultipleCalls) {
    // Test that multiple calls to the same function work correctly
    int result1 = dcm_strategy_handler.setup_phase(&ctx, &session);
    int result2 = dcm_strategy_handler.setup_phase(&ctx, &session);
    
    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_EQ(g_add_timestamp_call_count, 2);
}

// Entry point for the test executable
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
