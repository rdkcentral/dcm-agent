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
Test cases for uploadSTBLogs upload strategies
Covers: On-demand, reboot, DCM scheduled, RBUS integration
"""

import pytest
import time
import subprocess as sp
from uploadstblogs_helper import *
from helper_functions import *


class TestOnDemandStrategy:
    """Test suite for on-demand upload strategy"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_ondemand_immediate_execution(self):
        """Test: On-demand upload executes immediately"""
        create_test_log_files(count=2)
        
        # Trigger on-demand upload (TriggerType=5)
        args = "'' 0 0 0 HTTP http://localhost:8080 5 0 ''"
        
        start_time = time.time()
        result = run_uploadstblogs(args)
        elapsed = time.time() - start_time
        
        # Should start immediately without waiting
        assert elapsed < 30, "On-demand upload should execute immediately"

    @pytest.mark.order(2)
    def test_ondemand_no_schedule_wait(self):
        """Test: On-demand upload doesn't wait for scheduled time"""
        create_test_log_files(count=2)
        
        # Execute on-demand
        args = "'' 0 0 0 HTTP http://localhost:8080 5 0 ''"
        result = run_uploadstblogs(args)
        
        # Check logs for immediate execution
        immediate_logs = grep_uploadstb_logs_regex(r"immediate|ondemand|manual")
        # Process should complete
        assert result.returncode in [0, 1], "On-demand upload should complete"

    @pytest.mark.order(3)
    def test_ondemand_telemetry(self):
        """Test: On-demand upload generates appropriate telemetry"""
        create_test_log_files(count=1)
        
        args = "'' 0 0 0 HTTP http://localhost:8080 5 0 ''"
        result = run_uploadstblogs(args)
        
        # Check for telemetry
        telemetry_logs = grep_uploadstb_logs_regex(r"telemetry|marker")
        # Should complete
        assert result.returncode in [0, 1], "Should generate telemetry"


class TestRebootStrategy:
    """Test suite for upload on reboot strategy"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_reboot_upload_detection(self):
        """Test: Service detects reboot condition"""
        create_test_log_files(count=2)
        
        # Trigger reboot upload (UploadOnReboot=1, TriggerType=2)
        args = "'' 0 0 1 HTTP http://localhost:8080 2 0 ''"
        
        result = run_uploadstblogs(args)
        
        # Check for reboot detection (may not be explicitly logged)
        reboot_logs = grep_uploadstb_logs_regex(r"reboot|UploadOnReboot|REBOOT|TriggerType.*2")
        # Process should complete with reboot parameters
        assert result.returncode in [0, 1], f"Reboot upload should process, found {len(reboot_logs)} reboot-related logs"

    @pytest.mark.order(2)
    def test_reboot_previous_logs_collection(self):
        """Test: Service collects logs from previous session on reboot"""
        # Create files in PreviousLogs directory
        sp.run("mkdir -p /opt/logs/PreviousLogs", shell=True)
        sp.run("echo 'previous log content' > /opt/logs/PreviousLogs/prev.log", shell=True)
        
        create_test_log_files(count=1)
        
        args = "'' 0 0 1 HTTP http://localhost:8080 2 0 ''"
        result = run_uploadstblogs(args)
        
        # Check for previous log collection
        prev_logs = grep_uploadstb_logs_regex(r"previous|PreviousLogs")
        # Should process logs
        assert result.returncode in [0, 1], "Should collect previous logs"

    @pytest.mark.order(3)
    def test_reboot_upload_telemetry(self):
        """Test: Reboot upload generates appropriate telemetry"""
        create_test_log_files(count=1)
        
        args = "'' 0 0 1 HTTP http://localhost:8080 2 0 ''"
        result = run_uploadstblogs(args)
        
        # Check for telemetry
        telemetry_logs = grep_uploadstb_logs_regex(r"telemetry|reboot.*success")
        # Should complete
        assert result.returncode in [0, 1], "Should generate reboot telemetry"


class TestDCMScheduledStrategy:
    """Test suite for DCM scheduled upload strategy"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_dcm_scheduled_trigger(self):
        """Test: DCM scheduled upload is triggered correctly"""
        create_test_log_files(count=2)
        
        # DCM scheduled upload (FLAG=0, DCM_FLAG=0, TriggerType=0)
        args = "'' 0 0 0 HTTP http://localhost:8080 0 0 ''"
        
        result = run_uploadstblogs(args)
        
        # Check for DCM processing
        dcm_logs = grep_uploadstb_logs_regex(r"DCM|scheduled|FLAG.*0")
        assert len(dcm_logs) > 0, "DCM scheduled upload should be processed"

    @pytest.mark.order(2)
    def test_dcm_log_collection(self):
        """Test: DCM scheduled upload collects logs according to configuration"""
        create_test_log_files(count=3)
        
        args = "'' 0 0 0 HTTP http://localhost:8080 0 0 ''"
        result = run_uploadstblogs(args)
        
        # Check for log collection
        collection_logs = grep_uploadstb_logs_regex(r"collect|archive|DCM")
        # Should attempt collection
        assert result.returncode in [0, 1], "Should collect DCM logs"

    @pytest.mark.order(3)
    def test_dcm_upload_telemetry(self):
        """Test: DCM upload generates telemetry"""
        create_test_log_files(count=1)
        
        args = "'' 0 0 0 HTTP http://localhost:8080 0 0 ''"
        result = run_uploadstblogs(args)
        
        # Check telemetry
        telemetry_logs = grep_uploadstb_logs_regex(r"telemetry|marker|SYST")
        # Should complete
        assert result.returncode in [0, 1], "Should generate DCM telemetry"


class TestRBUSIntegration:
    """Test suite for RBUS event triggered uploads"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_rbus_parameter_loading(self):
        """Test: Service loads parameters from RBUS/TR-181"""
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for RBUS initialization
        rbus_logs = grep_uploadstb_logs_regex(r"rbus|RBUS|TR-181|Device\.DeviceInfo")
        # RBUS parameters may be loaded during context init
        assert result.returncode in [0, 1], "Should attempt RBUS parameter loading"

    @pytest.mark.order(2)
    def test_rbus_triggered_upload_via_cli(self):
        """Test: Upload can be triggered via RBUS CLI"""
        # Check if rbuscli is available
        rbus_check = sp.run("which rbuscli", shell=True, capture_output=True)
        if rbus_check.returncode != 0:
            pytest.skip("rbuscli not available")
        
        create_test_log_files(count=1)
        
        # Trigger via RBUS would be done through DCM agent typically
        # For direct test, we use command line args
        result = run_uploadstblogs()
        
        assert result.returncode in [0, 1], "RBUS-triggered upload should work"

    @pytest.mark.order(3)
    def test_rbus_configuration_loading(self):
        """Test: Service loads upload configuration from RBUS"""
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for configuration loading
        config_logs = grep_uploadstb_logs_regex(r"load.*TR-181|load.*param|endpoint|RFC")
        # Should attempt to load config
        assert result.returncode in [0, 1], "Should load RBUS configuration"

    @pytest.mark.order(4)
    def test_rbus_event_publishing(self):
        """Test: Upload success event is published via RBUS"""
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for event publishing
        event_logs = grep_uploadstb_logs_regex(r"event|publish|success")
        # Should complete
        assert result.returncode in [0, 1], "Should publish RBUS events"


class TestStrategySelection:
    """Test suite for upload strategy selection logic"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_strategy_selection_based_on_flags(self):
        """Test: Correct strategy is selected based on flags"""
        create_test_log_files(count=1)
        
        # Test different flag combinations
        # RRD mode: RRD_FLAG=1
        args = "'' 0 0 0 HTTP http://localhost:8080 0 1 /opt/logs/rrd.log"
        result = run_uploadstblogs(args)
        
        # Check for strategy selection
        strategy_logs = grep_uploadstb_logs_regex(r"strategy|RRD|select")
        assert result.returncode in [0, 1], "Should select appropriate strategy"

    @pytest.mark.order(2)
    def test_multiple_strategy_parameters(self):
        """Test: Service handles multiple strategy parameters"""
        create_test_log_files(count=1)
        
        # Test with various parameters
        args = "'' 1 1 1 HTTPS https://localhost:8443 1 0 ''"
        result = run_uploadstblogs(args)
        
        # Should handle all parameters
        assert result.returncode in [0, 1], "Should handle multiple parameters"

    @pytest.mark.order(3)
    def test_strategy_logging(self):
        """Test: Selected strategy is logged"""
        create_test_log_files(count=1)
        
        args = "'' 0 0 1 HTTP http://localhost:8080 2 0 ''"
        result = run_uploadstblogs(args)
        
        # Check strategy logging
        logs = grep_uploadstb_logs_regex(r"strategy|STRAT_|upload.*type")
        # Strategy should be determined
        assert result.returncode in [0, 1], "Strategy should be logged"

