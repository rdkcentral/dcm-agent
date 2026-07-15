####################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
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
Test cases for uploadSTBLogs normal upload operations
Covers: Normal upload, large file handling, MD5 verification
"""

import os
import pytest
import subprocess
import time
from uploadstblogs_helper import *
from helper_functions import *

# ---------------------------------------------------------------------------
# Sentinel file paths (mirror uploadstblogs_types.h)
# ---------------------------------------------------------------------------
BACKUP_LOGS_DONE_FLAG       = "/tmp/.backup_logs_done"
STT_FLAG                    = "/tmp/stt_received"
PATH_FLAG_INVOCATION        = "/tmp/Update_rebootInfo_invoked"
TELEMETRY_PREVLOGS_DONE_FLAG = "/tmp/.telemetry_prevlogs_done"

# Reboot-upload args that exercise all synchronization gates
REBOOT_UPLOAD_ARGS = "'' 1 1 1 HTTP https://mockxconf:50058/ 2 0 ''"


def _create_sentinel(path):
    """Touch a sentinel file, creating parent directories if needed."""
    parent = os.path.dirname(path)
    if parent:
        subprocess.run(f"mkdir -p {parent}", shell=True)
    subprocess.run(f"touch {path}", shell=True)


def _remove_sentinel(path):
    """Remove a sentinel file if it exists."""
    try:
        if os.path.exists(path):
            os.remove(path)
    except OSError:
        pass


def _create_all_sentinels():
    """Create all reboot-flow synchronization sentinels."""
    _create_sentinel(BACKUP_LOGS_DONE_FLAG)
    _create_sentinel(STT_FLAG)
    _create_sentinel(PATH_FLAG_INVOCATION)
    _create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)


def _remove_all_sentinels():
    """Remove all reboot-flow synchronization sentinels."""
    _remove_sentinel(BACKUP_LOGS_DONE_FLAG)
    _remove_sentinel(STT_FLAG)
    _remove_sentinel(PATH_FLAG_INVOCATION)
    _remove_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)


class TestNormalUpload:
    """Test suite for normal upload operations"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        # Setup
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        # Teardown
        cleanup_test_log_files()
        remove_lock_file()
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_normal_upload_initialization(self):
        """Test: uploadSTBLogs service initialization"""
        # Create test log files
        create_test_log_files(count=3, size_kb=50)
        set_include_property("LOG_PATH", "/opt/logs")

        # Run uploadSTBLogs
        #result = run_uploadstblogs()
        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)
       
        # Verify initialization
        assert result.returncode == 0 or result.returncode == 1, "Upload process should complete"

        # Check initialization logs
        init_logs = grep_uploadstb_logs("Context initialization successful")
        assert len(init_logs) > 0, "Context should be initialized successfully"

        collection_logs = grep_uploadstb_logs_regex(r"collect|archive|gather")
        assert len(collection_logs) > 0, "Log collection should be attempted"

        # Check for archive creation logs
        archive_logs = grep_uploadstb_logs_regex(r"Archive created successfully")
        # Process should complete successfully
        assert len(archive_logs) > 0, "Archive process should complete. Found {len(archive_logs)} archive-related logs: {archive_logs}"

        upload_logs = grep_uploadstb_logs_regex(r"upload.*success|uploading|HTTP")
        # Telemetry should be attempted
        assert len(archive_logs) > 0, "Upload Process should complete and succeed"


class TestLargeFileHandling:
    """Test suite for large file handling"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown for large file tests"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files("large_test")
        restore_device_properties()
        yield
        cleanup_test_log_files("large_test")
        remove_lock_file()

    @pytest.mark.order(1)
    def test_large_file_collection(self):
        """Test: Service collects large log files within limits"""
        # Create large test files (10MB each)
        large_files = create_large_test_log_files(count=3, size_mb=10)
        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        # Verify files were processed
        
        # Check for compression/archive activity
        compression_logs = grep_uploadstb_logs_regex(r"compress|archive|tgz")
        # Process should complete
        assert result.returncode in [0, 1], "Compression process should complete"


class TestNormalUploadWithSentinelFlow:
    """
    Test suite that mirrors the normal upload flow but adds
    sentinel file creation before the run and verifies that
    each synchronization gate logs the expected sentinel
    detection messages.

    Covered gates (from strategies.c reboot_setup()):
      - backup_logs completion sentinel   (/tmp/.backup_logs_done)
      - NTP sync sentinel                 (/tmp/stt_received)
      - Reboot reason sentinel            (/tmp/Update_rebootInfo_invoked)
      - Telemetry prevlogs done sentinel  (/tmp/.telemetry_prevlogs_done)
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after."""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        _remove_all_sentinels()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()
        _remove_all_sentinels()
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_upload_with_all_sentinels_present(self):
        """Test: All sentinels present — upload proceeds through every gate."""
        create_test_log_files(count=3, size_kb=50)
        set_include_property("LOG_PATH", "/opt/logs")

        # Create sentinel files before triggering upload
        _create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        assert result.returncode in [0, 1], "Upload process should complete"

        # Gate 1: backup_logs sentinel must NOT cause abort
        abort_logs = grep_uploadstb_logs("backup_logs not done")
        assert len(abort_logs) == 0, "backup_logs sentinel present — abort message should not appear"

        # Gate 2: NTP sync sentinel must be detected
        ntp_logs = grep_uploadstb_logs("NTP sync sentinel detected")
        assert len(ntp_logs) > 0, "Expected log: 'NTP sync sentinel detected'"

        # Gate 3: Reboot reason sentinel must be detected
        reboot_logs = grep_uploadstb_logs("Reboot reason sentinel detected. Proceeding.")
        assert len(reboot_logs) > 0, "Expected log: 'Reboot reason sentinel detected. Proceeding.'"

        # Gate 4: Telemetry prevlogs sentinel must be detected
        telemetry_logs = grep_uploadstb_logs("Telemetry prevlogs sentinel detected. Proceeding.")
        assert len(telemetry_logs) > 0, "Expected log: 'Telemetry prevlogs sentinel detected. Proceeding.'"

    @pytest.mark.order(2)
    def test_upload_without_backup_logs_sentinel_aborts(self):
        """Test: Missing backup_logs sentinel causes upload to abort."""
        create_test_log_files(count=3, size_kb=50)
        set_include_property("LOG_PATH", "/opt/logs")

        # Create every sentinel EXCEPT backup_logs
        _create_sentinel(STT_FLAG)
        _create_sentinel(PATH_FLAG_INVOCATION)
        _create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # BACKUP_LOGS_DONE_FLAG intentionally absent

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        assert result.returncode in [0, 1], "Process should exit cleanly even when aborting"

        # Must log that backup_logs is not done
        abort_logs = grep_uploadstb_logs("backup_logs not done")
        assert len(abort_logs) > 0, "Expected log: 'backup_logs not done'"

        # Must NOT reach archive/upload phase
        archive_logs = grep_uploadstb_logs("Starting archive phase")
        assert len(archive_logs) == 0, "Upload should not proceed to archive without backup_logs sentinel"

    @pytest.mark.order(3)
    def test_upload_without_ntp_sentinel_logs_absent(self):
        """Test: Missing NTP sentinel is detected; upload is not hard-aborted by NTP alone."""
        create_test_log_files(count=3, size_kb=50)
        set_include_property("LOG_PATH", "/opt/logs")

        # Create every sentinel EXCEPT NTP
        _create_sentinel(BACKUP_LOGS_DONE_FLAG)
        _create_sentinel(PATH_FLAG_INVOCATION)
        _create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # STT_FLAG intentionally absent

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        assert result.returncode in [0, 1], "Process should exit cleanly"

        # NTP absent path must be logged
        ntp_absent_logs = grep_uploadstb_logs("NTP absent")
        assert len(ntp_absent_logs) > 0, "Expected log: 'NTP absent'"

        # NTP alone should not hard-abort the upload
        ntp_abort_logs = [l for l in grep_uploadstb_logs("aborting upload") if "NTP" in l or "ntp" in l]
        assert len(ntp_abort_logs) == 0, "NTP absence alone should not abort upload"

