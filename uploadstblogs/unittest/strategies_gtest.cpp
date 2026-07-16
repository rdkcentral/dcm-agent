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
#include <thread>
#include <chrono>

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
static bool g_copy_file_should_fail = false;
static int g_copy_files_return_count = 3;
static int g_copy_files_to_dcm_path_call_count = 0;
static int g_execute_upload_cycle_call_count = 0;
// Mock implementations for uploadlogsnow module dependencies
bool copy_file(const char* src, const char* dest) {
    return g_copy_file_should_fail ? false : true;
}

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
int cleanup_old_log_backups(const char* log_path, int max_age_days);

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

// Mock for emit_upload_aborted used by strategies.c
void emit_upload_aborted(void) {
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

int cleanup_old_log_backups(const char* log_path, int max_age_days) {
    return 0; // Success
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
    
    // 3. Check if temp directory exists (assume it doesn't)
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    // 4. Create temp directory
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // 5. Collect logs
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(5));  // Return number of files collected
    
    // 6. Open lastlog_path file for writing
    EXPECT_CALL(mock_file_ops, fopen(_, StrEq("a")))
        .WillOnce(Return(reinterpret_cast<FILE*>(0x123)));  // Non-null pointer
    
    // 7. Write to the file
    EXPECT_CALL(mock_file_ops, fprintf(_, _, _))
        .WillOnce(Return(10));  // Number of characters written
    
    // 8. Close the file
    EXPECT_CALL(mock_file_ops, fclose(_))
        .WillOnce(Return(0));
    
    // 8. Check if old tar file exists
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
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

        // Create sentinel files required by reboot_setup prerequisites
        CreateSentinel(BACKUP_LOGS_DONE_FLAG);
        CreateSentinel(STT_FLAG);
        CreateSentinel(PATH_FLAG_INVOCATION);
        CreateSentinel(TELEMETRY_PREVLOGS_DONE_FLAG);
    }
    
    void TearDown() override {
        unlink(BACKUP_LOGS_DONE_FLAG);
        unlink(STT_FLAG);
        unlink(PATH_FLAG_INVOCATION);
        unlink(TELEMETRY_PREVLOGS_DONE_FLAG);
    }

    void CreateSentinel(const char* path) {
        int fd = open(path, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }
    
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

        // Create sentinel files required by reboot_setup prerequisites
        CreateSentinel(BACKUP_LOGS_DONE_FLAG);
        CreateSentinel(STT_FLAG);
        CreateSentinel(PATH_FLAG_INVOCATION);
        CreateSentinel(TELEMETRY_PREVLOGS_DONE_FLAG);
    }
    
    void TearDown() override {
        unlink(BACKUP_LOGS_DONE_FLAG);
        unlink(STT_FLAG);
        unlink(PATH_FLAG_INVOCATION);
        unlink(TELEMETRY_PREVLOGS_DONE_FLAG);
    }

    void CreateSentinel(const char* path) {
        int fd = open(path, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }
    
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

// ==================== WAIT FOR SENTINEL TESTS ====================

class WaitForSentinelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure g_mock_file_ops is NULL so we use real system calls
        g_mock_file_ops = nullptr;

        // Create unique temp directory using PID for test isolation
        snprintf(test_dir_, sizeof(test_dir_), "/tmp/sentinel_test_%d", getpid());
        mkdir(test_dir_, 0755);

        // Setup sentinel file path
        snprintf(sentinel_path_, sizeof(sentinel_path_), "%s/%s", test_dir_, kSentinelName);
    }

    void TearDown() override {
        unlink(sentinel_path_);
        rmdir(test_dir_);
    }

    void CreateSentinelFile() {
        int fd = open(sentinel_path_, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) {
            close(fd);
        }
    }

    void CreateFileInDir(const char* dir, const char* name) {
        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        int fd = open(path, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) {
            close(fd);
        }
    }

    char test_dir_[256];
    char sentinel_path_[256];
    static constexpr const char* kSentinelName = "test_sentinel";
};

/**
 * @test Fast path: sentinel file already exists before wait_for_sentinel is called.
 * Covers: Fast-path access() check at function entry.
 */
TEST_F(WaitForSentinelTest, FastPath_SentinelAlreadyExists) {
    CreateSentinelFile();

    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 5);
    EXPECT_EQ(0, result);
}

/**
 * @test Timeout: sentinel never appears within the specified timeout.
 * Covers: Full inotify loop with clock_gettime deadline expiry.
 */
TEST_F(WaitForSentinelTest, Timeout_SentinelNeverAppears) {
    // Sentinel not created - should timeout after 1 second
    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 1);
    EXPECT_EQ(-1, result);
}

/**
 * @test Inotify detection: sentinel appears after a short delay via IN_CREATE event.
 * Covers: select() wakeup, read() of inotify_event, filename match.
 */
TEST_F(WaitForSentinelTest, Detection_SentinelAppearsAfterDelay) {
    std::thread creator([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CreateSentinelFile();
    });

    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 5);
    creator.join();
    EXPECT_EQ(0, result);
}

/**
 * @test Race condition: sentinel appears between first access() and watch re-check.
 * Covers: Re-check after inotify_add_watch to close the race window.
 */
TEST_F(WaitForSentinelTest, RaceCondition_SentinelAppearsDuringSetup) {
    // Create sentinel with very short delay - may be caught by the re-check
    std::thread creator([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CreateSentinelFile();
    });

    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 5);
    creator.join();
    EXPECT_EQ(0, result);
}

/**
 * @test Zero timeout: should enter loop but immediately break on deadline check.
 * Covers: deadline.tv_sec += 0, immediate expiry in while loop.
 */
TEST_F(WaitForSentinelTest, ZeroTimeout_ReturnsNegativeOne) {
    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 0);
    EXPECT_EQ(-1, result);
}

/**
 * @test Invalid watch directory: inotify_add_watch fails on non-existent directory.
 * Covers: inotify_add_watch failure path and close(ifd).
 */
TEST_F(WaitForSentinelTest, InvalidWatchDir_Timeout) {
    const char* bad_dir = "/nonexistent_sentinel_test_dir_xyz";
    const char* bad_path = "/nonexistent_sentinel_test_dir_xyz/sentinel";

    int result = wait_for_sentinel(bad_path, bad_dir, "sentinel", 1);
    EXPECT_EQ(-1, result);
}

/**
 * @test Wrong filename created in watched directory - should not trigger detection.
 * Covers: inotify event filename comparison (strcmp != 0 path).
 */
TEST_F(WaitForSentinelTest, WrongFilename_DoesNotMatch) {
    std::thread creator([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // Create a file with a DIFFERENT name
        CreateFileInDir(test_dir_, "not_the_sentinel");
    });

    // Wait for "test_sentinel" which will never appear
    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 2);
    creator.join();
    EXPECT_EQ(-1, result);

    // Clean up the wrong file
    char wrong_path[256];
    snprintf(wrong_path, sizeof(wrong_path), "%s/not_the_sentinel", test_dir_);
    unlink(wrong_path);
}

/**
 * @test Multiple sequential calls with sentinel present - consistent behavior.
 * Covers: Function is idempotent and has no lingering state.
 */
TEST_F(WaitForSentinelTest, MultipleCalls_ConsistentBehavior) {
    CreateSentinelFile();

    EXPECT_EQ(0, wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 1));
    EXPECT_EQ(0, wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 1));
    EXPECT_EQ(0, wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 1));
}

/**
 * @test Sentinel removed then re-checked - absence detected after removal.
 * Covers: Ensures no caching of previous access() results.
 */
TEST_F(WaitForSentinelTest, SentinelRemovedThenRechecked) {
    CreateSentinelFile();
    EXPECT_EQ(0, wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 1));

    // Remove sentinel
    unlink(sentinel_path_);

    // Now should timeout since sentinel is gone
    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 1);
    EXPECT_EQ(-1, result);
}

/**
 * @test wait_for_reboot_reason wrapper: sentinel present -> returns 0.
 * Covers: PATH_FLAG_INVOCATION sentinel with production constants.
 */
TEST_F(WaitForSentinelTest, WaitForRebootReason_SentinelPresent) {
    int fd = open(PATH_FLAG_INVOCATION, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        GTEST_SKIP() << "Cannot create " << PATH_FLAG_INVOCATION;
    }
    close(fd);

    int result = wait_for_reboot_reason();
    EXPECT_EQ(0, result);

    unlink(PATH_FLAG_INVOCATION);
}

/**
 * @test wait_for_reboot_reason wrapper: sentinel absent -> returns -1 after timeout.
 * Covers: REBOOT_POLL_TIMEOUT_S timeout (2s in GTEST_ENABLE mode).
 */
TEST_F(WaitForSentinelTest, WaitForRebootReason_Timeout) {
    unlink(PATH_FLAG_INVOCATION);

    int result = wait_for_reboot_reason();
    EXPECT_EQ(-1, result);
}

/**
 * @test wait_for_telemetry_prevlogs_done wrapper: sentinel present -> returns 0.
 * Covers: TELEMETRY_PREVLOGS_DONE_FLAG with production constants.
 */
TEST_F(WaitForSentinelTest, WaitForTelemetryPrevlogsDone_SentinelPresent) {
    int fd = open(TELEMETRY_PREVLOGS_DONE_FLAG, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        GTEST_SKIP() << "Cannot create " << TELEMETRY_PREVLOGS_DONE_FLAG;
    }
    close(fd);

    int result = wait_for_telemetry_prevlogs_done();
    EXPECT_EQ(0, result);

    unlink(TELEMETRY_PREVLOGS_DONE_FLAG);
}

/**
 * @test wait_for_telemetry_prevlogs_done wrapper: sentinel absent -> timeout.
 * Covers: TELEMETRY_PREVLOGS_TIMEOUT_S timeout (2s in GTEST_ENABLE mode).
 */
TEST_F(WaitForSentinelTest, WaitForTelemetryPrevlogsDone_Timeout) {
    unlink(TELEMETRY_PREVLOGS_DONE_FLAG);

    int result = wait_for_telemetry_prevlogs_done();
    EXPECT_EQ(-1, result);
}

/**
 * @test Sentinel appears just before timeout deadline.
 * Covers: select() heartbeat re-checks and event delivery near deadline.
 */
TEST_F(WaitForSentinelTest, Detection_SentinelAppearsNearTimeout) {
    // Create sentinel close to the 3s timeout (at ~2.5s)
    std::thread creator([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        CreateSentinelFile();
    });

    int result = wait_for_sentinel(sentinel_path_, test_dir_, kSentinelName, 4);
    creator.join();
    EXPECT_EQ(0, result);
}

// ==================== HELPER FUNCTION TESTS ====================

/**
 * Test fixture for internet_write_cb, nm_query_ipver, check_internet_connectivity,
 * apply_ntp_fallback_time, and trigger_reboot_info_update.
 */
class HelperFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_file_ops = nullptr;
    }

    void TearDown() override {
        g_mock_file_ops = nullptr;
    }

    void CreateFile(const char* path) {
        int fd = open(path, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }
};

// ---- internet_write_cb tests ----

/**
 * @test Normal write: data fits entirely in buffer.
 * Covers: memcpy path, len update, null terminator.
 */
TEST_F(HelperFunctionsTest, InternetWriteCb_NormalWrite) {
    rpc_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    const char* data = "Hello, World!";
    size_t ret = internet_write_cb((void*)data, 1, strlen(data), &resp);

    EXPECT_EQ(ret, strlen(data));
    EXPECT_EQ(resp.len, strlen(data));
    EXPECT_STREQ(resp.buf, "Hello, World!");
}

/**
 * @test Multiple sequential writes accumulate in buffer.
 * Covers: Appending to existing content via r->len offset.
 */
TEST_F(HelperFunctionsTest, InternetWriteCb_MultipleWrites) {
    rpc_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    const char* part1 = "Hello";
    const char* part2 = ", World!";
    internet_write_cb((void*)part1, 1, strlen(part1), &resp);
    internet_write_cb((void*)part2, 1, strlen(part2), &resp);

    EXPECT_EQ(resp.len, strlen("Hello, World!"));
    EXPECT_STREQ(resp.buf, "Hello, World!");
}

/**
 * @test Buffer overflow protection: data larger than remaining space is truncated.
 * Covers: incoming > space clamp, return value still reports full size*nmemb.
 */
TEST_F(HelperFunctionsTest, InternetWriteCb_BufferOverflowProtection) {
    rpc_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    // Fill buffer almost to capacity (leave 5 bytes + null)
    resp.len = sizeof(resp.buf) - 6;
    memset(resp.buf, 'A', resp.len);

    const char* overflow_data = "OVERFLOW_DATA_THAT_IS_TOO_LONG";
    size_t ret = internet_write_cb((void*)overflow_data, 1, strlen(overflow_data), &resp);

    // Return value is always size*nmemb (curl convention)
    EXPECT_EQ(ret, strlen(overflow_data));
    // Buffer should only contain what fits (5 chars + null)
    EXPECT_EQ(resp.len, sizeof(resp.buf) - 1);
    // Null terminated
    EXPECT_EQ(resp.buf[resp.len], '\0');
}

/**
 * @test Zero-length write returns 0.
 * Covers: size*nmemb == 0 edge case.
 */
TEST_F(HelperFunctionsTest, InternetWriteCb_ZeroLength) {
    rpc_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    size_t ret = internet_write_cb((void*)"data", 0, 0, &resp);

    EXPECT_EQ(ret, 0u);
    EXPECT_EQ(resp.len, 0u);
    EXPECT_EQ(resp.buf[0], '\0');
}

/**
 * @test size != 1: verifies size*nmemb calculation.
 * Covers: Curl may pass size=sizeof(element), nmemb=count.
 */
TEST_F(HelperFunctionsTest, InternetWriteCb_SizeTimesNmemb) {
    rpc_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    const char data[] = "ABCDEF";
    // size=2, nmemb=3 → total 6 bytes
    size_t ret = internet_write_cb((void*)data, 2, 3, &resp);

    EXPECT_EQ(ret, 6u);
    EXPECT_EQ(resp.len, 6u);
    EXPECT_EQ(memcmp(resp.buf, "ABCDEF", 6), 0);
}

// ---- check_internet_connectivity / nm_query_ipver tests ----

/**
 * @test check_internet_connectivity returns false when Thunder is unreachable.
 * Covers: curl_easy_perform failure path (CURLE_COULDNT_CONNECT in CI).
 * Note: In Docker CI, nothing listens on 127.0.0.1:9998.
 */
TEST_F(HelperFunctionsTest, CheckInternetConnectivity_NoThunder) {
    // In CI/test environment, Thunder JSON-RPC is not running
    bool result = check_internet_connectivity();
    EXPECT_FALSE(result);
}

/**
 * @test nm_query_ipver returns false when Thunder is unreachable (IPv4).
 * Covers: curl_easy_perform → CURLE_COULDNT_CONNECT → returns false.
 */
TEST_F(HelperFunctionsTest, NmQueryIpver_IPv4_NoThunder) {
    bool result = nm_query_ipver("IPv4");
    EXPECT_FALSE(result);
}

/**
 * @test nm_query_ipver returns false when Thunder is unreachable (IPv6).
 * Covers: Same failure path for IPv6 variant.
 */
TEST_F(HelperFunctionsTest, NmQueryIpver_IPv6_NoThunder) {
    bool result = nm_query_ipver("IPv6");
    EXPECT_FALSE(result);
}

// ---- apply_ntp_fallback_time tests ----

/**
 * @test apply_ntp_fallback_time returns 0 when clock file is unreadable.
 * Covers: fopen returns NULL → early return 0.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_FileNotReadable) {
    // With g_mock_file_ops = nullptr, fopen always returns nullptr
    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, 0);
}

/**
 * @test apply_ntp_fallback_time returns 0 when clock file is empty.
 * Covers: fopen succeeds, fgets returns NULL → fclose + return 0.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_EmptyFile) {
    // Create an empty temp file
    char temp_file[64];
    snprintf(temp_file, sizeof(temp_file), "/tmp/ntp_test_empty_%d", getpid());
    int fd = open(temp_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    close(fd);

    // Open for reading via fdopen (NOT mocked) to get a valid FILE*
    fd = open(temp_file, O_RDONLY);
    ASSERT_GE(fd, 0);
    FILE* real_fp = fdopen(fd, "r");
    ASSERT_NE(nullptr, real_fp);

    // Temporarily set mock to return our real FILE*
    MockFileOperations mock_ops;
    g_mock_file_ops = &mock_ops;
    EXPECT_CALL(mock_ops, fopen(_, _)).WillOnce(Return(real_fp));
    EXPECT_CALL(mock_ops, fclose(_)).WillOnce(Return(0));

    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, 0);

    g_mock_file_ops = nullptr;
    // Close the real fd (mocked fclose didn't actually close it)
    fclose(real_fp);
    unlink(temp_file);
}

/**
 * @test apply_ntp_fallback_time returns 0 when file contains invalid epoch (non-numeric).
 * Covers: fopen succeeds, fgets succeeds, strtol returns 0 → return 0.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_InvalidEpochString) {
    char temp_file[64];
    snprintf(temp_file, sizeof(temp_file), "/tmp/ntp_test_invalid_%d", getpid());
    int fd = open(temp_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    const char* content = "not_a_number\n";
    write(fd, content, strlen(content));
    close(fd);

    fd = open(temp_file, O_RDONLY);
    ASSERT_GE(fd, 0);
    FILE* real_fp = fdopen(fd, "r");
    ASSERT_NE(nullptr, real_fp);

    MockFileOperations mock_ops;
    g_mock_file_ops = &mock_ops;
    EXPECT_CALL(mock_ops, fopen(_, _)).WillOnce(Return(real_fp));
    EXPECT_CALL(mock_ops, fclose(_)).WillOnce(Return(0));

    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, 0);

    g_mock_file_ops = nullptr;
    fclose(real_fp);
    unlink(temp_file);
}

/**
 * @test apply_ntp_fallback_time returns 0 when file contains negative epoch.
 * Covers: fopen succeeds, fgets succeeds, strtol returns < 0 → return 0.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_NegativeEpoch) {
    char temp_file[64];
    snprintf(temp_file, sizeof(temp_file), "/tmp/ntp_test_neg_%d", getpid());
    int fd = open(temp_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    const char* content = "-100\n";
    write(fd, content, strlen(content));
    close(fd);

    fd = open(temp_file, O_RDONLY);
    ASSERT_GE(fd, 0);
    FILE* real_fp = fdopen(fd, "r");
    ASSERT_NE(nullptr, real_fp);

    MockFileOperations mock_ops;
    g_mock_file_ops = &mock_ops;
    EXPECT_CALL(mock_ops, fopen(_, _)).WillOnce(Return(real_fp));
    EXPECT_CALL(mock_ops, fclose(_)).WillOnce(Return(0));

    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, 0);

    g_mock_file_ops = nullptr;
    fclose(real_fp);
    unlink(temp_file);
}

/**
 * @test apply_ntp_fallback_time returns 0 when file contains zero.
 * Covers: fopen succeeds, fgets succeeds, strtol returns 0 (epoch <= 0) → return 0.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_ZeroEpoch) {
    char temp_file[64];
    snprintf(temp_file, sizeof(temp_file), "/tmp/ntp_test_zero_%d", getpid());
    int fd = open(temp_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    const char* content = "0\n";
    write(fd, content, strlen(content));
    close(fd);

    fd = open(temp_file, O_RDONLY);
    ASSERT_GE(fd, 0);
    FILE* real_fp = fdopen(fd, "r");
    ASSERT_NE(nullptr, real_fp);

    MockFileOperations mock_ops;
    g_mock_file_ops = &mock_ops;
    EXPECT_CALL(mock_ops, fopen(_, _)).WillOnce(Return(real_fp));
    EXPECT_CALL(mock_ops, fclose(_)).WillOnce(Return(0));

    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, 0);

    g_mock_file_ops = nullptr;
    fclose(real_fp);
    unlink(temp_file);
}

/**
 * @test apply_ntp_fallback_time returns valid epoch on success.
 * Covers: fopen succeeds, fgets succeeds, strtol returns > 0 → return epoch.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_ValidEpoch) {
    char temp_file[64];
    snprintf(temp_file, sizeof(temp_file), "/tmp/ntp_test_valid_%d", getpid());
    int fd = open(temp_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    const char* content = "1700000000\n";
    write(fd, content, strlen(content));
    close(fd);

    fd = open(temp_file, O_RDONLY);
    ASSERT_GE(fd, 0);
    FILE* real_fp = fdopen(fd, "r");
    ASSERT_NE(nullptr, real_fp);

    MockFileOperations mock_ops;
    g_mock_file_ops = &mock_ops;
    EXPECT_CALL(mock_ops, fopen(_, _)).WillOnce(Return(real_fp));
    EXPECT_CALL(mock_ops, fclose(_)).WillOnce(Return(0));

    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, (time_t)1700000000);

    g_mock_file_ops = nullptr;
    fclose(real_fp);
    unlink(temp_file);
}

/**
 * @test apply_ntp_fallback_time handles epoch with leading whitespace.
 * Covers: strtol skips leading whitespace per C standard → returns valid epoch.
 */
TEST_F(HelperFunctionsTest, ApplyNtpFallbackTime_EpochWithWhitespace) {
    char temp_file[64];
    snprintf(temp_file, sizeof(temp_file), "/tmp/ntp_test_ws_%d", getpid());
    int fd = open(temp_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GE(fd, 0);
    const char* content = "  1642780800\n";
    write(fd, content, strlen(content));
    close(fd);

    fd = open(temp_file, O_RDONLY);
    ASSERT_GE(fd, 0);
    FILE* real_fp = fdopen(fd, "r");
    ASSERT_NE(nullptr, real_fp);

    MockFileOperations mock_ops;
    g_mock_file_ops = &mock_ops;
    EXPECT_CALL(mock_ops, fopen(_, _)).WillOnce(Return(real_fp));
    EXPECT_CALL(mock_ops, fclose(_)).WillOnce(Return(0));

    time_t result = apply_ntp_fallback_time();
    EXPECT_EQ(result, (time_t)1642780800);

    g_mock_file_ops = nullptr;
    fclose(real_fp);
    unlink(temp_file);
}

// ---- trigger_reboot_info_update tests ----

/**
 * @test trigger_reboot_info_update does nothing when PATH_FLAG_INVOCATION exists.
 * Covers: stat(PATH_FLAG_INVOCATION) succeeds → no STT_FLAG touch.
 */
TEST_F(HelperFunctionsTest, TriggerRebootInfoUpdate_FlagAlreadyPresent) {
    // Create PATH_FLAG_INVOCATION so stat() succeeds
    CreateFile(PATH_FLAG_INVOCATION);
    // Remove STT_FLAG to verify it's NOT created
    unlink(STT_FLAG);

    trigger_reboot_info_update();

    // STT_FLAG should NOT be created since PATH_FLAG_INVOCATION exists
    struct stat st;
    EXPECT_NE(stat(STT_FLAG, &st), 0);

    unlink(PATH_FLAG_INVOCATION);
}

/**
 * @test trigger_reboot_info_update creates STT_FLAG when PATH_FLAG_INVOCATION absent.
 * Covers: stat(PATH_FLAG_INVOCATION) fails → open(STT_FLAG) path.
 */
TEST_F(HelperFunctionsTest, TriggerRebootInfoUpdate_CreatesSTTFlag) {
    // Ensure PATH_FLAG_INVOCATION does NOT exist
    unlink(PATH_FLAG_INVOCATION);
    // Ensure STT_FLAG does NOT exist
    unlink(STT_FLAG);

    trigger_reboot_info_update();

    // STT_FLAG should now exist
    struct stat st;
    EXPECT_EQ(stat(STT_FLAG, &st), 0);

    // Cleanup
    unlink(STT_FLAG);
}

// ---- wait_for_reboot_reason / wait_for_telemetry_prevlogs_done ----
// (Additional tests beyond WaitForSentinelTest fixture)

/**
 * @test wait_for_reboot_reason uses correct constants.
 * Covers: Verifies PATH_FLAG_INVOCATION constant by creating it and checking return.
 */
TEST_F(HelperFunctionsTest, WaitForRebootReason_UsesCorrectPath) {
    CreateFile(PATH_FLAG_INVOCATION);

    int result = wait_for_reboot_reason();
    EXPECT_EQ(0, result);

    unlink(PATH_FLAG_INVOCATION);
}

/**
 * @test wait_for_telemetry_prevlogs_done uses correct constants.
 * Covers: Verifies TELEMETRY_PREVLOGS_DONE_FLAG constant.
 */
TEST_F(HelperFunctionsTest, WaitForTelemetryPrevlogsDone_UsesCorrectPath) {
    CreateFile(TELEMETRY_PREVLOGS_DONE_FLAG);

    int result = wait_for_telemetry_prevlogs_done();
    EXPECT_EQ(0, result);

    unlink(TELEMETRY_PREVLOGS_DONE_FLAG);
}

/**
 * @test wait_for_reboot_reason timeout is short in GTEST_ENABLE mode.
 * Covers: REBOOT_POLL_TIMEOUT_S == 2 when GTEST_ENABLE defined.
 */
TEST_F(HelperFunctionsTest, WaitForRebootReason_ShortTimeoutInTest) {
    unlink(PATH_FLAG_INVOCATION);

    auto start = std::chrono::steady_clock::now();
    int result = wait_for_reboot_reason();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(-1, result);
    // Should complete within ~3s (2s timeout + select heartbeat)
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 5);
}

/**
 * @test wait_for_telemetry_prevlogs_done timeout is short in GTEST_ENABLE mode.
 * Covers: TELEMETRY_PREVLOGS_TIMEOUT_S == 2 when GTEST_ENABLE defined.
 */
TEST_F(HelperFunctionsTest, WaitForTelemetryPrevlogsDone_ShortTimeoutInTest) {
    unlink(TELEMETRY_PREVLOGS_DONE_FLAG);

    auto start = std::chrono::steady_clock::now();
    int result = wait_for_telemetry_prevlogs_done();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(-1, result);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 5);
}

// Entry point for the test executable
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
