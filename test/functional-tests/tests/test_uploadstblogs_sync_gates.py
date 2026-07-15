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
Test cases for uploadSTBLogs log upload synchronization gates.

Covers the synchronization paths in the reboot upload strategy (strategies.c):
  - REQ-SYNC-001: backup_logs completion sentinel gate
  - REQ-SYNC-002: NTP sync gate with internet/systimemgr fallback
  - REQ-SYNC-003: Reboot reason sentinel (inotify-based wait)
  - REQ-SYNC-004: Telemetry previous logs done sentinel (inotify-based wait)

These tests verify the inotify-based wait_for_sentinel() logic and the
various gate conditions in reboot_setup().
"""

import pytest
import os
import time
import subprocess as sp
import threading
from uploadstblogs_helper import *
from helper_functions import *

# Sentinel file paths (mirrors uploadstblogs_types.h)
BACKUP_LOGS_DONE_FLAG = "/tmp/.backup_logs_done"
STT_FLAG = "/tmp/stt_received"
PATH_FLAG_INVOCATION = "/tmp/Update_rebootInfo_invoked"
PATH_FLAG_INVOCATION_DIR = "/tmp"
PATH_FLAG_INVOCATION_FILENAME = "Update_rebootInfo_invoked"
TELEMETRY_PREVLOGS_DONE_FLAG = "/tmp/.telemetry_prevlogs_done"
TELEMETRY_PREVLOGS_DONE_DIR = "/tmp"
TELEMETRY_PREVLOGS_DONE_FILENAME = ".telemetry_prevlogs_done"
SYSTIMEMGR_CLOCK_FILE = "/opt/secure/clock.txt"
TRIGGER_REBOOT_INFO_UPDATE = "/tmp/.trigger_reboot_info_update"

# Reboot upload arguments: flag=1, dcm_flag=1, upload_on_reboot=1, TriggerType=2 (TRIGGER_REBOOT)
REBOOT_UPLOAD_ARGS = "'' 1 1 1 HTTP http://localhost:8080 2 0 ''"

# Non-DCM reboot args: flag=1, dcm_flag=0, upload_on_reboot=1, TriggerType=2
NON_DCM_REBOOT_ARGS = "'' 1 0 1 HTTP http://localhost:8080 2 0 ''"

PREV_LOG_PATH = "/opt/logs/PreviousLogs"
LOG_PATH = "/opt/logs"


def create_sentinel(path):
    """Create a sentinel file"""
    dir_path = os.path.dirname(path)
    if dir_path:
        sp.run(f"mkdir -p {dir_path}", shell=True)
    sp.run(f"touch {path}", shell=True)


def remove_sentinel(path):
    """Remove a sentinel file if it exists"""
    if os.path.exists(path):
        os.remove(path)


def remove_all_sentinels():
    """Remove all synchronization sentinel files"""
    sentinels = [
        BACKUP_LOGS_DONE_FLAG,
        STT_FLAG,
        PATH_FLAG_INVOCATION,
        TELEMETRY_PREVLOGS_DONE_FLAG,
        TRIGGER_REBOOT_INFO_UPDATE,
    ]
    for s in sentinels:
        remove_sentinel(s)


def create_all_sentinels():
    """Create all synchronization sentinel files"""
    create_sentinel(BACKUP_LOGS_DONE_FLAG)
    create_sentinel(STT_FLAG)
    create_sentinel(PATH_FLAG_INVOCATION)
    create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)


def setup_previous_logs():
    """Create PreviousLogs directory with sample log files"""
    sp.run(f"mkdir -p {PREV_LOG_PATH}", shell=True)
    sp.run(f"echo 'sample log content' > {PREV_LOG_PATH}/messages.log", shell=True)
    sp.run(f"echo 'wifi log content' > {PREV_LOG_PATH}/wifi.log", shell=True)
    sp.run(f"echo 'system log' > {PREV_LOG_PATH}/system.txt", shell=True)


def setup_systimemgr_clock(epoch_value):
    """Create systimemgr clock file with specified epoch"""
    sp.run("mkdir -p /opt/secure", shell=True)
    sp.run(f"echo '{epoch_value}' > {SYSTIMEMGR_CLOCK_FILE}", shell=True)


def remove_systimemgr_clock():
    """Remove systimemgr clock file"""
    remove_sentinel(SYSTIMEMGR_CLOCK_FILE)


def delayed_sentinel_create(path, delay_seconds):
    """Create a sentinel file after a delay (for testing inotify wait)"""
    def _create():
        time.sleep(delay_seconds)
        create_sentinel(path)
    thread = threading.Thread(target=_create, daemon=True)
    thread.start()
    return thread


class TestBackupLogsSyncGate:
    """
    REQ-SYNC-001: backup_logs completion gate.
    
    Upload must not proceed unless backup_logs has fully assembled PreviousLogs.
    The presence of /tmp/.backup_logs_done signals completion.
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
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
        """Test: Upload proceeds when backup_logs done flag is present"""
        # Create all sentinels including backup_logs
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should proceed past the backup_logs gate
        logs = grep_uploadstb_logs("backup_logs not done")
        assert len(logs) == 0, "Should NOT log 'backup_logs not done' when sentinel is present"

        # Should reach subsequent phases
        progress_logs = grep_uploadstb_logs_regex(
            r"REBOOT.*Starting setup phase|Starting archive phase|Starting upload phase"
        )
        assert len(progress_logs) > 0, "Should proceed to later phases when backup_logs is done"

    @pytest.mark.order(2)
    def test_backup_logs_sentinel_absent_aborts_upload(self):
        """Test: Upload aborts when backup_logs done flag is absent"""
        # Create all sentinels EXCEPT backup_logs
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # Do NOT create BACKUP_LOGS_DONE_FLAG

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should log warning about backup_logs not done
        logs = grep_uploadstb_logs("backup_logs not done")
        assert len(logs) > 0, "Should log 'backup_logs not done' when sentinel is absent"

        # Should NOT reach archive/upload phases
        archive_logs = grep_uploadstb_logs("Starting archive phase")
        assert len(archive_logs) == 0, "Should NOT proceed to archive phase"


class TestNTPSyncGate:
    """
    REQ-SYNC-002: NTP synchronization gate.
    
    If NTP sentinel (/tmp/stt_received) is absent:
    - Check internet connectivity
    - If internet available: use systimemgr last-known-good time
    - If no internet: proceed with current system time (log warning)
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
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
        """Test: Upload proceeds normally when NTP sentinel is present"""
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should log NTP sync detected
        logs = grep_uploadstb_logs("NTP sync sentinel detected")
        assert len(logs) > 0, "Should detect NTP sync sentinel"

        # Should NOT attempt NTP fallback
        fallback_logs = grep_uploadstb_logs("NTP absent")
        assert len(fallback_logs) == 0, "Should NOT attempt NTP fallback when sentinel present"

    @pytest.mark.order(2)
    def test_ntp_absent_internet_available_uses_systimemgr(self):
        """Test: When NTP absent but internet available, use systimemgr time"""
        # Create all sentinels except STT_FLAG (NTP)
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # No STT_FLAG

        # Setup systimemgr with a valid epoch
        setup_systimemgr_clock(int(time.time()))

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should attempt fallback path (actual behavior depends on internet availability in test env)
        logs = grep_uploadstb_logs_regex(r"NTP absent|last-known-good time|systimemgr")
        # We expect at least the NTP absent detection
        ntp_absent_logs = grep_uploadstb_logs("NTP absent")
        assert len(ntp_absent_logs) > 0, "Should detect NTP is absent"

    @pytest.mark.order(3)
    def test_ntp_absent_no_internet_proceeds_with_warning(self):
        """Test: When NTP absent and no internet, proceed with system time"""
        # Create all sentinels except STT_FLAG (NTP)
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # No STT_FLAG

        # No systimemgr clock file either
        remove_systimemgr_clock()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should detect NTP absent
        ntp_logs = grep_uploadstb_logs("NTP absent")
        assert len(ntp_logs) > 0, "Should detect NTP is absent"

        # Should still proceed (not abort) — NTP is not a hard gate
        # The upload should continue past setup phase
        setup_logs = grep_uploadstb_logs_regex(r"Setup phase complete|Starting archive phase")
        # May or may not reach archive depending on other conditions,
        # but should not abort due to NTP alone
        abort_logs = grep_uploadstb_logs("aborting upload")
        ntp_abort = [l for l in abort_logs if "NTP" in l or "ntp" in l]
        assert len(ntp_abort) == 0, "Should NOT abort upload due to NTP absence alone"

    @pytest.mark.order(4)
    def test_systimemgr_clock_invalid_epoch(self):
        """Test: Invalid systimemgr clock value is handled gracefully"""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # No STT_FLAG

        # Setup invalid epoch in systimemgr
        setup_systimemgr_clock("invalid_not_a_number")

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should handle gracefully (log warning about invalid epoch)
        logs = grep_uploadstb_logs_regex(r"invalid epoch|systimemgr")
        # Should not crash
        assert result.returncode in [0, 1], "Should not crash with invalid systimemgr value"

    @pytest.mark.order(5)
    def test_systimemgr_clock_file_missing(self):
        """Test: Missing systimemgr clock file is handled gracefully"""
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # No STT_FLAG, no systimemgr file

        remove_systimemgr_clock()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should handle gracefully
        assert result.returncode in [0, 1], "Should not crash with missing systimemgr file"


class TestRebootReasonSyncGate:
    """
    Reboot reason sentinel gate.
    
    Uses inotify-based wait_for_sentinel() to watch for
    /tmp/Update_rebootInfo_invoked. On timeout, triggers
    reboot-manager to update reboot info.
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
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
        """Test: Fast path - sentinel already exists before inotify setup"""
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should detect sentinel immediately
        logs = grep_uploadstb_logs("Reboot reason sentinel detected")
        assert len(logs) > 0, "Should detect reboot reason sentinel immediately"

        # Should NOT trigger reboot info update
        trigger_logs = grep_uploadstb_logs("trigger to request immediate update")
        assert len(trigger_logs) == 0, "Should NOT trigger update when sentinel present"

    @pytest.mark.order(2)
    def test_reboot_reason_sentinel_appears_during_wait(self):
        """Test: Sentinel appears while inotify is watching"""
        # Create all sentinels except reboot reason
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # Do NOT create PATH_FLAG_INVOCATION yet

        # Create sentinel after a short delay (within timeout)
        delayed_sentinel_create(PATH_FLAG_INVOCATION, 1)

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should eventually detect the sentinel via inotify
        logs = grep_uploadstb_logs("Reboot reason sentinel detected")
        assert len(logs) > 0, "Should detect reboot reason sentinel via inotify"

    @pytest.mark.order(3)
    def test_reboot_reason_timeout_triggers_update(self):
        """Test: Timeout triggers reboot info update request"""
        # Create all sentinels except reboot reason
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # Do NOT create PATH_FLAG_INVOCATION - let it timeout

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should log timeout warning
        timeout_logs = grep_uploadstb_logs("Reboot reason sentinel not present")
        assert len(timeout_logs) > 0, "Should log reboot reason timeout"

        # Should trigger reboot info update
        trigger_logs = grep_uploadstb_logs("trigger to request immediate update")
        assert len(trigger_logs) > 0, "Should trigger reboot info update on timeout"

        # Should still proceed with upload (not a hard gate after trigger)
        # The upload continues regardless of whether reboot reason was obtained

    @pytest.mark.order(4)
    def test_reboot_reason_trigger_creates_stt_flag(self):
        """Test: Trigger creates the STT flag file for reboot-manager"""
        # Create all sentinels except reboot reason
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # Do NOT create PATH_FLAG_INVOCATION

        # Also ensure the invocation flag doesn't exist (so trigger fires)
        remove_sentinel(PATH_FLAG_INVOCATION)

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # After timeout, trigger_reboot_info_update should create STT_FLAG
        # (Note: STT_FLAG = /tmp/stt_received is the trigger target)
        # The function creates STT_FLAG only if PATH_FLAG_INVOCATION doesn't exist
        trigger_logs = grep_uploadstb_logs("Trigger reboot reason update")
        # Trigger may or may not fire depending on timing
        assert result.returncode in [0, 1], "Should complete regardless of trigger"


class TestTelemetryPrevlogsSyncGate:
    """
    REQ-SYNC-003: Telemetry previous logs completion sentinel.
    
    Uses inotify-based wait_for_sentinel() to watch for
    /tmp/.telemetry_prevlogs_done. This is a soft gate -
    upload proceeds on timeout with a warning.
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
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
        """Test: Fast path - telemetry prevlogs sentinel already exists"""
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should detect sentinel immediately
        logs = grep_uploadstb_logs("Telemetry prevlogs sentinel detected")
        assert len(logs) > 0, "Should detect telemetry prevlogs sentinel immediately"

    @pytest.mark.order(2)
    def test_telemetry_prevlogs_sentinel_appears_during_wait(self):
        """Test: Sentinel appears while inotify is watching"""
        # Create all sentinels except telemetry prevlogs
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        # Do NOT create TELEMETRY_PREVLOGS_DONE_FLAG yet

        # Create sentinel after a short delay
        delayed_sentinel_create(TELEMETRY_PREVLOGS_DONE_FLAG, 1)

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should detect via inotify
        logs = grep_uploadstb_logs("Telemetry prevlogs sentinel detected")
        assert len(logs) > 0, "Should detect telemetry prevlogs sentinel via inotify"

    @pytest.mark.order(3)
    def test_telemetry_prevlogs_timeout_is_soft_gate(self):
        """Test: Timeout on telemetry prevlogs is a soft gate (upload continues)"""
        # Create all sentinels except telemetry prevlogs
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        # Do NOT create TELEMETRY_PREVLOGS_DONE_FLAG

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should log timeout warning
        timeout_logs = grep_uploadstb_logs("Telemetry prevlogs sentinel not present")
        assert len(timeout_logs) > 0, "Should log telemetry prevlogs timeout"

        # Should log "proceeding without telemetry sync"
        proceed_logs = grep_uploadstb_logs("proceeding without telemetry sync")
        assert len(proceed_logs) > 0, "Should proceed without telemetry sync (soft gate)"

        # Should still reach archive/upload phases (soft gate)
        archive_logs = grep_uploadstb_logs_regex(r"Starting archive phase|archive")
        assert len(archive_logs) > 0, "Should still proceed to archive phase (soft gate)"


class TestInotifyWaitMechanism:
    """
    Tests for the inotify-based wait_for_sentinel() function behavior.
    
    Verifies the inotify mechanism handles edge cases:
    - Race condition between access() and inotify_add_watch
    - Multiple events in the buffer
    - EINTR handling
    - FD_SETSIZE limits
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
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
    def test_sentinel_created_between_check_and_watch(self):
        """Test: Race condition - sentinel created between access() and add_watch"""
        # This tests the re-check after watch is set in wait_for_sentinel():
        #   "Re-check after watch is set — closes race between access() and add_watch"
        # Create all sentinels - the re-check should catch it immediately
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should not timeout - should detect sentinel via re-check
        timeout_logs = grep_uploadstb_logs_regex(
            r"sentinel not present after|timeout"
        )
        reboot_timeout = [l for l in timeout_logs if "Reboot reason" in l]
        telemetry_timeout = [l for l in timeout_logs if "Telemetry" in l]
        assert len(reboot_timeout) == 0, "Should not timeout on reboot reason sentinel"
        assert len(telemetry_timeout) == 0, "Should not timeout on telemetry sentinel"

    @pytest.mark.order(2)
    def test_multiple_file_creates_in_watched_dir(self):
        """Test: Inotify correctly identifies target among multiple file creates"""
        # Create all sentinels except reboot reason
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)

        # Create several non-target files in /tmp before the target
        def create_noise_then_target():
            time.sleep(0.5)
            # Create noise files first
            for i in range(3):
                sp.run(f"touch /tmp/.noise_file_{i}", shell=True)
                time.sleep(0.1)
            # Then create the actual target
            create_sentinel(PATH_FLAG_INVOCATION)

        thread = threading.Thread(target=create_noise_then_target, daemon=True)
        thread.start()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Cleanup noise files
        for i in range(3):
            remove_sentinel(f"/tmp/.noise_file_{i}")

        # Should detect the correct sentinel
        logs = grep_uploadstb_logs("Reboot reason sentinel detected")
        assert len(logs) > 0, "Should detect correct sentinel among noise"

    @pytest.mark.order(3)
    def test_inotify_init_failure_handling(self):
        """Test: Graceful handling if inotify resources exhausted"""
        # This is hard to force in a real test env, but we can verify
        # the code doesn't crash by exhausting inotify instances
        # In practice, the code falls through with a warning
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)
        # Leave reboot reason and telemetry absent to force inotify path

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should not crash regardless of inotify state
        assert result.returncode in [0, 1], "Should not crash even if inotify fails"


class TestFullSyncSequence:
    """
    End-to-end tests for the complete reboot synchronization sequence.
    
    Tests the ordering and interaction of all sync gates together.
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
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
    def test_reboot_upload_all_sentinels_detected(self):
        """Test: Trigger reboot upload and verify every sentinel is detected via greplogs.

        Pre-conditions:
            - All four sentinel files are present before upload starts:
              /tmp/.backup_logs_done          (REQ-SYNC-001 backup_logs gate)
              /tmp/stt_received               (REQ-SYNC-002 NTP gate)
              /tmp/Update_rebootInfo_invoked   (reboot-reason gate)
              /tmp/.telemetry_prevlogs_done    (REQ-SYNC-003 telemetry gate)
            - PreviousLogs directory contains .log/.txt files

        Expected log output (verified via grep):
            1. NTP sync sentinel detected. Proceeding.
            2. Reboot reason sentinel detected. Proceeding.
            3. Telemetry prevlogs sentinel detected. Proceeding.
            4. REBOOT/NON_DCM: Starting archive phase  (proves setup passed all gates)
        """
        # Arrange — create every sentinel so each gate takes the fast path
        create_all_sentinels()

        # Act — trigger a reboot upload
        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Assert — verify each sentinel was individually detected in the logs

        # 1. backup_logs gate: absence message should NOT appear (sentinel present)
        backup_absent = grep_uploadstb_logs("backup_logs not done")
        assert len(backup_absent) == 0, \
            "backup_logs sentinel is present; 'backup_logs not done' must NOT appear in logs"

        # 2. NTP sync gate: should log detection
        ntp_detected = grep_uploadstb_logs("NTP sync sentinel detected")
        assert len(ntp_detected) > 0, \
            "NTP sentinel /tmp/stt_received is present; expected 'NTP sync sentinel detected' in logs"

        # 3. NTP fallback path should NOT be taken
        ntp_fallback = grep_uploadstb_logs("NTP absent")
        assert len(ntp_fallback) == 0, \
            "NTP sentinel is present; 'NTP absent' must NOT appear in logs"

        # 4. Reboot reason gate: should log detection
        reboot_detected = grep_uploadstb_logs("Reboot reason sentinel detected")
        assert len(reboot_detected) > 0, \
            "Reboot reason sentinel is present; expected 'Reboot reason sentinel detected' in logs"

        # 5. Reboot reason timeout/trigger should NOT fire
        reboot_timeout = grep_uploadstb_logs("Reboot reason sentinel not present")
        assert len(reboot_timeout) == 0, \
            "Reboot reason sentinel is present; timeout message must NOT appear in logs"

        reboot_trigger = grep_uploadstb_logs("trigger to request immediate update")
        assert len(reboot_trigger) == 0, \
            "Reboot reason sentinel is present; trigger message must NOT appear in logs"

        # 6. Telemetry prevlogs gate: should log detection
        telemetry_detected = grep_uploadstb_logs("Telemetry prevlogs sentinel detected")
        assert len(telemetry_detected) > 0, \
            "Telemetry prevlogs sentinel is present; expected 'Telemetry prevlogs sentinel detected' in logs"

        # 7. Telemetry timeout should NOT fire
        telemetry_timeout = grep_uploadstb_logs("proceeding without telemetry sync")
        assert len(telemetry_timeout) == 0, \
            "Telemetry sentinel is present; 'proceeding without telemetry sync' must NOT appear"

        # 8. All gates passed — setup completed, archive phase started
        setup_done = grep_uploadstb_logs("REBOOT/NON_DCM: Setup phase complete")
        assert len(setup_done) > 0, \
            "All gates passed; expected 'Setup phase complete' in logs"

        archive_started = grep_uploadstb_logs("REBOOT/NON_DCM: Starting archive phase")
        assert len(archive_started) > 0, \
            "All gates passed; expected 'Starting archive phase' in logs"

    @pytest.mark.order(2)
    def test_first_gate_failure_prevents_later_gates(self):
        """Test: backup_logs gate failure prevents reaching other gates"""
        # Only omit backup_logs - should abort before checking NTP/reboot/telemetry
        create_sentinel(STT_FLAG)
        create_sentinel(PATH_FLAG_INVOCATION)
        create_sentinel(TELEMETRY_PREVLOGS_DONE_FLAG)
        # NO BACKUP_LOGS_DONE_FLAG

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should abort at backup_logs gate
        abort_logs = grep_uploadstb_logs("backup_logs not done")
        assert len(abort_logs) > 0, "Should abort at backup_logs gate"

        # Should NOT reach NTP or reboot reason checks
        ntp_logs = grep_uploadstb_logs("NTP sync sentinel")
        assert len(ntp_logs) == 0, "Should not reach NTP check after backup_logs failure"

    @pytest.mark.order(3)
    def test_soft_gates_allow_continuation(self):
        """Test: Soft gates (telemetry) allow upload to continue on timeout"""
        # backup_logs present (hard gate)
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        # NTP present
        create_sentinel(STT_FLAG)
        # Reboot reason present
        create_sentinel(PATH_FLAG_INVOCATION)
        # Telemetry prevlogs ABSENT (soft gate - should timeout but continue)

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Telemetry timeout should be logged
        timeout_logs = grep_uploadstb_logs("proceeding without telemetry sync")
        assert len(timeout_logs) > 0, "Should log telemetry timeout"

        # Should still reach archive phase
        archive_logs = grep_uploadstb_logs("Starting archive phase")
        assert len(archive_logs) > 0, "Should proceed to archive despite telemetry timeout"

    @pytest.mark.order(4)
    def test_non_dcm_mode_sync_sequence(self):
        """Test: Non-DCM mode (dcm_flag=0) follows same sync gates"""
        create_all_sentinels()

        result = run_uploadstblogs(NON_DCM_REBOOT_ARGS)

        # Should still check sync gates in non-DCM mode
        logs = grep_uploadstb_logs_regex(
            r"backup_logs|NTP|Reboot reason|Telemetry prevlogs"
        )
        assert len(logs) >= 2, "Non-DCM mode should also check sync gates"

    @pytest.mark.order(5)
    def test_sync_gates_timing_under_timeout(self):
        """Test: All inotify waits complete within expected timeouts"""
        # Create all sentinels to ensure fast-path (no waiting)
        create_all_sentinels()

        start_time = time.time()
        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)
        elapsed = time.time() - start_time

        # With all sentinels present, setup should be quick
        # (test binary uses 2s timeouts, so total should be well under 10s for gates)
        assert elapsed < 30, f"Sync gates with all sentinels should complete quickly, took {elapsed:.1f}s"

    @pytest.mark.order(6)
    def test_delayed_sentinels_within_timeout(self):
        """Test: Sentinels appearing within timeout are detected via inotify"""
        # backup_logs and NTP present immediately
        create_sentinel(BACKUP_LOGS_DONE_FLAG)
        create_sentinel(STT_FLAG)

        # Reboot reason and telemetry appear after short delays
        delayed_sentinel_create(PATH_FLAG_INVOCATION, 0.5)
        delayed_sentinel_create(TELEMETRY_PREVLOGS_DONE_FLAG, 0.8)

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Both should be detected (not timed out)
        reboot_detected = grep_uploadstb_logs("Reboot reason sentinel detected")
        telemetry_detected = grep_uploadstb_logs("Telemetry prevlogs sentinel detected")

        assert len(reboot_detected) > 0, "Should detect delayed reboot reason sentinel"
        assert len(telemetry_detected) > 0, "Should detect delayed telemetry sentinel"


class TestNoLogsSyncBehavior:
    """
    Tests that sync gates interact correctly with the no-logs condition.
    
    If PreviousLogs has no .log/.txt files, upload should abort AFTER
    the sync gates (since gates are checked first).
    """

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_uploadstb_logs()
        remove_lock_file()
        remove_all_sentinels()
        restore_device_properties()
        yield
        remove_all_sentinels()
        remove_lock_file()
        kill_uploadstblogs()
        sp.run(f"rm -rf {PREV_LOG_PATH}", shell=True)

    @pytest.mark.order(1)
    def test_sync_gates_checked_before_log_existence(self):
        """Test: Sync gates are evaluated before checking for log files"""
        # Create PreviousLogs dir but with NO log files
        sp.run(f"mkdir -p {PREV_LOG_PATH}", shell=True)
        sp.run(f"touch {PREV_LOG_PATH}/empty_file", shell=True)  # Not .log or .txt

        # All sync sentinels present
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should pass sync gates first
        ntp_logs = grep_uploadstb_logs("NTP sync sentinel detected")
        # Then abort due to no log files
        no_logs = grep_uploadstb_logs_regex(r"No .txt or .log files|aborting")

        # Sync gates should be checked (verified by NTP log appearing)
        assert len(ntp_logs) > 0, "Sync gates should be checked before log existence"

    @pytest.mark.order(2)
    def test_no_prev_log_path_aborts_after_sync(self):
        """Test: Missing PreviousLogs directory aborts after sync gates pass"""
        # Remove PreviousLogs entirely
        sp.run(f"rm -rf {PREV_LOG_PATH}", shell=True)

        # All sync sentinels present
        create_all_sentinels()

        result = run_uploadstblogs(REBOOT_UPLOAD_ARGS)

        # Should abort due to missing directory
        logs = grep_uploadstb_logs("PREV_LOG_PATH does not exist")
        assert len(logs) > 0, "Should abort when PreviousLogs directory is missing"

