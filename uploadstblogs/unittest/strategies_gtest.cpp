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
 * @file strategies_gtest.cpp
 * @brief Google Test implementation for all strategies (DCM, Ondemand, Reboot)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "uploadstblogs_types.h"
#include "strategy_handler.h"

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH 256
#endif

// External function declarations needed by strategies.c
bool dir_exists(const char* dirpath);
int add_timestamp_to_files(const char* dirpath);
int collect_pcap_logs(RuntimeContext* ctx, const char* target_dir);
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir);
int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path);
int clear_old_packet_captures(const char* log_path);
bool remove_directory(const char* dirpath);
bool join_path(char* buffer, size_t buffer_size, const char* dir, const char* filename);

// Additional external functions needed by strategies.c
bool has_log_files(const char* dirpath);
bool get_system_uptime(double* uptime);
int remove_old_directories(const char* base_dir, const char* prefix, int keep_count);
bool file_exists(const char* filepath);
bool remove_file(const char* filepath);
void emit_no_logs_reboot(const RuntimeContext* ctx);
void emit_no_logs_ondemand(void);
bool create_directory(const char* dirpath);
int collect_logs(const RuntimeContext* ctx, const SessionState* session, const char* dest_dir);
int remove_timestamp_from_files(const char* dirpath);
int move_directory_contents(const char* source_dir, const char* dest_dir);
int clean_directory(const char* dirpath);
bool rbus_get_bool_param(const char* param_name, bool* value);
bool generate_archive_name(char* buffer, size_t buffer_size, const char* type, const char* timestamp);
int create_dri_archive(RuntimeContext* ctx, const char* archive_path);
void t2_count_notify(char* marker);

// Mock sleep function to avoid delays in tests
unsigned int sleep(unsigned int seconds);

// File operations
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
int fprintf(FILE* stream, const char* format, ...);

// Declaration for strategy handlers
extern const StrategyHandler dcm_strategy_handler;
extern const StrategyHandler ondemand_strategy_handler;
extern const StrategyHandler reboot_strategy_handler;

// Constants
#define ONDEMAND_TEMP_DIR "/tmp/log_on_demand"
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

int clear_old_packet_captures(const char* log_path) {
    g_clear_packet_captures_call_count++;
    strncpy(g_last_clear_log_path, log_path, sizeof(g_last_clear_log_path) - 1);
    return g_mock_clear_packet_captures_result;
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

// Additional mock implementations for strategies.c
bool get_system_uptime(double* uptime) {
    if (uptime) *uptime = 3600.0; // Default: 1 hour uptime
    return true;
}

int remove_old_directories(const char* base_dir, const char* prefix, int keep_count) {
    return 0; // Success
}

void emit_no_logs_reboot(const RuntimeContext* ctx) {
    // No-op for tests
}

int remove_timestamp_from_files(const char* dirpath) {
    return 0; // Success
}

int move_directory_contents(const char* source_dir, const char* dest_dir) {
    return 0; // Success
}

int clean_directory(const char* dirpath) {
    return 0; // Success
}

bool rbus_get_bool_param(const char* param_name, bool* value) {
    if (value) *value = false;
    return true;
}

bool generate_archive_name(char* buffer, size_t buffer_size, const char* type, const char* timestamp) {
    if (buffer && buffer_size > 0) {
        snprintf(buffer, buffer_size, "test_archive_%s.tar.gz", type ? type : "default");
        return true;
    }
    return false;
}

int create_dri_archive(RuntimeContext* ctx, const char* archive_path) {
    return 0; // Success
}

void t2_count_notify(char* marker) {
    // No-op for tests
}

// Include the actual implementation for testing
#ifdef GTEST_ENABLE
#include "../src/strategies.c"
#endif

// ==================== DCM STRATEGY TESTS ====================

// Test fixture class for DCM strategy
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
        
        // Initialize test context
        memset(&ctx, 0, sizeof(ctx));
        strcpy(ctx.log_path, "/opt/logs");
        strcpy(ctx.telemetry_path, "/tmp/telemetry");
        strcpy(ctx.dcm_log_path, "/tmp/dcm_logs");
        
        // Initialize test session
        memset(&session, 0, sizeof(session));
        strcpy(session.archive_file, "test_archive.tar.gz");
        session.success = false;
    }
    
    void TearDown() override {}
    
    RuntimeContext ctx;
    SessionState session;
};

TEST_F(StrategyDcmTest, StrategyHandler_Exists) {
    EXPECT_NE(nullptr, &dcm_strategy_handler);
    EXPECT_NE(nullptr, dcm_strategy_handler.setup_phase);
    EXPECT_NE(nullptr, dcm_strategy_handler.archive_phase);
    EXPECT_NE(nullptr, dcm_strategy_handler.upload_phase);
    EXPECT_NE(nullptr, dcm_strategy_handler.cleanup_phase);
}

TEST_F(StrategyDcmTest, SetupPhase_Success) {
    g_mock_dir_exists = true;
    
    int result = dcm_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_add_timestamp_call_count, 1);
    // Note: collect_pcap_logs is called in archive phase, not setup
}

TEST_F(StrategyDcmTest, ArchivePhase_Success) {
    int result = dcm_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_create_archive_call_count, 1);
}

TEST_F(StrategyDcmTest, ArchivePhase_WithPcap) {
    ctx.include_pcap = true;
    
    int result = dcm_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_collect_pcap_call_count, 1); // Should collect PCAP in archive phase
    EXPECT_EQ(g_create_archive_call_count, 1);
}

TEST_F(StrategyDcmTest, UploadPhase_Success) {
    g_mock_upload_archive_result = 0;
    
    int result = dcm_strategy_handler.upload_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_upload_archive_call_count, 1);
    EXPECT_TRUE(session.success);
}

TEST_F(StrategyDcmTest, CleanupPhase_Success) {
    session.success = true;
    
    int result = dcm_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(result, 0);
}

// ==================== ONDEMAND STRATEGY TESTS ====================

using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::Invoke;

// Mock class for external dependencies for Ondemand tests
class MockFileOperations {
public:
    MOCK_METHOD(bool, dir_exists, (const char* dirpath));
    MOCK_METHOD(bool, has_log_files, (const char* dirpath));
    MOCK_METHOD(bool, create_directory, (const char* dirpath));
    MOCK_METHOD(bool, remove_directory, (const char* dirpath));
    MOCK_METHOD(bool, file_exists, (const char* filepath));
    MOCK_METHOD(bool, remove_file, (const char* filepath));
    MOCK_METHOD(int, collect_logs, (const RuntimeContext* ctx, const SessionState* session, const char* dest_dir));
    MOCK_METHOD(int, create_archive, (RuntimeContext* ctx, SessionState* session, const char* source_dir));
    MOCK_METHOD(int, upload_archive, (RuntimeContext* ctx, SessionState* session, const char* archive_path));
    MOCK_METHOD(void, emit_no_logs_ondemand, ());
    MOCK_METHOD(unsigned int, sleep, (unsigned int seconds));
    MOCK_METHOD(FILE*, fopen, (const char* filename, const char* mode));
    MOCK_METHOD(int, fclose, (FILE* stream));
    MOCK_METHOD(int, fprintf, (FILE* stream, const char* format, const char* arg));
};

static MockFileOperations* g_mock_file_ops = nullptr;

// Mock implementations that delegate to the mock object
extern "C" {
    bool dir_exists(const char* dirpath) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->dir_exists(dirpath);
        }
        return g_mock_dir_exists;
    }
    
    bool has_log_files(const char* dirpath) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->has_log_files(dirpath);
        }
        return true; // Default: assume logs exist
    }
    
    bool create_directory(const char* dirpath) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->create_directory(dirpath);
        }
        return true; // Success
    }
    
    bool remove_directory(const char* dirpath) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->remove_directory(dirpath);
        }
        g_remove_directory_call_count++;
        strncpy(g_last_remove_directory, dirpath, sizeof(g_last_remove_directory) - 1);
        return g_mock_remove_directory_result;
    }
    
    bool file_exists(const char* filepath) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->file_exists(filepath);
        }
        return false; // Default: file doesn't exist
    }
    
    bool remove_file(const char* filepath) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->remove_file(filepath);
        }
        return true; // Success
    }
    
    int collect_logs(const RuntimeContext* ctx, const SessionState* session, const char* dest_dir) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->collect_logs(ctx, session, dest_dir);
        }
        return 0; // Success
    }
    
    int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->create_archive(ctx, session, source_dir);
        }
        g_create_archive_call_count++;
        strncpy(g_last_archive_source_dir, source_dir, sizeof(g_last_archive_source_dir) - 1);
        return g_mock_create_archive_result;
    }
    
    int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->upload_archive(ctx, session, archive_path);
        }
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
    
    void emit_no_logs_ondemand(void) {
        if (g_mock_file_ops) {
            g_mock_file_ops->emit_no_logs_ondemand();
            return;
        }
        // No-op for tests
    }
    
    unsigned int sleep(unsigned int seconds) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->sleep(seconds);
        }
        g_sleep_call_count++;
        g_last_sleep_seconds = seconds;
        // Return immediately instead of sleeping in tests
        return 0;
    }
    
    FILE* fopen(const char* filename, const char* mode) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->fopen(filename, mode);
        }
        return nullptr; // Simplified for tests
    }
    
    int fclose(FILE* stream) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->fclose(stream);
        }
        return 0; // Success
    }
    
    int fprintf(FILE* stream, const char* format, ...) {
        if (g_mock_file_ops) {
            return g_mock_file_ops->fprintf(stream, format, "");
        }
        return 0; // Simplified for tests
    }
}

class StrategyOndemandTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_file_ops = &mock_file_ops;
        
        // Initialize test context and session
        memset(&ctx, 0, sizeof(ctx));
        memset(&session, 0, sizeof(session));
        
        // Setup default paths
        strncpy(ctx.log_path, "/opt/logs", sizeof(ctx.log_path) - 1);
        strncpy(ctx.telemetry_path, "/tmp/telemetry", sizeof(ctx.telemetry_path) - 1);
        
        // Default session settings
        strncpy(session.archive_file, "logs_ondemand.tar.gz", sizeof(session.archive_file) - 1);
        session.success = false;
        
        // Default flags
        ctx.flag = true;  // Upload enabled by default
    }
    
    void TearDown() override {
        g_mock_file_ops = nullptr;
    }
    
    StrictMock<MockFileOperations> mock_file_ops;
    RuntimeContext ctx;
    SessionState session;
};

TEST_F(StrategyOndemandTest, StrategyHandler_Exists) {
    EXPECT_NE(nullptr, &ondemand_strategy_handler);
    EXPECT_NE(nullptr, ondemand_strategy_handler.setup_phase);
    EXPECT_NE(nullptr, ondemand_strategy_handler.archive_phase);
    EXPECT_NE(nullptr, ondemand_strategy_handler.upload_phase);
    EXPECT_NE(nullptr, ondemand_strategy_handler.cleanup_phase);
}

TEST_F(StrategyOndemandTest, SetupPhase_Success_WithLogFiles) {
    // Setup expectations for successful setup
    InSequence seq;
    
    // 1. Check LOG_PATH exists
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    // 2. Check if log files exist
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    // 3. Create temp directory (expect call after removing if exists)
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // 4. Collect logs
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(5));  // Return number of files collected
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(0, result);
}

// ==================== REBOOT STRATEGY TESTS ====================

class StrategyRebootTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock states for reboot tests
        g_mock_dir_exists = true;
        g_mock_create_archive_result = 0;
        g_mock_upload_archive_result = 0;
        
        // Initialize test context
        memset(&ctx, 0, sizeof(ctx));
        strcpy(ctx.log_path, "/opt/logs");
        strcpy(ctx.prev_log_path, "/opt/PreviousLogs");
        ctx.upload_on_reboot = 1;
        
        // Initialize test session
        memset(&session, 0, sizeof(session));
        strcpy(session.archive_file, "reboot_logs.tar.gz");
        session.success = false;
    }
    
    void TearDown() override {}
    
    RuntimeContext ctx;
    SessionState session;
};

TEST_F(StrategyRebootTest, StrategyHandler_Exists) {
    EXPECT_NE(nullptr, &reboot_strategy_handler);
    EXPECT_NE(nullptr, reboot_strategy_handler.setup_phase);
    EXPECT_NE(nullptr, reboot_strategy_handler.archive_phase);
    EXPECT_NE(nullptr, reboot_strategy_handler.upload_phase);
    EXPECT_NE(nullptr, reboot_strategy_handler.cleanup_phase);
}

TEST_F(StrategyRebootTest, SetupPhase_Success) {
    g_mock_dir_exists = true;
    
    int result = reboot_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
}

TEST_F(StrategyRebootTest, ArchivePhase_Success) {
    int result = reboot_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_create_archive_call_count, 1);
}

TEST_F(StrategyRebootTest, UploadPhase_Success) {
    g_mock_upload_archive_result = 0;
    
    int result = reboot_strategy_handler.upload_phase(&ctx, &session);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_upload_archive_call_count, 1);
    EXPECT_TRUE(session.success);
}

// ==================== INTEGRATION TESTS ====================

class StrategiesIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all mock states
        g_mock_dir_exists = true;
        g_mock_add_timestamp_result = 0;
        g_mock_collect_pcap_result = 0;
        g_mock_create_archive_result = 0;
        g_mock_upload_archive_result = 0;
        g_mock_clear_packet_captures_result = 0;
        g_mock_remove_directory_result = true;
        
        // Reset all call counters
        g_add_timestamp_call_count = 0;
        g_collect_pcap_call_count = 0;
        g_create_archive_call_count = 0;
        g_upload_archive_call_count = 0;
        g_clear_packet_captures_call_count = 0;
        g_remove_directory_call_count = 0;
        g_sleep_call_count = 0;
        
        // Initialize common context
        memset(&ctx, 0, sizeof(ctx));
        strcpy(ctx.log_path, "/opt/logs");
        strcpy(ctx.telemetry_path, "/tmp/telemetry");
        strcpy(ctx.dcm_log_path, "/tmp/dcm_logs");
        
        // Initialize common session
        memset(&session, 0, sizeof(session));
        session.success = false;
    }
    
    void TearDown() override {}
    
    RuntimeContext ctx;
    SessionState session;
};

TEST_F(StrategiesIntegrationTest, AllStrategies_FullWorkflow) {
    // Test that all three strategies can run their complete workflow
    
    // DCM Strategy
    strcpy(session.archive_file, "dcm_logs.tar.gz");
    EXPECT_EQ(dcm_strategy_handler.setup_phase(&ctx, &session), 0);
    EXPECT_EQ(dcm_strategy_handler.archive_phase(&ctx, &session), 0);
    EXPECT_EQ(dcm_strategy_handler.upload_phase(&ctx, &session), 0);
    EXPECT_EQ(dcm_strategy_handler.cleanup_phase(&ctx, &session, true), 0);
    
    // Reset for next strategy
    session.success = false;
    g_create_archive_call_count = 0;
    g_upload_archive_call_count = 0;
    
    // Reboot Strategy
    strcpy(session.archive_file, "reboot_logs.tar.gz");
    EXPECT_EQ(reboot_strategy_handler.setup_phase(&ctx, &session), 0);
    EXPECT_EQ(reboot_strategy_handler.archive_phase(&ctx, &session), 0);
    EXPECT_EQ(reboot_strategy_handler.upload_phase(&ctx, &session), 0);
    EXPECT_EQ(reboot_strategy_handler.cleanup_phase(&ctx, &session, true), 0);
}

TEST_F(StrategiesIntegrationTest, ErrorHandling_UploadFailure) {
    // Test that all strategies handle upload failures gracefully
    g_mock_upload_archive_result = -1; // Simulate upload failure
    
    // DCM Strategy - should handle failure
    strcpy(session.archive_file, "dcm_logs.tar.gz");
    EXPECT_EQ(dcm_strategy_handler.setup_phase(&ctx, &session), 0);
    EXPECT_EQ(dcm_strategy_handler.archive_phase(&ctx, &session), 0);
    EXPECT_NE(dcm_strategy_handler.upload_phase(&ctx, &session), 0); // Should fail
    EXPECT_FALSE(session.success); // Should remain false
}

// Entry point for the test executable
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}