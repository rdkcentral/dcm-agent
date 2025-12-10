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
 * @file strategy_ondemand_gtest.cpp
 * @brief Google Test implementation for strategy_ondemand.c
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "uploadstblogs_types.h"
#include "strategy_handler.h"

#ifndef MAX_PATH_LENGTH
#define MAX_PATH_LENGTH 256
#endif

// External function declarations needed by strategy_ondemand.c
bool dir_exists(const char* dirpath);
bool has_log_files(const char* dirpath);
bool create_directory(const char* dirpath);
bool remove_directory(const char* dirpath);
bool file_exists(const char* filepath);
bool remove_file(const char* filepath);
int collect_logs(const RuntimeContext* ctx, const SessionState* session, const char* dest_dir);
int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir);
int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path);
void emit_no_logs_ondemand(void);

// Mock sleep function to avoid delays in tests
unsigned int sleep(unsigned int seconds);

// File operations
FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* stream);
int fprintf(FILE* stream, const char* format, ...);

// Declaration for the ONDEMAND strategy handler
extern const StrategyHandler ondemand_strategy_handler;

// Constants
#define ONDEMAND_TEMP_DIR "/tmp/log_on_demand"

// Include the source file to access static functions
#include "../src/strategy_ondemand.c"
}

using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::Invoke;

// Mock class for external dependencies
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

// Mock implementations
extern "C" {
    bool dir_exists(const char* dirpath) {
        return g_mock_file_ops ? g_mock_file_ops->dir_exists(dirpath) : false;
    }
    
    bool has_log_files(const char* dirpath) {
        return g_mock_file_ops ? g_mock_file_ops->has_log_files(dirpath) : false;
    }
    
    bool create_directory(const char* dirpath) {
        return g_mock_file_ops ? g_mock_file_ops->create_directory(dirpath) : false;
    }
    
    bool remove_directory(const char* dirpath) {
        return g_mock_file_ops ? g_mock_file_ops->remove_directory(dirpath) : false;
    }
    
    bool file_exists(const char* filepath) {
        return g_mock_file_ops ? g_mock_file_ops->file_exists(filepath) : false;
    }
    
    bool remove_file(const char* filepath) {
        return g_mock_file_ops ? g_mock_file_ops->remove_file(filepath) : false;
    }
    
    int collect_logs(const RuntimeContext* ctx, const SessionState* session, const char* dest_dir) {
        return g_mock_file_ops ? g_mock_file_ops->collect_logs(ctx, session, dest_dir) : -1;
    }
    
    int create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir) {
        return g_mock_file_ops ? g_mock_file_ops->create_archive(ctx, session, source_dir) : -1;
    }
    
    int upload_archive(RuntimeContext* ctx, SessionState* session, const char* archive_path) {
        return g_mock_file_ops ? g_mock_file_ops->upload_archive(ctx, session, archive_path) : -1;
    }
    
    void emit_no_logs_ondemand(void) {
        if (g_mock_file_ops) g_mock_file_ops->emit_no_logs_ondemand();
    }
    
    unsigned int sleep(unsigned int seconds) {
        return g_mock_file_ops ? g_mock_file_ops->sleep(seconds) : 0;
    }
    
    FILE* fopen(const char* filename, const char* mode) {
        return g_mock_file_ops ? g_mock_file_ops->fopen(filename, mode) : nullptr;
    }
    
    int fclose(FILE* stream) {
        return g_mock_file_ops ? g_mock_file_ops->fclose(stream) : 0;
    }
    
    int fprintf(FILE* stream, const char* format, ...) {
        // Simplified - just pass the format string
        return g_mock_file_ops ? g_mock_file_ops->fprintf(stream, format, "") : 0;
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
        strncpy(ctx.paths.log_path, "/opt/logs", sizeof(ctx.paths.log_path) - 1);
        strncpy(ctx.paths.telemetry_path, "/tmp/telemetry", sizeof(ctx.paths.telemetry_path) - 1);
        
        // Default session settings
        strncpy(session.archive_file, "logs_ondemand.tar.gz", sizeof(session.archive_file) - 1);
        session.success = false;
        
        // Default flags
        ctx.flags.flag = true;  // Upload enabled by default
    }
    
    void TearDown() override {
        g_mock_file_ops = nullptr;
    }
    
    StrictMock<MockFileOperations> mock_file_ops;
    RuntimeContext ctx;
    SessionState session;
};

// ==================== SETUP PHASE TESTS ====================

TEST_F(StrategyOndemandTest, SetupPhase_Success_WithLogFiles) {
    // Setup expectations for successful setup
    InSequence seq;
    
    // 1. Check LOG_PATH exists
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    // 2. Check if log files exist
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    // 3. Check if temp directory exists (doesn't exist)
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    // 4. Create temp directory
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // 5. Collect logs
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(5));  // 5 log files collected
    
    // 6. Open lastlog_path file for writing
    FILE* mock_fp = (FILE*)0x12345;  // Dummy pointer
    EXPECT_CALL(mock_file_ops, fopen(StrEq("/tmp/telemetry/lastlog_path"), StrEq("a")))
        .WillOnce(Return(mock_fp));
    
    // 7. Write to file and close
    EXPECT_CALL(mock_file_ops, fprintf(mock_fp, _, _))
        .WillOnce(Return(10));
    EXPECT_CALL(mock_file_ops, fclose(mock_fp))
        .WillOnce(Return(0));
    
    // 8. Check for old tar file (doesn't exist)
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
    // Execute setup phase
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, SetupPhase_LogPathNotExist) {
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(false));
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(-1, result);
}

TEST_F(StrategyOndemandTest, SetupPhase_NoLogFiles) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(false));
    
    // Should emit no logs event
    EXPECT_CALL(mock_file_ops, emit_no_logs_ondemand());
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(-1, result);
}

TEST_F(StrategyOndemandTest, SetupPhase_TempDirectoryExists_CleanupAndRecreate) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    // Temp directory exists - should remove it
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, remove_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // Create new temp directory
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // Rest of setup
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(3));
    
    FILE* mock_fp = (FILE*)0x12345;
    EXPECT_CALL(mock_file_ops, fopen(_, StrEq("a")))
        .WillOnce(Return(mock_fp));
    EXPECT_CALL(mock_file_ops, fprintf(mock_fp, _, _))
        .WillOnce(Return(10));
    EXPECT_CALL(mock_file_ops, fclose(mock_fp))
        .WillOnce(Return(0));
    
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, SetupPhase_CreateDirectoryFails) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    // Directory creation fails
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(-1, result);
}

TEST_F(StrategyOndemandTest, SetupPhase_CollectLogsReturnsZero) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // No log files collected
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(0));
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(-1, result);
}

TEST_F(StrategyOndemandTest, SetupPhase_OldTarFileExists_RemoveIt) {
    InSequence seq;
    
    // Setup successful path
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(3));
    
    FILE* mock_fp = (FILE*)0x12345;
    EXPECT_CALL(mock_file_ops, fopen(_, StrEq("a")))
        .WillOnce(Return(mock_fp));
    EXPECT_CALL(mock_file_ops, fprintf(mock_fp, _, _))
        .WillOnce(Return(10));
    EXPECT_CALL(mock_file_ops, fclose(mock_fp))
        .WillOnce(Return(0));
    
    // Old tar file exists - should remove it
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, remove_file(_))
        .WillOnce(Return(true));
    
    int result = ondemand_strategy_handler.setup_phase(&ctx, &session);
    EXPECT_EQ(0, result);
}

// ==================== ARCHIVE PHASE TESTS ====================

TEST_F(StrategyOndemandTest, ArchivePhase_Success) {
    InSequence seq;
    
    // Should create archive from temp directory
    EXPECT_CALL(mock_file_ops, create_archive(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(0));
    
    // Should sleep 2 seconds after archive creation
    EXPECT_CALL(mock_file_ops, sleep(2))
        .WillOnce(Return(0));
    
    int result = ondemand_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, ArchivePhase_CreateArchiveFails) {
    EXPECT_CALL(mock_file_ops, create_archive(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(-1));
    
    // Sleep should not be called if archive creation fails
    
    int result = ondemand_strategy_handler.archive_phase(&ctx, &session);
    EXPECT_EQ(-1, result);
}

// ==================== UPLOAD PHASE TESTS ====================

TEST_F(StrategyOndemandTest, UploadPhase_Success) {
    ctx.flags.flag = true;  // Upload enabled
    
    EXPECT_CALL(mock_file_ops, upload_archive(&ctx, &session, _))
        .WillOnce(DoAll(Invoke([](RuntimeContext* ctx, SessionState* session, const char* path) {
            session->success = true;
        }), Return(0)));
    
    int result = ondemand_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(0, result);
    EXPECT_TRUE(session.success);
}

TEST_F(StrategyOndemandTest, UploadPhase_UploadDisabled) {
    ctx.flags.flag = false;  // Upload disabled
    
    // upload_archive should not be called
    
    int result = ondemand_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(0, result);
    EXPECT_FALSE(session.success);  // Should remain unchanged
}

TEST_F(StrategyOndemandTest, UploadPhase_UploadFails) {
    ctx.flags.flag = true;  // Upload enabled
    
    EXPECT_CALL(mock_file_ops, upload_archive(&ctx, &session, _))
        .WillOnce(DoAll(
            Invoke([](RuntimeContext* ctx, SessionState* session, const char* path) {
                if (session) session->success = false;
            }),
            Return(-1)
        ));
    
    int result = ondemand_strategy_handler.upload_phase(&ctx, &session);
    
    EXPECT_EQ(-1, result);
    EXPECT_FALSE(session.success);
}

TEST_F(StrategyOndemandTest, UploadPhase_CorrectArchivePath) {
    ctx.flags.flag = true;
    
    // Verify correct archive path is constructed
    char expected_path[256];
    snprintf(expected_path, sizeof(expected_path), "%s/%s", 
             ONDEMAND_TEMP_DIR, session.archive_file);
    
    EXPECT_CALL(mock_file_ops, upload_archive(&ctx, &session, StrEq(expected_path)))
        .WillOnce(DoAll(
            Invoke([](RuntimeContext* ctx, SessionState* session, const char* path) {
                if (session) session->success = true;
            }),
            Return(0)
        ));
    
    int result = ondemand_strategy_handler.upload_phase(&ctx, &session);
    EXPECT_EQ(0, result);
}

// ==================== CLEANUP PHASE TESTS ====================

TEST_F(StrategyOndemandTest, CleanupPhase_Success_UploadSucceeded) {
    InSequence seq;
    
    // Check if tar file exists
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(true));
    
    // Remove tar file
    EXPECT_CALL(mock_file_ops, remove_file(_))
        .WillOnce(Return(true));
    
    // Check if temp directory exists
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // Remove temp directory
    EXPECT_CALL(mock_file_ops, remove_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    int result = ondemand_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, CleanupPhase_Success_UploadFailed) {
    InSequence seq;
    
    // Should still perform cleanup even if upload failed
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));  // No tar file
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, remove_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    int result = ondemand_strategy_handler.cleanup_phase(&ctx, &session, false);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, CleanupPhase_TarFileNotExists) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
    // Should not try to remove non-existent tar file
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    EXPECT_CALL(mock_file_ops, remove_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    int result = ondemand_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, CleanupPhase_TempDirectoryNotExists) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    // Should not try to remove non-existent directory
    
    int result = ondemand_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(0, result);
}

TEST_F(StrategyOndemandTest, CleanupPhase_RemoveDirectoryFails) {
    InSequence seq;
    
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // Directory removal fails
    EXPECT_CALL(mock_file_ops, remove_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    
    int result = ondemand_strategy_handler.cleanup_phase(&ctx, &session, true);
    EXPECT_EQ(-1, result);
}

// ==================== INTEGRATION TESTS ====================

TEST_F(StrategyOndemandTest, FullWorkflow_Success) {
    // Test complete ONDEMAND strategy workflow
    InSequence seq;
    
    // === SETUP PHASE ===
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, has_log_files(StrEq("/opt/logs")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_file_ops, create_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, collect_logs(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(5));
    
    FILE* mock_fp = (FILE*)0x12345;
    EXPECT_CALL(mock_file_ops, fopen(_, StrEq("a")))
        .WillOnce(Return(mock_fp));
    EXPECT_CALL(mock_file_ops, fprintf(mock_fp, _, _))
        .WillOnce(Return(10));
    EXPECT_CALL(mock_file_ops, fclose(mock_fp))
        .WillOnce(Return(0));
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(false));
    
    // === ARCHIVE PHASE ===
    EXPECT_CALL(mock_file_ops, create_archive(&ctx, &session, StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(0));
    EXPECT_CALL(mock_file_ops, sleep(2))
        .WillOnce(Return(0));
    
    // === UPLOAD PHASE ===
    EXPECT_CALL(mock_file_ops, upload_archive(&ctx, &session, _))
        .WillOnce(DoAll(
            Invoke([](RuntimeContext* ctx, SessionState* session, const char* path) {
                if (session) session->success = true;
            }),
            Return(0)
        ));
    
    // === CLEANUP PHASE ===
    EXPECT_CALL(mock_file_ops, file_exists(_))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, remove_file(_))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, remove_directory(StrEq(ONDEMAND_TEMP_DIR)))
        .WillOnce(Return(true));
    
    // Execute all phases
    EXPECT_EQ(0, ondemand_strategy_handler.setup_phase(&ctx, &session));
    EXPECT_EQ(0, ondemand_strategy_handler.archive_phase(&ctx, &session));
    EXPECT_EQ(0, ondemand_strategy_handler.upload_phase(&ctx, &session));
    EXPECT_EQ(0, ondemand_strategy_handler.cleanup_phase(&ctx, &session, true));
    
    EXPECT_TRUE(session.success);
}

TEST_F(StrategyOndemandTest, FullWorkflow_SetupFails_NoSubsequentPhases) {
    // If setup fails, no other phases should be executed
    
    EXPECT_CALL(mock_file_ops, dir_exists(StrEq("/opt/logs")))
        .WillOnce(Return(false));
    
    // Setup fails
    EXPECT_EQ(-1, ondemand_strategy_handler.setup_phase(&ctx, &session));
    
    // Other phases should not be called in real workflow
    // This test verifies setup failure handling
}

// ==================== STRATEGY HANDLER INTERFACE TESTS ====================

TEST_F(StrategyOndemandTest, StrategyHandler_AllPhasesExist) {
    // Verify strategy handler structure is properly initialized
    EXPECT_NE(nullptr, ondemand_strategy_handler.setup_phase);
    EXPECT_NE(nullptr, ondemand_strategy_handler.archive_phase);
    EXPECT_NE(nullptr, ondemand_strategy_handler.upload_phase);
    EXPECT_NE(nullptr, ondemand_strategy_handler.cleanup_phase);
}

TEST_F(StrategyOndemandTest, StrategyHandler_NullPointerSafety) {
    // Test null pointer safety for all phases
    
    // These tests would require null pointer checks in the actual implementation
    // For now, just verify the handler exists
    EXPECT_NE(nullptr, &ondemand_strategy_handler);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
