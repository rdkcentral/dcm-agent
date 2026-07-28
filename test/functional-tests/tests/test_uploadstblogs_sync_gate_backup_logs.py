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
BACKUP_LOGS_DONE_FLAG    = "/tmp/.backup_logs_done"
STT_FLAG                 = "/tmp/stt_received"
PATH_FLAG_INVOCATION     = "/tmp/Update_rebootInfo_invoked"
TELEMETRY_PREVLOGS_DONE_FLAG = "/tmp/.telemetry_prevlogs_done"

REBOOT_UPLOAD_ARGS = "'' 1 1 1 HTTP http://localhost:8080 2 0 ''"
PREV_LOG_PATH      = "/opt/logs/PreviousLogs"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def create_sentinel(path):
    """Touch a sentinel file, creating parent directories if needed."""
    dir_path = os.path.dirname(path)
    if dir_path:
        sp.run(f"mkdir -p {dir_path}", shell=True)
    sp.run(f"touch {path}", shell=True)


def remove_sentinel(path):
    """Remove a sentinel file if it exists."""
    try:
        if os.path.exists(path):
            os.remove(path)
    except OSError:
        pass


def create_all_sentinels():
    """Create all reboot-flow synchronization sentinels."""
    create_sentinel(BACKUP_LOGS_DONE_FLAG)
    create_sentinel(STT_FLAG)
    create_sentinel(PATH_FLAG_INVOCATION)
    create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)


def remove_all_sentinels():
    """Remove all reboot-flow synchronization sentinels."""
    for path in (BACKUP_LOGS_DONE_FLAG, STT_FLAG,
                 PATH_FLAG_INVOCATION, TELEMETRY_PREVLOGS_DONE_FLAG):
        remove_sentinel(path)


def setup_previous_logs():
    """Create PreviousLogs directory with sample log files."""
    sp.run(f"mkdir -p {PREV_LOG_PATH}", shell=True)
    sp.run(f"echo 'sample log content' > {PREV_LOG_PATH}/messages.log", shell=True)
    sp.run(f"echo 'wifi log content'   > {PREV_LOG_PATH}/wifi.log",     shell=True)
    sp.run(f"echo 'system log'         > {PREV_LOG_PATH}/system.txt",   shell=True)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestBackupLogsSyncGate:
    """
    REQ-SYNC-001: backup_logs completion sentinel gate.

    Covered cases:
      1. Sentinel present  → upload proceeds, detection log emitted.
      2. Sentinel absent   → upload aborts, warning log emitted.
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        clear_uploadstb_logs()
        remove_lock_file()
        remove_all_sentinels()
        setup_previous_logs()
        restore_device_properties()
        yield
        remove_all_sentinels()
        remove_lock_file()
        kill_uploadstblogs()
        sp.run(f"rm -rf {PREV_LOG_PATH}", shell=True)

    @pytest.mark.order(1)
    def test_backup_logs_sentinel_present_allows_upload(self):
        """Test: Upload proceeds when backup_logs done flag is present."""
        create_all_sentinels()

        result = result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)
        
        # Must NOT log the absent-warning
        absent_logs = grep_uploadstb_logs("backup_logs not done")
        assert len(absent_logs) == 0, \
            "Should NOT log 'backup_logs not done' when sentinel is present"

        # Must log detection
        detected_logs = grep_uploadstb_logs("bacukup_logs sentinel detected. Proceeding.")
        assert len(detected_logs) > 0, \
            "Expected log: 'bacukup_logs sentinel detected. Proceeding.'"

        # Should proceed to later phases
        progress_logs = grep_uploadstb_logs_regex(
            r"Starting archive phase|Starting upload phase"
        )
        assert len(progress_logs) > 0, \
            "Should proceed to archive/upload phase when backup_logs gate passes"

    @pytest.mark.order(2)
    def test_backup_logs_sentinel_absent_aborts_upload(self):
        """Test: Upload aborts when backup_logs done flag is absent."""
        # All sentinels EXCEPT backup_logs
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        assert result.returncode in [0, 1], "Process should exit cleanly when aborting"

        # Must log the absent-warning
        absent_logs = grep_uploadstb_logs("backup_logs not done")
        assert len(absent_logs) > 0, \
            "Should log 'backup_logs not done' when sentinel is absent"

        # Must NOT reach archive/upload phases
        archive_logs = grep_uploadstb_logs("Starting archive phase")
        assert len(archive_logs) == 0, \
            "Should NOT proceed to archive phase when backup_logs sentinel is absent"

    @pytest.mark.order(3)
    def test_backup_logs_sentinel_absent_no_detection_log(self):
        """Test: Detection log does NOT appear when sentinel is absent."""
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        detected_logs = grep_uploadstb_logs("bacukup_logs sentinel detected. Proceeding.")
        assert len(detected_logs) == 0, \
            "Detection log must not appear when backup_logs sentinel is absent"
