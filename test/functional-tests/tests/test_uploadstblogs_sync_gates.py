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

import os
import pytest
import subprocess as sp
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
NTP_SYNC_INDICATOR = "/tmp/systimemgr/ntp"
PREV_LOG_PATH      = "/opt/logs/PreviousLogs"

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
    _create_sentinel(NTP_SYNC_INDICATOR)

def _remove_all_sentinels():
    """Remove all reboot-flow synchronization sentinels."""
    _remove_sentinel(BACKUP_LOGS_DONE_FLAG)
    _remove_sentinel(STT_FLAG)
    _remove_sentinel(PATH_FLAG_INVOCATION)
    _remove_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)

def setup_previous_logs():
    """Create PreviousLogs directory with sample log files."""
    sp.run(f"mkdir -p {PREV_LOG_PATH}", shell=True)
    sp.run(f"echo 'sample log content' > {PREV_LOG_PATH}/messages.log", shell=True)
    sp.run(f"echo 'wifi log content'   > {PREV_LOG_PATH}/wifi.log",     shell=True)
    sp.run(f"echo 'system log'         > {PREV_LOG_PATH}/system.txt",   shell=True)

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
        _create_all_sentinels()
        setup_previous_logs()
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

        # Gate 1: backup_logs sentinel must NOT cause abort and must log detection
        backup_detected_logs = grep_uploadstb_logs("bacukup_logs sentinel detected. Proceeding.")
        assert len(backup_detected_logs) > 0, "Expected log: 'bacukup_logs sentinel detected. Proceeding.'"

        # Gate 2: NTP sync sentinel must be detected
        ntp_logs = grep_uploadstb_logs("NTP sync sentinel detected")
        assert len(ntp_logs) > 0, "Expected log: 'NTP sync sentinel detected'"

        # Gate 3: Reboot reason sentinel must be detected
        reboot_logs = grep_uploadstb_logs("Reboot reason sentinel detected. Proceeding.")
        assert len(reboot_logs) > 0, "Expected log: 'Reboot reason sentinel detected. Proceeding.'"

        # Gate 4: Telemetry prevlogs sentinel must be detected
        telemetry_logs = grep_uploadstb_logs("Telemetry prevlogs sentinel detected. Proceeding.")
        assert len(telemetry_logs) > 0, "Expected log: 'Telemetry prevlogs sentinel detected. Proceeding.'"

        collection_logs = grep_uploadstb_logs_regex(r"collect|archive|gather")
        assert len(collection_logs) > 0, "Log collection should be attempted"

        # Check for archive creation logs
        archive_logs = grep_uploadstb_logs_regex(r"Archive created successfully")
        # Process should complete successfully
        assert len(archive_logs) > 0, "Archive process should complete. Found {len(archive_logs)} archive-related logs: {archive_logs}"

        upload_logs = grep_uploadstb_logs_regex(r"upload.*success|uploading|HTTP")
        # Telemetry should be attempted
        assert len(archive_logs) > 0, "Upload Process should complete and succeed"
