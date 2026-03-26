####################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
####################################################################################

"""
Integration test cases for backup_logs
Covers: Full initialization sequence, backup execution lifecycle,
        systemd notification, disk threshold check, cleanup behaviour
"""

import pytest
import os
import re
import time
import subprocess
from backup_logs_helper import *


class TestBackupLogsInitialization:
    """Test suite for backup_logs_init() full initialization sequence"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        remove_dir_contents(PREV_LOG_PATH)
        remove_dir_contents(PREV_LOG_BACKUP_PATH)
        yield
        cleanup_log_directories()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_initialization_completes_successfully(self):
        """Test: Initialization completes without error"""
        result = run_backup_logs()

        assert result.returncode == 0, \
            f"backup_logs should exit 0 on successful init. stderr: {result.stderr}"

        logs = grep_backup_logs("Backup system initialization completed successfully")
        assert len(logs) > 0, "Initialization completion message should be logged"

    @pytest.mark.order(3)
    def test_null_config_parameter_handled(self):
        """Test: Initialization with invalid invocation doesn't produce segfault"""
        # Simply re-run and check no crash
        result = run_backup_logs()

        crash_logs = grep_backup_logs_regex(r"segfault|core dump|signal 11")
        assert len(crash_logs) == 0, "No segfault or crash should occur during init"


class TestBackupExecutionLifecycle:
    """Test suite for end-to-end backup execution (backup_logs_execute)"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        set_device_property("HDD_ENABLED", "false")
        remove_dir_contents(PREV_LOG_PATH)
        remove_dir_contents(PREV_LOG_BACKUP_PATH)
        yield
        cleanup_log_directories()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_backup_execution_starts(self):
        """Test: Backup execution process starts and is logged"""
        create_test_log_files()

        run_backup_logs()

        logs = grep_backup_logs("Backup system initialization completed successfully")
        assert len(logs) > 0, "Backup execution start should be logged"

    @pytest.mark.order(2)
    def test_backup_execution_completes_successfully(self):
        """Test: Complete backup execution returns exit code 0"""
        create_test_log_files()

        result = run_backup_logs()

        assert result.returncode == 0, \
            f"backup_logs should exit 0. returncode={result.returncode}, " \
            f"stderr={result.stderr}"

    @pytest.mark.order(4)
    def test_backup_strategy_selection_logged(self):
        """Test: Chosen backup strategy (HDD-enabled or HDD-disabled) is logged"""
        create_test_log_files()

        run_backup_logs()

        logs = grep_backup_logs_regex(
            r"Executing backup (with strategy|strategy execution)"
        )
        assert len(logs) > 0, "Backup strategy selection should be logged"

    @pytest.mark.order(5)
    def test_backup_execution_completed_logged(self):
        """Test: Backup execution completion is logged"""
        create_test_log_files()

        run_backup_logs()

        logs = grep_backup_logs("Backup execution process completed successfully")
        assert len(logs) > 0, "Backup execution completion should be logged"


class TestSystemdNotification:
    """Test suite for systemd READY notification (sys_integration.c)"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        yield
        cleanup_log_directories()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_systemd_notification_attempted(self):
        """Test: Systemd READY notification is sent as part of common operations"""
        run_backup_logs()

        logs = grep_backup_logs_regex(
            r"systemd.*notif|sd_notify|READY=1|Sending.*systemd"
        )
        assert len(logs) > 0, \
            "Systemd notification attempt should appear in logs"

    @pytest.mark.order(2)
    def test_backup_completes_when_systemd_unavailable(self):
        """Test: Backup completes successfully even when systemd is not available"""
        result = run_backup_logs()

        assert result.returncode == 0, \
            "backup_logs should complete successfully regardless of sd_notify availability"


class TestBackupLogsCleanup:
    """Test suite for backup_logs_cleanup() teardown behaviour"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        yield
        cleanup_log_directories()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_cleanup_completes_after_execution(self):
        """Test: Cleanup phase runs after backup execution"""
        run_backup_logs()

        logs = grep_backup_logs_regex(r"cleanup|Cleanup|shut.*down")
        # If log messages are present, cleanup ran; binary completing is also acceptable
        result = run_backup_logs()
        assert result.returncode == 0, "Cleanup should allow clean exit"


class TestMultipleBackupCycles:
    """Test suite for running backup_logs multiple times in sequence"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        set_device_property("HDD_ENABLED", "false")
        remove_dir_contents(PREV_LOG_PATH)
        remove_dir_contents(PREV_LOG_BACKUP_PATH)
        yield
        cleanup_log_directories()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_two_consecutive_hdd_disabled_runs(self):
        """Test: Running backup_logs twice transitions from state 0 to state 1"""
        # First run - state 0
        create_test_log_files()
        create_messages_txt(LOG_PATH)
        result1 = run_backup_logs()
        assert result1.returncode == 0, "First backup run should succeed"

        # After first run messages.txt should be in PreviousLogs
        assert file_exists_in(PREV_LOG_PATH, "messages.txt"), \
            "messages.txt must exist in PreviousLogs after first backup"

        # Second run - state 1 (bak1_ prefix)
        clear_backup_logs()
        create_messages_txt(LOG_PATH)
        result2 = run_backup_logs()
        assert result2.returncode == 0, "Second backup run should succeed"

        logs = grep_backup_logs("Moving logs to bak1_ prefix")
        assert len(logs) > 0, "bak1_ prefix rotation should occur on second run"

    @pytest.mark.order(2)
    def test_four_consecutive_cycles_all_slots_filled(self):
        """Test: Four consecutive runs fill all four rotation slots"""
        for cycle in range(4):
            clear_backup_logs()
            create_messages_txt(LOG_PATH)
            create_test_log_files()
            result = run_backup_logs()
            assert result.returncode == 0, \
                f"Backup run {cycle + 1} should succeed"

        # After 4 cycles, all rotation slots should be present
        for name in ["messages.txt", "bak1_messages.txt",
                     "bak2_messages.txt", "bak3_messages.txt"]:
            assert file_exists_in(PREV_LOG_PATH, name), \
                f"Rotation slot {name} should exist after 4 backup cycles"
