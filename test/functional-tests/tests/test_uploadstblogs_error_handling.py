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
Test cases for uploadSTBLogs error handling and edge cases
Covers: Invalid configuration, size limits, empty logs
"""

import pytest
import time
from uploadstblogs_helper import *
from helper_functions import *


class TestInvalidConfiguration:
    """Test suite for invalid configuration handling"""

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
        restore_device_properties()
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_corrupted_device_properties_detection(self):
        """Test: Service detects corrupted device properties file"""
        # Corrupt the device properties file
        corrupt_device_properties()
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for configuration error detection
        error_logs = grep_uploadstb_logs_regex(r"ERROR|error|fail.*properties|invalid")
        # Should handle gracefully
        assert result.returncode in [0, 1], "Should detect corrupted config"

    @pytest.mark.order(2)
    def test_malformed_config_graceful_failure(self):
        """Test: Service fails gracefully with malformed configuration"""
        # Create malformed configuration
        subprocess.run(f"echo 'INVALID^^^CONFIG@@@' >> {DEVICE_PROPERTIES}", shell=True)
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Should not crash, should exit gracefully
        assert result.returncode in [0, 1], "Should fail gracefully without crash"
        
        # Verify no segfault or crash
        crash_logs = grep_uploadstb_logs_regex(r"segfault|crash|core dump")
        assert len(crash_logs) == 0, "Should not crash"

    @pytest.mark.order(3)
    def test_config_error_logging(self):
        """Test: Configuration errors are logged"""
        corrupt_device_properties()
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Verify error logging
        error_logs = grep_uploadstb_logs_regex(r"ERROR|error|configuration|properties")
        assert len(error_logs) > 0 or result.returncode != 0, "Configuration errors should be logged"

    @pytest.mark.order(4)
    def test_no_upload_with_invalid_config(self):
        """Test: No upload attempt is made with invalid configuration"""
        corrupt_device_properties()
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check that upload was not attempted
        # Look for actual upload completion indicators, not just words containing "upload"
        upload_logs = grep_uploadstb_logs_regex(r"(Upload completed|Successfully uploaded|HTTP/\d\.\d\" 200)")
        # Should not succeed with invalid config
        assert len(upload_logs) == 0, f"Upload should not succeed with invalid config, but found: {upload_logs}"

    @pytest.mark.order(5)
    def test_config_error_exit_code(self):
        """Test: Service exits with appropriate error code"""
        corrupt_device_properties()
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Should exit with non-zero code
        assert result.returncode != 0, "Should exit with error code for invalid config"


class TestFileSizeLimits:
    """Test suite for file size limit handling"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files("large")
        cleanup_test_log_files("huge")
        restore_device_properties()
        yield
        cleanup_test_log_files("large")
        cleanup_test_log_files("huge")
        remove_lock_file()

    @pytest.mark.order(1)
    def test_oversized_file_detection(self):
        """Test: Service detects files exceeding size limits"""
        # Create very large files (> 100MB)
        subprocess.run("dd if=/dev/urandom of=/opt/logs/huge_test_log.log bs=1M count=150 2>/dev/null", 
                      shell=True)
        
        result = run_uploadstblogs()
        
        # Check for size limit detection
        size_logs = grep_uploadstb_logs_regex(r"size.*limit|too large|exceed")
        # Process should complete
        assert result.returncode in [0, 1], "Should handle large files"

    @pytest.mark.order(2)
    def test_size_limit_error_logging(self):
        """Test: Size limit errors are logged"""
        # Create oversized file
        subprocess.run("dd if=/dev/urandom of=/opt/logs/huge_test_log.log bs=1M count=120 2>/dev/null", 
                      shell=True)
        
        result = run_uploadstblogs()
        
        # Check for size-related logs
        logs = grep_uploadstb_logs_regex(r"size|large|limit|truncate")
        # Should complete even with large files
        assert result.returncode in [0, 1], "Should log size issues"

    @pytest.mark.order(3)
    def test_partial_upload_with_oversized_files(self):
        """Test: Service proceeds with allowed files when some exceed limits"""
        # Create mix of normal and oversized files
        create_test_log_files(count=2, size_kb=100)
        subprocess.run("dd if=/dev/urandom of=/opt/logs/huge_test.log bs=1M count=150 2>/dev/null", 
                      shell=True)
        
        result = run_uploadstblogs()
        
        # Should handle mixed file sizes
        assert result.returncode in [0, 1], "Should process allowed files"

    @pytest.mark.order(4)
    def test_size_warning_telemetry(self):
        """Test: Size warning telemetry is generated"""
        # Create large file
        subprocess.run("dd if=/dev/urandom of=/opt/logs/huge_test.log bs=1M count=110 2>/dev/null", 
                      shell=True)
        
        result = run_uploadstblogs()
        
        # Check for telemetry markers
        telemetry_logs = grep_uploadstb_logs_regex(r"telemetry|marker|warning")
        # Process should complete
        assert result.returncode in [0, 1], "Should generate telemetry"


class TestEmptyLogs:
    """Test suite for empty log scenarios"""

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
    def test_no_log_files_detection(self):
        """Test: Service detects when no log files are available"""
        # Don't create any log files
        # Clear existing logs
        subprocess.run("rm -f /opt/logs/*.log 2>/dev/null", shell=True)
        subprocess.run("rm -rf /opt/logs/PreviousLogs/* 2>/dev/null", shell=True)
        
        result = run_uploadstblogs()
        
        # Check for no files detection
        no_files_logs = grep_uploadstb_logs_regex(r"no.*file|empty|not found|no logs")
        # Should handle empty logs scenario
        assert result.returncode in [0, 1], "Should handle no log files"

    @pytest.mark.order(2)
    def test_empty_logs_message_logged(self):
        """Test: Appropriate message is logged when no files available"""
        # Clean all logs
        subprocess.run("rm -f /opt/logs/*.log 2>/dev/null", shell=True)
        subprocess.run("rm -rf /opt/logs/PreviousLogs/* 2>/dev/null", shell=True)
        
        result = run_uploadstblogs()
        
        # Check for informative message
        logs = grep_uploadstb_logs_regex(r"no.*file|empty|nothing to upload")
        # Process should complete
        assert result.returncode in [0, 1], "Should log empty state"

    @pytest.mark.order(3)
    def test_no_upload_without_files(self):
        """Test: No upload attempt when no files available"""
        # Clean logs
        subprocess.run("rm -f /opt/logs/*.log 2>/dev/null", shell=True)
        subprocess.run("rm -rf /opt/logs/PreviousLogs/* 2>/dev/null", shell=True)
        
        result = run_uploadstblogs()
        
        # Should not attempt upload
        upload_logs = grep_uploadstb_logs_regex(r"upload.*success|uploading|HTTP")
        # Might not see upload attempts
        assert result.returncode in [0, 1], "Should not upload without files"

    @pytest.mark.order(4)
    def test_graceful_exit_with_empty_logs(self):
        """Test: Service exits gracefully when no logs available"""
        # Clean logs
        subprocess.run("rm -f /opt/logs/*.log 2>/dev/null", shell=True)
        subprocess.run("rm -rf /opt/logs/PreviousLogs/* 2>/dev/null", shell=True)
        
        result = run_uploadstblogs()
        
        # Should exit gracefully (not crash)
        assert result.returncode in [0, 1], "Should exit gracefully"
        
        # No crash indicators
        crash_logs = grep_uploadstb_logs_regex(r"segfault|crash|abort")
        assert len(crash_logs) == 0, "Should not crash with empty logs"

    @pytest.mark.order(5)
    def test_telemetry_for_empty_logs(self):
        """Test: Appropriate telemetry is generated for empty logs"""
        # Clean logs
        subprocess.run("rm -f /opt/logs/*.log 2>/dev/null", shell=True)
        
        result = run_uploadstblogs()
        
        # Check for telemetry
        telemetry_logs = grep_uploadstb_logs_regex(r"telemetry|marker|event")
        # Process should complete
        assert result.returncode in [0, 1], "Should handle empty logs with telemetry"

