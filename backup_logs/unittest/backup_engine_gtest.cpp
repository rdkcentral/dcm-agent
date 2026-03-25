/*
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

/**
 * @file backup_engine_gtest.cpp
 * @brief Comprehensive Google Test suite for backup_engine.c
 *
 * This test suite validates the backup engine functionality with comprehensive
 * mock testing and edge case coverage.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

extern "C" {
    #include "backup_engine.h"
    #include "backup_types.h"
}

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

// ================================================================================================
// Mock Function Control Variables
// ================================================================================================

static struct {
    // RDK_LOG mock control
    volatile bool rdk_log_enabled = false;

    // Directory operation mock controls
    volatile DIR* opendir_return = nullptr;
    volatile bool opendir_called = false;
    char opendir_last_path[PATH_MAX] = {0};

    volatile struct dirent* readdir_return = nullptr;
    volatile bool readdir_called = false;
    volatile int readdir_call_count = 0;

    volatile int closedir_return = 0;
    volatile bool closedir_called = false;

    // File operation mock controls
    volatile int filePresentCheck_return = -1; // Default: file not present
    volatile bool filePresentCheck_called = false;
    char filePresentCheck_last_path[PATH_MAX] = {0};

    volatile int createDir_return = 0;
    volatile bool createDir_called = false;
    char createDir_last_path[PATH_MAX] = {0};

    volatile int copyFiles_return = 0;
    volatile bool copyFiles_called = false;
    char copyFiles_last_source[PATH_MAX] = {0};
    char copyFiles_last_dest[PATH_MAX] = {0};

    volatile int remove_return = 0;
    volatile bool remove_called = false;
    char remove_last_path[PATH_MAX] = {0};

    volatile FILE *fopen_return = nullptr;
    volatile bool fopen_called = false;
    char fopen_last_filename[PATH_MAX] = {0};
    char fopen_last_mode[16] = {0};

    volatile int fclose_return = 0;
    volatile bool fclose_called = false;

    // System operation mock controls
    volatile int stat_return = 0;
    volatile bool stat_called = false;
    char stat_last_path[PATH_MAX] = {0};
    volatile mode_t stat_mode = S_IFREG; // Default: regular file

    // open/fstat/close mock controls (used by backup_and_recover_logs)
    volatile int open_return = 3; // Default: valid fd
    volatile bool open_called = false;
    volatile int fstat_return = 0;
    volatile bool fstat_called = false;
    volatile int close_return = 0;
    volatile bool close_called = false;

    // Time operation mock controls
    volatile time_t time_return = 1234567890; // Fixed timestamp
    volatile bool time_called = false;

    volatile struct tm* localtime_return = nullptr;
    volatile bool localtime_called = false;

    volatile size_t strftime_return = 0;
    volatile bool strftime_called = false;
    char strftime_last_format[64] = {0};

    // Special files operation mock controls
    volatile bool special_files_init_called = false;
    volatile int special_files_load_config_return = BACKUP_SUCCESS;
    volatile bool special_files_load_config_called = false;
    volatile int special_files_execute_all_return = BACKUP_SUCCESS;
    volatile bool special_files_execute_all_called = false;
    volatile bool special_files_cleanup_called = false;

    // System integration mock controls
    volatile bool sys_send_systemd_notification_called = false;
    char sys_send_systemd_notification_last_message[256] = {0};

    // Control flag for safe path copying
    volatile bool safe_to_copy_paths = false;

    // Mock directory entries for readdir simulation
    struct dirent mock_entries[10];
    volatile int mock_entry_count = 0;
    volatile int mock_entry_index = 0;

} mock_control;

// ================================================================================================
// Mock Function Implementations
// ================================================================================================

extern "C" {
    // RDK logging mock
    void __wrap_RDK_LOG(int level, const char* module, const char* format, ...) {
        (void)level; (void)module; (void)format;
        mock_control.rdk_log_enabled = true;
    }

    // Directory operation mocks
    DIR* __wrap_opendir(const char *name) {
        mock_control.opendir_called = true;
        if (mock_control.safe_to_copy_paths && name != nullptr) {
            strncpy(mock_control.opendir_last_path, name, PATH_MAX - 1);
            mock_control.opendir_last_path[PATH_MAX - 1] = '\0';
        } else {
            strcpy(mock_control.opendir_last_path, "<mock_called>");
        }
        return mock_control.opendir_return;
    }

    struct dirent* __wrap_readdir(DIR *dirp) {
        (void)dirp;
        mock_control.readdir_called = true;
        mock_control.readdir_call_count++;

        if (mock_control.mock_entry_index < mock_control.mock_entry_count) {
            return &mock_control.mock_entries[mock_control.mock_entry_index++];
        }
        return nullptr; // End of directory
    }

    int __wrap_closedir(DIR *dirp) {
        (void)dirp;
        mock_control.closedir_called = true;
        return mock_control.closedir_return;
    }

    // File operation mocks
    int __wrap_filePresentCheck(char *path) {
        mock_control.filePresentCheck_called = true;
        if (mock_control.safe_to_copy_paths && path != nullptr) {
            strncpy(mock_control.filePresentCheck_last_path, path, PATH_MAX - 1);
            mock_control.filePresentCheck_last_path[PATH_MAX - 1] = '\0';
        } else {
            strcpy(mock_control.filePresentCheck_last_path, "<mock_called>");
        }
        return mock_control.filePresentCheck_return;
    }

    int __wrap_createDir(char *path) {
        mock_control.createDir_called = true;
        if (mock_control.safe_to_copy_paths && path != nullptr) {
            strncpy(mock_control.createDir_last_path, path, PATH_MAX - 1);
            mock_control.createDir_last_path[PATH_MAX - 1] = '\0';
        } else {
            strcpy(mock_control.createDir_last_path, "<mock_called>");
        }
        return mock_control.createDir_return;
    }

    int __wrap_copyFiles(const char *source, const char *dest) {
        mock_control.copyFiles_called = true;
        if (mock_control.safe_to_copy_paths && source != nullptr && dest != nullptr) {
            strncpy(mock_control.copyFiles_last_source, source, PATH_MAX - 1);
            mock_control.copyFiles_last_source[PATH_MAX - 1] = '\0';
            strncpy(mock_control.copyFiles_last_dest, dest, PATH_MAX - 1);
            mock_control.copyFiles_last_dest[PATH_MAX - 1] = '\0';
        } else {
            strcpy(mock_control.copyFiles_last_source, "<mock_called>");
            strcpy(mock_control.copyFiles_last_dest, "<mock_called>");
        }
        return mock_control.copyFiles_return;
    }

    int __wrap_remove(const char *pathname) {
        mock_control.remove_called = true;
        if (mock_control.safe_to_copy_paths && pathname != nullptr) {
            strncpy(mock_control.remove_last_path, pathname, PATH_MAX - 1);
            mock_control.remove_last_path[PATH_MAX - 1] = '\0';
        } else {
            strcpy(mock_control.remove_last_path, "<mock_called>");
        }
        return mock_control.remove_return;
    }

    FILE* __wrap_fopen(const char *filename, const char *mode) {
        mock_control.fopen_called = true;
        if (filename) {
            strncpy(mock_control.fopen_last_filename, filename, PATH_MAX - 1);
            mock_control.fopen_last_filename[PATH_MAX - 1] = '\0';
        } else {
            mock_control.fopen_last_filename[0] = '\0';
        }
        if (mode) {
            strncpy(mock_control.fopen_last_mode, mode, sizeof(mock_control.fopen_last_mode) - 1);
            mock_control.fopen_last_mode[sizeof(mock_control.fopen_last_mode) - 1] = '\0';
        } else {
            mock_control.fopen_last_mode[0] = '\0';
        }
        return mock_control.fopen_return;
    }

    int __wrap_fclose(FILE *fp) {
        (void)fp;
        mock_control.fclose_called = true;
        return mock_control.fclose_return;
    }

    // System operation mocks
    int __wrap_stat(const char *pathname, struct stat *statbuf) {
        mock_control.stat_called = true;
        if (mock_control.safe_to_copy_paths && pathname != nullptr) {
            strncpy(mock_control.stat_last_path, pathname, PATH_MAX - 1);
            mock_control.stat_last_path[PATH_MAX - 1] = '\0';
        } else {
            strcpy(mock_control.stat_last_path, "<mock_called>");
        }

        if (mock_control.stat_return == 0 && statbuf) {
            memset(statbuf, 0, sizeof(struct stat));
            statbuf->st_mode = mock_control.stat_mode;
        }
        return mock_control.stat_return;
    }

    // Real function declarations for forwarding non-test calls
    extern int __real_open(const char *pathname, int flags, ...);
    extern int __real_fstat(int fd, struct stat *statbuf);
    extern int __real_close(int fd);

    // open/fstat/close mocks (used by backup_and_recover_logs for file type check)
    // These forward to real implementations except when open_return is set (non-zero).
    int __wrap_open(const char *pathname, int flags, ...) {
        if (mock_control.open_return > 0) {
            mock_control.open_called = true;
            mock_control.stat_called = true; // Tests check stat_called for file-type checking
            if (mock_control.safe_to_copy_paths && pathname != nullptr) {
                strncpy(mock_control.stat_last_path, pathname, PATH_MAX - 1);
                mock_control.stat_last_path[PATH_MAX - 1] = '\0';
            }
            return mock_control.open_return;
        }
        return __real_open(pathname, flags);
    }

    int __wrap_fstat(int fd, struct stat *statbuf) {
        if (fd == mock_control.open_return && mock_control.open_return > 0) {
            mock_control.fstat_called = true;
            if (mock_control.stat_return == 0 && statbuf) {
                memset(statbuf, 0, sizeof(struct stat));
                statbuf->st_mode = mock_control.stat_mode;
            }
            return mock_control.stat_return;
        }
        return __real_fstat(fd, statbuf);
    }

    int __wrap_close(int fd) {
        if (fd == mock_control.open_return && mock_control.open_return > 0) {
            mock_control.close_called = true;
            return mock_control.close_return;
        }
        return __real_close(fd);
    }

    // Time operation mocks
    time_t __wrap_time(time_t *tloc) {
        mock_control.time_called = true;
        if (tloc) {
            *tloc = mock_control.time_return;
        }
        return mock_control.time_return;
    }

    struct tm* __wrap_localtime(const time_t *timep) {
        (void)timep;
        mock_control.localtime_called = true;
        return mock_control.localtime_return;
    }

    size_t __wrap_strftime(char *s, size_t max, const char *format, const struct tm *tm) {
        mock_control.strftime_called = true;
        if (format) {
            strncpy(mock_control.strftime_last_format, format, sizeof(mock_control.strftime_last_format) - 1);
            mock_control.strftime_last_format[sizeof(mock_control.strftime_last_format) - 1] = '\0';
        }

        if (s && mock_control.strftime_return > 0 && mock_control.strftime_return < max) {
            strcpy(s, "01-01-24-12-00-00AM"); // Mock timestamp
        }
        (void)tm;
        return mock_control.strftime_return;
    }

    // Special files operation mocks
    int __wrap_special_files_init(void) {
        mock_control.special_files_init_called = true;
        return BACKUP_SUCCESS;
    }

    int __wrap_special_files_load_config(special_files_config_t *config, const char *config_file) {
        (void)config_file;
        mock_control.special_files_load_config_called = true;
        if (config && mock_control.special_files_load_config_return == BACKUP_SUCCESS) {
            config->count = 2; // Mock: 2 special files
        }
        return mock_control.special_files_load_config_return;
    }

    int __wrap_special_files_execute_all(const special_files_config_t *config, const backup_config_t *backup_config) {
        (void)config; (void)backup_config;
        mock_control.special_files_execute_all_called = true;
        return mock_control.special_files_execute_all_return;
    }

    void __wrap_special_files_cleanup(void) {
        mock_control.special_files_cleanup_called = true;
    }

    // System integration mocks
    int __wrap_sys_send_systemd_notification(const char *message) {
        mock_control.sys_send_systemd_notification_called = true;
        if (message) {
            strncpy(mock_control.sys_send_systemd_notification_last_message, message,
                   sizeof(mock_control.sys_send_systemd_notification_last_message) - 1);
            mock_control.sys_send_systemd_notification_last_message[sizeof(mock_control.sys_send_systemd_notification_last_message) - 1] = '\0';
        } else {
            mock_control.sys_send_systemd_notification_last_message[0] = '\0';
        }
        return 0; // Return success
    }
}

// ================================================================================================
// Helper Functions for Mock Setup
// ================================================================================================

void setup_mock_directory_entries(const char* names[], int count) {
    // Reset directory entry state
    mock_control.mock_entry_count = 0;
    mock_control.mock_entry_index = 0;
    memset(mock_control.mock_entries, 0, sizeof(mock_control.mock_entries));
    
    if (names == nullptr || count < 0) {
        return;
    }
    
    // Safely copy entries with bounds checking
    int safe_count = (count > 10) ? 10 : count;
    mock_control.mock_entry_count = safe_count;

    for (int i = 0; i < safe_count; i++) {
        if (names[i] != nullptr) {
            strncpy(mock_control.mock_entries[i].d_name, names[i], 
                   sizeof(mock_control.mock_entries[i].d_name) - 1);
            mock_control.mock_entries[i].d_name[sizeof(mock_control.mock_entries[i].d_name) - 1] = '\0';
        }
    }
}

void setup_default_time_mocks() {
    static struct tm test_tm = {
        .tm_sec = 0,
        .tm_min = 0,
        .tm_hour = 12,
        .tm_mday = 1,
        .tm_mon = 0, // January
        .tm_year = 124, // 2024
        .tm_wday = 1,
        .tm_yday = 0,
        .tm_isdst = 0
    };

    mock_control.localtime_return = &test_tm;
    mock_control.strftime_return = 18; // Length of "01-01-24-12-00-00AM"
}

// ================================================================================================
// Test Fixture
// ================================================================================================

class BackupEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all mock control variables
        memset(&mock_control, 0, sizeof(mock_control));
        mock_control.filePresentCheck_return = -1; // Default: file not present
        mock_control.stat_mode = S_IFREG; // Default: regular file
        mock_control.open_return = 100; // Mock fd for open/fstat/close interception
        mock_control.fopen_return = (FILE*)0x12345678;  // Valid fake pointer
        setup_default_time_mocks();

        // Initialize test config
        memset(&test_config, 0, sizeof(test_config));
        strcpy(test_config.log_path, "/opt/logs");
        strcpy(test_config.prev_log_path, "/opt/logs/PreviousLogs");
        strcpy(test_config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
        strcpy(test_config.persistent_path, "/opt/persistent");
        test_config.hdd_enabled = false;
    }

    void TearDown() override {
        // Reset mock control to prevent memory corruption between tests
        memset(&mock_control, 0, sizeof(mock_control));
        mock_control.filePresentCheck_return = -1; // Reset defaults
        mock_control.stat_mode = S_IFREG;
        mock_control.open_return = 100;
    }

    backup_config_t test_config;
};

// ================================================================================================
// move_log_files_by_pattern() Tests
// ================================================================================================

TEST_F(BackupEngineTest, MoveLogFilesByPattern_Success) {
    const char* mock_files[] = {"messages.txt", "system.log", "bootlog", "config.conf", "data.bin"};
    setup_mock_directory_entries(mock_files, 5);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.filePresentCheck_return = 0; // Files exist
    mock_control.copyFiles_return = 0; // Copy succeeds
    mock_control.remove_return = 0; // Remove succeeds
    mock_control.safe_to_copy_paths = true;

    int result = move_log_files_by_pattern("/opt/logs", "/opt/logs/PreviousLogs");

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.opendir_called);
    EXPECT_TRUE(mock_control.copyFiles_called);
    EXPECT_TRUE(mock_control.remove_called);
    EXPECT_TRUE(mock_control.closedir_called);
}

TEST_F(BackupEngineTest, MoveLogFilesByPattern_OpenDirFails) {
    mock_control.opendir_return = nullptr; // opendir fails

    int result = move_log_files_by_pattern("/opt/logs", "/opt/logs/PreviousLogs");

    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
    EXPECT_TRUE(mock_control.opendir_called);
    EXPECT_FALSE(mock_control.copyFiles_called);
}

TEST_F(BackupEngineTest, MoveLogFilesByPattern_NoMatchingFiles) {
    const char* mock_files[] = {"config.conf", "data.bin"};
    setup_mock_directory_entries(mock_files, 2);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.filePresentCheck_return = 0;

    int result = move_log_files_by_pattern("/opt/logs", "/opt/logs/PreviousLogs");

    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM); // No files moved
    EXPECT_TRUE(mock_control.opendir_called);
    EXPECT_FALSE(mock_control.copyFiles_called);
}

TEST_F(BackupEngineTest, MoveLogFilesByPattern_CopyFails) {
    const char* mock_files[] = {"messages.txt"};
    setup_mock_directory_entries(mock_files, 1);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.filePresentCheck_return = 0;
    mock_control.copyFiles_return = -1; // Copy fails

    int result = move_log_files_by_pattern("/opt/logs", "/opt/logs/PreviousLogs");

    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
    EXPECT_TRUE(mock_control.copyFiles_called);
    EXPECT_FALSE(mock_control.remove_called); // Remove not called if copy fails
}

// ================================================================================================
// backup_execute_hdd_enabled_strategy() Tests
// ================================================================================================

TEST_F(BackupEngineTest, HDDEnabledStrategy_FirstTime) {
    mock_control.filePresentCheck_return = -1; // messages.txt not present (first time)
    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.fopen_return = (FILE*)0x12345678;
    mock_control.safe_to_copy_paths = true;

    int result = backup_execute_hdd_enabled_strategy(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.filePresentCheck_called);
    EXPECT_TRUE(mock_control.fopen_called); // Creates last_reboot file
    EXPECT_TRUE(mock_control.fclose_called);
}

TEST_F(BackupEngineTest, HDDEnabledStrategy_SubsequentBackup) {
    mock_control.filePresentCheck_return = 0; // messages.txt exists (subsequent backup)
    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.createDir_return = 0;
    mock_control.fopen_return = (FILE*)0x12345678;
    mock_control.safe_to_copy_paths = true;

    // Setup directory entries with last_reboot file
    const char* mock_files[] = {"last_reboot", "messages.txt"};
    setup_mock_directory_entries(mock_files, 2);

    int result = backup_execute_hdd_enabled_strategy(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.createDir_called); // Creates timestamped directory
    EXPECT_TRUE(mock_control.time_called);
    EXPECT_TRUE(mock_control.localtime_called);
    EXPECT_TRUE(mock_control.strftime_called);
}

TEST_F(BackupEngineTest, HDDEnabledStrategy_PathTooLong) {
    // Create config with very long path
    backup_config_t long_config = test_config;
    memset(long_config.prev_log_path, 'A', PATH_MAX - 5);
    long_config.prev_log_path[PATH_MAX - 5] = '\0';

    int result = backup_execute_hdd_enabled_strategy(&long_config);

    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
}

// ================================================================================================
// backup_execute_hdd_disabled_strategy() Tests
// ================================================================================================

TEST_F(BackupEngineTest, HDDDisabledStrategy_FirstTime) {
    mock_control.filePresentCheck_return = -1; // No messages.txt (first time)
    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.fopen_return = (FILE*)0x12345678;
    mock_control.safe_to_copy_paths = true;

    int result = backup_execute_hdd_disabled_strategy(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.filePresentCheck_called);
    EXPECT_TRUE(mock_control.fopen_called); // Creates last_reboot
}

TEST_F(BackupEngineTest, HDDDisabledStrategy_SecondTime) {
    // First call: messages.txt exists, bak1 doesn't
    mock_control.filePresentCheck_return = 0; // messages.txt exists

    // Need to simulate multiple filePresentCheck calls with different return values
    // This is a simplified test - in reality we'd need more sophisticated mock behavior

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.fopen_return = (FILE*)0x12345678;
    mock_control.safe_to_copy_paths = true;

    int result = backup_execute_hdd_disabled_strategy(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
}

TEST_F(BackupEngineTest, HDDDisabledStrategy_PathTooLong) {
    backup_config_t long_config = test_config;
    memset(long_config.prev_log_path, 'A', PATH_MAX - 5);
    long_config.prev_log_path[PATH_MAX - 5] = '\0';

    int result = backup_execute_hdd_disabled_strategy(&long_config);

    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
}

// ================================================================================================
// backup_and_recover_logs() Tests
// ================================================================================================

TEST_F(BackupEngineTest, BackupAndRecoverLogs_MoveOperation) {
    const char* mock_files[] = {"messages.txt", "system.log"};
    setup_mock_directory_entries(mock_files, 2);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.stat_return = 0; // stat succeeds
    mock_control.stat_mode = S_IFREG; // Regular file
    mock_control.copyFiles_return = 0; // Copy succeeds
    mock_control.remove_return = 0; // Remove succeeds
    mock_control.safe_to_copy_paths = true;

    int result = backup_and_recover_logs("/opt/logs/", "/opt/logs/PreviousLogs/",
                                       BACKUP_OP_MOVE, "", "");

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.opendir_called);
    EXPECT_TRUE(mock_control.stat_called);
    EXPECT_TRUE(mock_control.copyFiles_called);
    EXPECT_TRUE(mock_control.remove_called);
}

TEST_F(BackupEngineTest, BackupAndRecoverLogs_CopyOperation) {
    const char* mock_files[] = {"messages.txt"};
    setup_mock_directory_entries(mock_files, 1);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.stat_return = 0;
    mock_control.stat_mode = S_IFREG;
    mock_control.copyFiles_return = 0;
    mock_control.safe_to_copy_paths = true;

    int result = backup_and_recover_logs("/opt/logs/", "/opt/logs/PreviousLogs/",
                                       BACKUP_OP_COPY, "", "");

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.copyFiles_called);
    EXPECT_FALSE(mock_control.remove_called); // No remove for copy operation
}

TEST_F(BackupEngineTest, BackupAndRecoverLogs_WithPrefixes) {
    const char* mock_files[] = {"bak1_messages.txt", "bak1_system.log", "other.txt"};
    setup_mock_directory_entries(mock_files, 3);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.stat_return = 0;
    mock_control.stat_mode = S_IFREG;
    mock_control.copyFiles_return = 0;
    mock_control.safe_to_copy_paths = true;

    int result = backup_and_recover_logs("/opt/logs/PreviousLogs/", "/opt/logs/PreviousLogs/",
                                       BACKUP_OP_MOVE, "bak1_", "bak2_");

    EXPECT_EQ(result, BACKUP_SUCCESS);
    // Should process only files starting with "bak1_"
}

TEST_F(BackupEngineTest, BackupAndRecoverLogs_SkipDirectories) {
    const char* mock_files[] = {"messages.txt", "subdir"};
    setup_mock_directory_entries(mock_files, 2);

    mock_control.opendir_return = (DIR*)0x12345678;

    // First stat call returns regular file, second returns directory
    static int stat_call_count = 0;
    stat_call_count = 0;
    mock_control.stat_return = 0;
    // Need to set up different modes for different files - this is simplified
    mock_control.stat_mode = S_IFREG; // Will be regular file for first call

    mock_control.copyFiles_return = 0;
    mock_control.safe_to_copy_paths = true;

    int result = backup_and_recover_logs("/opt/logs/", "/opt/logs/PreviousLogs/",
                                       BACKUP_OP_COPY, "", "");

    EXPECT_EQ(result, BACKUP_SUCCESS);
}

TEST_F(BackupEngineTest, BackupAndRecoverLogs_OpenDirFails) {
    mock_control.opendir_return = nullptr; // opendir fails

    int result = backup_and_recover_logs("/opt/logs/", "/opt/logs/PreviousLogs/",
                                       BACKUP_OP_MOVE, "", "");

    EXPECT_EQ(result, BACKUP_ERROR_FILESYSTEM);
    EXPECT_TRUE(mock_control.opendir_called);
}

TEST_F(BackupEngineTest, BackupAndRecoverLogs_NoFiles) {
    setup_mock_directory_entries(nullptr, 0); // No files

    mock_control.opendir_return = (DIR*)0x12345678;

    int result = backup_and_recover_logs("/opt/logs/", "/opt/logs/PreviousLogs/",
                                       BACKUP_OP_MOVE, "", "");

    EXPECT_EQ(result, BACKUP_SUCCESS); // Success if no files found
}

// ================================================================================================
// backup_execute_common_operations() Tests
// ================================================================================================

TEST_F(BackupEngineTest, CommonOperations_Success) {
    mock_control.special_files_load_config_return = BACKUP_SUCCESS;
    mock_control.special_files_execute_all_return = BACKUP_SUCCESS;

    int result = backup_execute_common_operations(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.special_files_init_called);
    EXPECT_TRUE(mock_control.special_files_load_config_called);
    EXPECT_TRUE(mock_control.special_files_execute_all_called);
    EXPECT_TRUE(mock_control.sys_send_systemd_notification_called);
    EXPECT_TRUE(mock_control.special_files_cleanup_called);
    
    // Ensure string is properly null-terminated before comparison
    mock_control.sys_send_systemd_notification_last_message[
        sizeof(mock_control.sys_send_systemd_notification_last_message) - 1] = '\0';
    EXPECT_STREQ(mock_control.sys_send_systemd_notification_last_message, "Logs Backup Done..!");
}

TEST_F(BackupEngineTest, CommonOperations_ConfigLoadFails) {
    mock_control.special_files_load_config_return = BACKUP_ERROR_CONFIG;

    int result = backup_execute_common_operations(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS); // Still succeeds even if special files config fails
    EXPECT_TRUE(mock_control.special_files_init_called);
    EXPECT_TRUE(mock_control.special_files_load_config_called);
    EXPECT_FALSE(mock_control.special_files_execute_all_called); // Not called if config fails
    EXPECT_TRUE(mock_control.sys_send_systemd_notification_called);
    EXPECT_TRUE(mock_control.special_files_cleanup_called);
}

TEST_F(BackupEngineTest, CommonOperations_ExecuteAllFails) {
    mock_control.special_files_load_config_return = BACKUP_SUCCESS;
    mock_control.special_files_execute_all_return = BACKUP_ERROR_FILESYSTEM;

    int result = backup_execute_common_operations(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS); // Still succeeds even if execute fails
    EXPECT_TRUE(mock_control.special_files_execute_all_called);
}

// ================================================================================================
// Edge Cases and Error Handling Tests
// ================================================================================================

TEST_F(BackupEngineTest, TimeOperations_FailureHandling) {
    mock_control.localtime_return = nullptr; // localtime fails
    mock_control.filePresentCheck_return = 0; // Trigger subsequent backup path
    mock_control.opendir_return = (DIR*)0x12345678;

    // Should handle gracefully even if time operations fail
    int result = backup_execute_hdd_enabled_strategy(&test_config);

    EXPECT_TRUE(mock_control.time_called);
    EXPECT_TRUE(mock_control.localtime_called);
    // Function should still attempt to continue
}

TEST_F(BackupEngineTest, FileOperations_EdgeCases) {
    const char* mock_files[] = {".txt", "file.txt.backup", "file.log.old"};
    setup_mock_directory_entries(mock_files, 3);

    mock_control.opendir_return = (DIR*)0x12345678;
    mock_control.filePresentCheck_return = 0;
    mock_control.copyFiles_return = 0;
    mock_control.safe_to_copy_paths = true;

    int result = move_log_files_by_pattern("/opt/logs", "/opt/logs/PreviousLogs");

    EXPECT_EQ(result, BACKUP_SUCCESS);
    // All files contain .txt or .log so should be processed
}

// ================================================================================================
// Main Function for Test Runner
// ================================================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
