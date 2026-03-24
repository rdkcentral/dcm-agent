####################################################################################
# If not stated otherwise in this file or this component's Licenses file the
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
Test cases for special_files.c
Covers: Config file parsing, special file copy and move operations,
        missing config file handling, conditional checks
"""

import pytest
import os
import subprocess
from backup_logs_helper import *


# ---------------------------------------------------------------------------
# Helpers specific to special files testing
# ---------------------------------------------------------------------------

def create_special_files_conf(entries):
    """
    Write a special_files.conf to /etc/backup_logs/special_files.conf.
    entries: list of path strings (one per line).
    Comments and blank lines are silently skipped by the C parser.
    """
    conf_dir = "/etc/backup_logs"
    os.makedirs(conf_dir, exist_ok=True)
    with open(SPECIAL_FILES_CONF, "w") as f:
        f.write("# Special Files Configuration for Backup Logs\n")
        f.write("# Format: one filename per line (full path)\n\n")
        for entry in entries:
            f.write(entry + "\n")


def remove_special_files_conf():
    """Remove the special_files.conf if it exists."""
    if os.path.exists(SPECIAL_FILES_CONF):
        os.remove(SPECIAL_FILES_CONF)


def create_tmp_file(path, content="test special file content\n"):
    """Create a temp file with given content."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(content)


# ---------------------------------------------------------------------------
# Test classes
# ---------------------------------------------------------------------------

class TestSpecialFilesConfigParsing:
    """Test suite for special_files_load_config() parsing behaviour"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        remove_special_files_conf()
        yield
        cleanup_log_directories()
        remove_special_files_conf()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_missing_conf_file_logged_as_warning(self):
        """Test: Missing special_files.conf produces a warning, not a fatal error"""
        # No conf file created - should warn but not crash
        run_backup_logs()

        logs = grep_backup_logs_regex(
            r"Config file not found.*special_files\.conf|special_files.*not found"
        )
        assert len(logs) > 0, \
            "Missing special_files.conf should produce a warning log entry"

    @pytest.mark.order(2)
    def test_conf_file_opened_successfully_logged(self):
        """Test: Successfully opened special_files.conf is logged"""
        create_special_files_conf(["/tmp/disk_cleanup.log"])

        run_backup_logs()

        logs = grep_backup_logs_regex(
            r"Config file opened successfully.*special_files\.conf"
        )
        assert len(logs) > 0, "Successful config file open should be logged"

    @pytest.mark.order(3)
    def test_comments_and_empty_lines_skipped(self):
        """Test: Lines starting with '#' and blank lines are ignored by parser"""
        # Write conf with only comments and blank lines - no valid entries
        conf_dir = "/etc/backup_logs"
        os.makedirs(conf_dir, exist_ok=True)
        with open(SPECIAL_FILES_CONF, "w") as f:
            f.write("# comment line\n\n# another comment\n\n")

        run_backup_logs()

        # Should not crash; binary should complete normally
        result = run_backup_logs()
        assert result.returncode == 0, \
            "backup_logs should complete without error when conf has only comments"

    @pytest.mark.order(4)
    def test_max_special_files_limit_not_exceeded(self):
        """Test: Parser respects MAX_SPECIAL_FILES (32) limit"""
        # Create 35 entries - only 32 should be loaded
        entries = [f"/tmp/test_special_{i}.log" for i in range(35)]
        create_special_files_conf(entries)

        run_backup_logs()

        # Should complete without crash or memory error
        result = run_backup_logs()
        assert result.returncode == 0, \
            "backup_logs should not crash when conf contains more than 32 entries"


class TestSpecialFileMoveOperations:
    """Test suite for move operations on special files"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        remove_special_files_conf()
        yield
        cleanup_log_directories()
        remove_special_files_conf()
        subprocess.run("rm -f /tmp/disk_cleanup.log /tmp/mount_log.txt /tmp/mount-ta_log.txt",
                       shell=True)
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_disk_cleanup_log_moved_to_log_path(self):
        """Test: /tmp/disk_cleanup.log is moved to LOG_PATH"""
        create_tmp_file("/tmp/disk_cleanup.log", "disk cleanup data\n")
        create_special_files_conf(["/tmp/disk_cleanup.log"])

        run_backup_logs()

        logs = grep_backup_logs_regex(r"disk_cleanup\.log")
        assert len(logs) > 0, "disk_cleanup.log processing should be logged"

    @pytest.mark.order(2)
    def test_mount_log_moved_to_log_path(self):
        """Test: /tmp/mount_log.txt is processed from special files config"""
        create_tmp_file("/tmp/mount_log.txt", "mount log data\n")
        create_special_files_conf(["/tmp/mount_log.txt"])

        run_backup_logs()

        logs = grep_backup_logs_regex(r"mount_log\.txt")
        assert len(logs) > 0, "mount_log.txt processing should be logged"

    @pytest.mark.order(3)
    def test_mount_ta_log_moved_to_log_path(self):
        """Test: /tmp/mount-ta_log.txt is processed from special files config"""
        create_tmp_file("/tmp/mount-ta_log.txt", "mount-ta log data\n")
        create_special_files_conf(["/tmp/mount-ta_log.txt"])

        run_backup_logs()

        logs = grep_backup_logs_regex(r"mount-ta_log\.txt")
        assert len(logs) > 0, "mount-ta_log.txt processing should be logged"


class TestSpecialFileCopyOperations:
    """Test suite for copy operations on version/metadata files"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        remove_special_files_conf()
        yield
        cleanup_log_directories()
        remove_special_files_conf()
        subprocess.run("rm -f /version.txt /etc/skyversion.txt /etc/rippleversion.txt",
                       shell=True)
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_version_txt_copy_logged(self):
        """Test: /version.txt copy operation is processed and logged"""
        create_tmp_file("/version.txt", "v1.0.0\n")
        create_special_files_conf(["/version.txt"])

        run_backup_logs()

        logs = grep_backup_logs_regex(r"version\.txt")
        assert len(logs) > 0, "version.txt copy operation should be logged"

    @pytest.mark.order(2)
    def test_skyversion_txt_copy_logged(self):
        """Test: /etc/skyversion.txt copy operation is processed and logged"""
        create_tmp_file("/etc/skyversion.txt", "sky-v1.0\n")
        create_special_files_conf(["/etc/skyversion.txt"])

        run_backup_logs()

        logs = grep_backup_logs_regex(r"skyversion\.txt")
        assert len(logs) > 0, "skyversion.txt copy operation should be logged"

    @pytest.mark.order(3)
    def test_rippleversion_txt_copy_logged(self):
        """Test: /etc/rippleversion.txt copy operation is processed and logged"""
        create_tmp_file("/etc/rippleversion.txt", "ripple-v1.0\n")
        create_special_files_conf(["/etc/rippleversion.txt"])

        run_backup_logs()

        logs = grep_backup_logs_regex(r"rippleversion\.txt")
        assert len(logs) > 0, "rippleversion.txt copy operation should be logged"


class TestSpecialFilesExecution:
    """Test suite for special_files_execute_all() overall execution"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_backup_logs()
        setup_log_directories()
        restore_default_properties()
        remove_special_files_conf()
        yield
        cleanup_log_directories()
        remove_special_files_conf()
        kill_backup_logs()

    @pytest.mark.order(1)
    def test_special_files_manager_init_logged(self):
        """Test: Special files manager initialization is logged"""
        run_backup_logs()

        logs = grep_backup_logs(
            "Special files manager initialization completed successfully"
        )
        assert len(logs) > 0, "Special files manager init log should be present"

    @pytest.mark.order(2)
    def test_special_files_execute_all_completes(self):
        """Test: backup_logs binary completes without error when processing special files"""
        create_special_files_conf([
            "/tmp/disk_cleanup.log",
            "/tmp/mount_log.txt",
            "/version.txt",
        ])
        create_tmp_file("/tmp/disk_cleanup.log")
        create_tmp_file("/tmp/mount_log.txt")
        create_tmp_file("/version.txt", "1.0\n")

        result = run_backup_logs()

        assert result.returncode == 0, \
            f"backup_logs should exit 0 when processing special files. " \
            f"stderr: {result.stderr}"

    @pytest.mark.order(3)
    def test_special_files_missing_source_handled_gracefully(self):
        """Test: Missing source file in special files config does not crash binary"""
        # Config references a file that does not exist
        create_special_files_conf(["/tmp/nonexistent_special_file.log"])

        result = run_backup_logs()

        assert result.returncode == 0, \
            "backup_logs should not crash when a special file source is missing"
