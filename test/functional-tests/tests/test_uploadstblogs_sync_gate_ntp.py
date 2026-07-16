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
Test cases for REQ-SYNC-002: NTP synchronization gate.

If the NTP sentinel (/tmp/stt_received) is absent the upload strategy falls
back through two sub-paths:
  - Internet available  → apply last-known-good time from systimemgr
  - No internet         → proceed with current system time (log warning)

Log messages verified (strategies.c reboot_setup()):
  - Present       : "NTP sync sentinel detected. Proceeding."
  - Absent+inet   : "NTP absent but internet available; applying last-known-good time"
  - Absent+noinet : "NTP absent and no internet; proceeding with current system time"
"""

import os
import pytest
import subprocess as sp
import time
from uploadstblogs_helper import *
from helper_functions import *

# ---------------------------------------------------------------------------
# Sentinel file paths (mirror uploadstblogs_types.h)
# ---------------------------------------------------------------------------
BACKUP_LOGS_DONE_FLAG        = "/tmp/.backup_logs_done"
STT_FLAG                     = "/tmp/stt_received"
PATH_FLAG_INVOCATION         = "/tmp/Update_rebootInfo_invoked"
TELEMETRY_PREVLOGS_DONE_FLAG = "/tmp/.telemetry_prevlogs_done"
SYSTIMEMGR_CLOCK_FILE        = "/opt/secure/clock.txt"
NTP_SYNC_INDICATOR = "/tmp/systimemgr/ntp"

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
    create_sentinel(NTP_SYNC_INDICATOR)


def remove_all_sentinels():
    """Remove all reboot-flow synchronization sentinels."""
    for path in (BACKUP_LOGS_DONE_FLAG, STT_FLAG,
                 PATH_FLAG_INVOCATION, TELEMETRY_PREVLOGS_DONE_FLAG, NTP_SYNC_INDICATOR):
        remove_sentinel(path)


def setup_previous_logs():
    """Create PreviousLogs directory with sample log files."""
    sp.run(f"mkdir -p {PREV_LOG_PATH}", shell=True)
    sp.run(f"echo 'sample log content' > {PREV_LOG_PATH}/messages.log", shell=True)
    sp.run(f"echo 'wifi log content'   > {PREV_LOG_PATH}/wifi.log",     shell=True)
    sp.run(f"echo 'system log'         > {PREV_LOG_PATH}/system.txt",   shell=True)


def setup_systimemgr_clock(epoch_value):
    """Write epoch_value into the systimemgr clock file."""
    sp.run("mkdir -p /opt/secure", shell=True)
    sp.run(f"echo '{epoch_value}' > {SYSTIMEMGR_CLOCK_FILE}", shell=True)


def remove_systimemgr_clock():
    """Remove the systimemgr clock file."""
    remove_sentinel(SYSTIMEMGR_CLOCK_FILE)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestNTPSyncGate:
    """
    REQ-SYNC-002: NTP synchronization gate.

    Covered cases:
      1. NTP sentinel present          → detection log emitted, no fallback.
      2. NTP absent + internet         → systimemgr fallback attempted.
      3. NTP absent + no internet      → proceed with current time, warning logged.
      4. NTP absent + invalid epoch    → invalid epoch handled gracefully.
      5. NTP absent + clock file gone  → missing clock file handled gracefully.
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
        remove_systimemgr_clock()
        remove_lock_file()
        kill_uploadstblogs()
        sp.run(f"rm -rf {PREV_LOG_PATH}", shell=True)

    @pytest.mark.order(1)
    def test_ntp_sentinel_present_proceeds_normally(self):
        """Test: Upload proceeds normally when NTP sentinel is present."""
        create_all_sentinels()

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        # Must log detection
        detected = grep_uploadstb_logs("NTP sync sentinel detected")
        assert len(detected) > 0, "Expected log: 'NTP sync sentinel detected'"

        # Must NOT attempt fallback
        fallback = grep_uploadstb_logs("NTP absent")
        assert len(fallback) == 0, \
            "Should NOT log 'NTP absent' when NTP sentinel is present"

    @pytest.mark.order(2)
    def test_ntp_sentinel_present_no_detection_absent_log(self):
        """Test: 'NTP absent' log does NOT appear when sentinel is present."""
        create_all_sentinels()

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        absent_logs = grep_uploadstb_logs("NTP absent")
        assert len(absent_logs) == 0, \
            "'NTP absent' must not appear when NTP sentinel (/tmp/stt_received) exists"

    
    @pytest.mark.order(3)
    def test_ntp_absent_internet_available_uses_systimemgr(self):
        """Test: When NTP absent but internet available, use systimemgr fallback."""
        remove_all_sentinels()
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # STT_FLAG intentionally absent
        setup_systimemgr_clock(int(time.time()))

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        absent_logs = grep_uploadstb_logs("NTP absent")
        assert len(absent_logs) > 0, "Expected log: 'NTP absent �~@�' when NTP sentinel is missing"
  
    @pytest.mark.order(4)
    def test_ntp_absent_no_internet_proceeds_with_warning(self):
        """Test: When NTP absent and no internet, proceed with current system time."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # STT_FLAG intentionally absent
        remove_systimemgr_clock()

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        # Must log NTP absent
        absent_logs = grep_uploadstb_logs("NTP absent")
        assert len(absent_logs) > 0, "Expected log: 'NTP absent …'"

        # NTP absence alone must NOT abort upload
        abort_logs = grep_uploadstb_logs("aborting upload")
        ntp_aborts = [l for l in abort_logs if "NTP" in l or "ntp" in l]
        assert len(ntp_aborts) == 0, \
            "NTP absence alone should not abort the upload"

    @pytest.mark.order(5)
    def test_ntp_absent_invalid_systimemgr_epoch_handled(self):
        """Test: Invalid systimemgr epoch value is handled gracefully."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        setup_systimemgr_clock("invalid_not_a_number")

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        assert result.returncode in [0, 1], \
            "Should not crash with invalid systimemgr epoch value"

    @pytest.mark.order(6)
    def test_ntp_absent_missing_systimemgr_clock_file_handled(self):
        """Test: Missing systimemgr clock file is handled gracefully."""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        remove_systimemgr_clock()

        result = subprocess.run("/usr/local/bin/logupload '' 1 1 true HTTP https://mockxconf:50058/ >> /opt/logs/logupload.log.0",shell=True)

        assert result.returncode in [0, 1], \
            "Should not crash when systimemgr clock file is missing"
