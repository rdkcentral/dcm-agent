####################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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
Test cases for REQ-SYNC-004: Telemetry previous logs done sentinel gate.

Uses inotify-based wait_for_sentinel() to watch for
/tmp/.telemetry_prevlogs_done.  This is a **soft gate** — when the timeout
expires the upload continues with a warning rather than aborting.

Log messages verified (strategies.c reboot_setup()):
  - Present : "Telemetry prevlogs sentinel detected. Proceeding."
  - Absent  : "Telemetry prevlogs sentinel not present after %us ,
               proceeding without telemetry sync"
"""

import os
import pytest
import subprocess
import subprocess as sp
import time
import threading
from uploadstblogs_helper import *
from helper_functions import *

# ---------------------------------------------------------------------------
# Sentinel file paths (mirror uploadstblogs_types.h)
# ---------------------------------------------------------------------------
BACKUP_LOGS_DONE_FLAG        = "/tmp/.backup_logs_done"
STT_FLAG                     = "/tmp/stt_received"
PATH_FLAG_INVOCATION         = "/tmp/Update_rebootInfo_invoked"
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


def delayed_sentinel_create(path, delay_seconds):
    """Create a sentinel file after a delay (for testing inotify detection)."""
    def _create():
        time.sleep(delay_seconds)
        create_sentinel(path)
    thread = threading.Thread(target=_create, daemon=True)
    thread.start()
    return thread


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestTelemetryPrevlogsSyncGate:
    """
    REQ-SYNC-004: Telemetry previous logs done sentinel gate (inotify-based, soft).

    Covered cases:
      1. Sentinel already present (fast path).
      2. Sentinel appears while inotify is watching (inotify detection).
      3. Sentinel never appears → soft gate: warning logged, upload continues.
      4. Detection log absent when sentinel is missing.
      5. Soft gate: archive phase reached even on timeout.
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
    def test_telemetry_prevlogs_sentinel_already_present(self):
        """Test: Fast path — sentinel exists before inotify watch is set up."""
        create_all_sentinels()

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        # Must log detection
        detected = grep_uploadstb_logs("Telemetry prevlogs sentinel detected. Proceeding.")
        assert len(detected) > 0, \
            "Expected log: 'Telemetry prevlogs sentinel detected. Proceeding.'"

        # Must NOT log the absent/timeout warning
        timeout_log = grep_uploadstb_logs("proceeding without telemetry sync")
        assert len(timeout_log) == 0, \
            "Should NOT log 'proceeding without telemetry sync' when sentinel is present"

    @pytest.mark.order(2)
    def test_telemetry_prevlogs_sentinel_appears_during_inotify_wait(self):
        """Test: Sentinel appears while inotify is watching — detected via IN_CREATE."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        # TELEMETRY_PREVLOGS_DONE_FLAG will appear after 1 s
        thread = delayed_sentinel_create(TELEMETRY_PREVLOGS_DONE_FLAG, 1)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)
        thread.join(timeout=10)

        assert result.returncode in [0, 1], "Process should complete cleanly"

        detected = grep_uploadstb_logs("Telemetry prevlogs sentinel detected")
        assert len(detected) > 0, \
            "Should detect telemetry prevlogs sentinel via inotify after delay"

    @pytest.mark.order(3)
    def test_telemetry_prevlogs_timeout_is_soft_gate(self):
        """Test: Timeout logs warning but does NOT abort the upload."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        # TELEMETRY_PREVLOGS_DONE_FLAG intentionally absent

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        assert result.returncode in [0, 1], "Process should exit cleanly"

        # Must log the timeout/absent warning
        timeout_logs = grep_uploadstb_logs("Telemetry prevlogs sentinel not present")
        assert len(timeout_logs) > 0, \
            "Expected log: 'Telemetry prevlogs sentinel not present after …'"

        # Must log "proceeding without telemetry sync"
        proceed_logs = grep_uploadstb_logs("proceeding without telemetry sync")
        assert len(proceed_logs) > 0, \
            "Expected log: '… proceeding without telemetry sync'"

    @pytest.mark.order(4)
    def test_telemetry_prevlogs_absent_no_detection_log(self):
        """Test: Detection log does NOT appear when sentinel is absent."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        detected = grep_uploadstb_logs("Telemetry prevlogs sentinel detected. Proceeding.")
        assert len(detected) == 0, \
            "Detection log must not appear when telemetry prevlogs sentinel is absent"

    @pytest.mark.order(5)
    def test_telemetry_prevlogs_timeout_archive_phase_still_reached(self):
        """Test: Soft gate — archive phase is reached even when telemetry times out."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        # TELEMETRY_PREVLOGS_DONE_FLAG intentionally absent

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        archive_logs = grep_uploadstb_logs_regex(r"Starting archive phase|archive")
        assert len(archive_logs) > 0, \
            "Should proceed to archive phase even when telemetry prevlogs gate times out"
