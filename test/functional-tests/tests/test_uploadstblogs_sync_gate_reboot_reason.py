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
Test cases for REQ-SYNC-003: Reboot reason sentinel gate.

Uses inotify-based wait_for_sentinel() to watch for
/tmp/Update_rebootInfo_invoked.  On timeout, trigger_reboot_info_update()
creates /tmp/stt_received so reboot-manager re-populates the reason;
upload continues regardless (not a hard abort gate).

Log messages verified (strategies.c reboot_setup()):
  - Present  : "Reboot reason sentinel detected. Proceeding."
  - Absent   : "Reboot reason sentinel not present after %us. trigger to request immediate update."
  - Trigger  : "Trigger reboot reason update: /tmp/stt_received"
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

class TestRebootReasonSyncGate:
    """
    REQ-SYNC-003: Reboot reason sentinel gate (inotify-based).

    Covered cases:
      1. Sentinel already present (fast path).
      2. Sentinel appears while inotify is watching (inotify detection).
      3. Sentinel never appears → timeout triggers update request.
      4. Detection log absent when sentinel is missing.
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
    def test_reboot_reason_sentinel_already_present(self):
        """Test: Fast path — sentinel exists before inotify watch is set up."""
        create_all_sentinels()

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        # Must log detection
        detected = grep_uploadstb_logs("Reboot reason sentinel detected. Proceeding.")
        assert len(detected) > 0, \
            "Expected log: 'Reboot reason sentinel detected. Proceeding.'"

        # Must NOT fire the timeout trigger
        trigger = grep_uploadstb_logs("trigger to request immediate update")
        assert len(trigger) == 0, \
            "Should NOT trigger update when sentinel is already present"

    @pytest.mark.order(2)
    def test_reboot_reason_sentinel_appears_during_inotify_wait(self):
        """Test: Sentinel appears while inotify is watching — detected via IN_CREATE."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # PATH_FLAG_INVOCATION will appear after 1 s
        thread = delayed_sentinel_create(PATH_FLAG_INVOCATION, 1)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)
        thread.join(timeout=10)

        assert result.returncode in [0, 1], "Process should complete cleanly"

        detected = grep_uploadstb_logs("Reboot reason sentinel detected")
        assert len(detected) > 0, \
            "Should detect reboot reason sentinel via inotify after delay"

    @pytest.mark.order(3)
    def test_reboot_reason_timeout_triggers_update(self):
        """Test: Timeout fires the reboot info update trigger."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # PATH_FLAG_INVOCATION intentionally absent

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        # Must log timeout warning
        timeout_logs = grep_uploadstb_logs("Reboot reason sentinel not present")
        assert len(timeout_logs) > 0, \
            "Expected log: 'Reboot reason sentinel not present after …'"

        # Must fire the update trigger
        trigger_logs = grep_uploadstb_logs("trigger to request immediate update")
        assert len(trigger_logs) > 0, \
            "Should trigger reboot info update on timeout"

    @pytest.mark.order(4)
    def test_reboot_reason_absent_no_detection_log(self):
        """Test: Detection log does NOT appear when sentinel is absent."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        detected = grep_uploadstb_logs("Reboot reason sentinel detected. Proceeding.")
        assert len(detected) == 0, \
            "Detection log must not appear when reboot reason sentinel is absent"

    @pytest.mark.order(5)
    def test_reboot_reason_timeout_upload_still_continues(self):
        """Test: Upload is not hard-aborted when reboot reason sentinel times out."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        assert result.returncode in [0, 1], "Process should exit cleanly"

        # Gate is not a hard abort — should still proceed past setup
        abort_due_to_reboot = grep_uploadstb_logs("aborting upload")
        reboot_aborts = [l for l in abort_due_to_reboot if "reboot" in l.lower()]
        assert len(reboot_aborts) == 0, \
            "Reboot reason timeout should NOT hard-abort the upload"
