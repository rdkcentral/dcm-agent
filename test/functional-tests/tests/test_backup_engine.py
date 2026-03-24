####################################################################################
# If not stated otherwise in this file or this component's Licenses file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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
Test cases for backup_engine.c
Covers: HDD-enabled strategy, HDD-disabled rotation strategy,
        file pattern matching, backup_logs.log exclusion
"""

import pytest
import re
import os
import time
from backup_logs_helper import *


def pytest_configure(config):
    config.addinivalue_line("markers", "order: set execution order of tests within a class")


class TestHDDEnabledStrategy:
    """Test suite for HDD-enabled backup strategy"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        set_device_property("HDD_ENABLED", "true")
        remove_dir_contents(PREV_LOG_PATH)
        remove_dir_contents(PREV_LOG_BACKUP_PATH)
        yield
        cleanup_log_directories()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_hdd_enabled_strategy_logged(self):
        """Test: HDD-enabled strategy execution is logged"""
        create_test_log_files()

        result = run_backup_logs()

        logs = grep_backup_logs("Executing HDD-enabled backup strategy")
        assert len(logs) > 0, "HDD-enabled strategy log message should be present"

    @pytest.mark.order(2)
    def test_first_backup_moves_files_to_prev_log(self):
        """Test: First-time backup moves log files directly to PreviousLogs"""
        create_test_log_files()
        create_messages_txt()
        # Ensure no messages.txt in PreviousLogs (first backup condition)
        msgs = os.path.join(PREV_LOG_PATH, "messages.txt")
        if os.path.exists(msgs):
            os.remove(msgs)

        run_backup_logs()

        files_in_prev = list_files(PREV_LOG_PATH)
        assert len(files_in_prev) > 0, "Files should be moved to PreviousLogs on first backup"

    @pytest.mark.order(3)
    def test_first_backup_creates_last_reboot_marker(self):
        """Test: First-time backup creates last_reboot marker in PreviousLogs"""
        create_test_log_files()

        run_backup_logs()

        assert file_exists_in(PREV_LOG_PATH, "last_reboot"), \
            "last_reboot marker must be created in PreviousLogs after first backup"

    @pytest.mark.order(4)
    def test_backup_logs_log_not_moved(self):
        """Test: Active backup_logs.log file is never moved to PreviousLogs"""
        create_test_log_files()

        run_backup_logs()

        assert not file_exists_in(PREV_LOG_PATH, "backup_logs.log"), \
            "backup_logs.log must not be moved to PreviousLogs"

    @pytest.mark.order(5)
    def test_log_files_removed_from_source(self):
        """Test: Matched log files are removed from LOG_PATH after HDD-enabled backup"""
        create_test_log_files()
        create_bootlog()

        run_backup_logs()

        remaining = [f for f in list_files(LOG_PATH)
                     if f.endswith(".log") and f != "backup_logs.log"]
        assert len(remaining) == 0, \
            f"Matched log files should be removed from LOG_PATH; remaining: {remaining}"


class TestHDDDisabledStrategy:
    """Test suite for HDD-disabled rotation strategy (4-level rotation)"""

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
    def test_hdd_disabled_strategy_logged(self):
        """Test: HDD-disabled rotation strategy execution is logged"""
        create_test_log_files()

        result = run_backup_logs()

        logs = grep_backup_logs("Executing HDD-disabled backup strategy with rotation")
        assert len(logs) > 0, "HDD-disabled strategy log message should be present"

    @pytest.mark.order(2)
    def test_first_backup_moves_files_no_prefix(self):
        """Test: State 0 - no messages.txt in PreviousLogs - files moved without prefix"""
        create_messages_txt(LOG_PATH)
        create_test_log_files()
        # Ensure no messages.txt in PreviousLogs
        msgs = os.path.join(PREV_LOG_PATH, "messages.txt")
        if os.path.exists(msgs):
            os.remove(msgs)

        run_backup_logs()

        assert file_exists_in(PREV_LOG_PATH, "messages.txt"), \
            "messages.txt should be moved to PreviousLogs in state 0 (no prefix)"
        logs = grep_backup_logs("First time HDD-disabled backup")
        assert len(logs) > 0, "First-time HDD-disabled log should be present"

    @pytest.mark.order(3)
    def test_second_backup_uses_bak1_prefix(self):
        """Test: State 1 - messages.txt exists but no bak1_ - files get bak1_ prefix"""
        create_messages_txt(PREV_LOG_PATH)   # sentinel: prior backup exists
        create_messages_txt(LOG_PATH)
        create_test_log_files()

        run_backup_logs()

        logs = grep_backup_logs("Moving logs to bak1_ prefix")
        assert len(logs) > 0, "bak1_ rotation log should be present"

    @pytest.mark.order(4)
    def test_third_backup_uses_bak2_prefix(self):
        """Test: State 2 - bak1_ exists but no bak2_ - files get bak2_ prefix"""
        create_messages_txt(PREV_LOG_PATH)
        open(os.path.join(PREV_LOG_PATH, "bak1_messages.txt"), "w").close()
        create_messages_txt(LOG_PATH)

        run_backup_logs()

        logs = grep_backup_logs("Moving logs to bak2_ prefix")
        assert len(logs) > 0, "bak2_ rotation log should be present"

    @pytest.mark.order(5)
    def test_fourth_backup_uses_bak3_prefix(self):
        """Test: State 3 - bak2_ exists but no bak3_ - files get bak3_ prefix"""
        create_messages_txt(PREV_LOG_PATH)
        open(os.path.join(PREV_LOG_PATH, "bak1_messages.txt"), "w").close()
        open(os.path.join(PREV_LOG_PATH, "bak2_messages.txt"), "w").close()
        create_messages_txt(LOG_PATH)

        run_backup_logs()

        logs = grep_backup_logs("Moving logs to bak3_ prefix")
        assert len(logs) > 0, "bak3_ rotation log should be present"

    @pytest.mark.order(6)
    def test_full_rotation_cycle_logged(self):
        """Test: State 4 - all slots full - full rotation cycle is logged"""
        for name in ["messages.txt", "bak1_messages.txt",
                     "bak2_messages.txt", "bak3_messages.txt"]:
            open(os.path.join(PREV_LOG_PATH, name), "w").close()
        create_messages_txt(LOG_PATH)

        run_backup_logs()

        logs = grep_backup_logs("Performing full rotation cycle")
        assert len(logs) > 0, "Full rotation cycle log should be present"

    @pytest.mark.order(7)
    def test_last_reboot_marker_created(self):
        """Test: last_reboot marker created in PreviousLogs after HDD-disabled backup"""
        create_test_log_files()

        run_backup_logs()

        assert file_exists_in(PREV_LOG_PATH, "last_reboot"), \
            "last_reboot marker must be created in PreviousLogs"


class TestFilePatternMatching:
    """Test suite for file pattern matching in move_log_files_by_pattern"""

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
    def test_txt_files_are_moved(self):
        """Test: Files containing .txt in name are moved to PreviousLogs"""
        open(os.path.join(LOG_PATH, "test_app.txt"), "w").close()

        run_backup_logs()

        files = list_files(PREV_LOG_PATH)
        txt_files = [f for f in files if ".txt" in f and not f.startswith("backup_logs")]
        assert len(txt_files) > 0, "*.txt files should be moved to PreviousLogs"

    @pytest.mark.order(2)
    def test_log_files_are_moved(self):
        """Test: Files containing .log in name are moved to PreviousLogs"""
        open(os.path.join(LOG_PATH, "test_app.log"), "w").close()

        run_backup_logs()

        files = list_files(PREV_LOG_PATH)
        log_files = [f for f in files if ".log" in f and f != "backup_logs.log"]
        assert len(log_files) > 0, "*.log files should be moved to PreviousLogs"

    @pytest.mark.order(3)
    def test_bootlog_is_moved(self):
        """Test: 'bootlog' file (exact name) is matched and moved"""
        create_bootlog(LOG_PATH)

        run_backup_logs()

        assert file_exists_in(PREV_LOG_PATH, "bootlog"), \
            "'bootlog' file should be moved to PreviousLogs"
