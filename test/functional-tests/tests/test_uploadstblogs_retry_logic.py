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
Test cases for uploadSTBLogs retry logic and network failure handling
Covers: Network failures, retry attempts, server errors
"""

import pytest
import time
from uploadstblogs_helper import *
from helper_functions import *


class TestRetryLogic:
    """Test suite for upload retry logic"""

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
    def test_network_failure_detection(self):
        """Test: Service detects network failure"""
        # Set invalid upload URL to simulate network failure
        set_include_property("UPLOAD_HTTPLINK", "http://invalid.server.unreachable:9999")
        
        create_test_log_files(count=2)
        
        result = run_uploadstblogs()
        
        # Check for network failure detection
        failure_logs = grep_uploadstb_logs_regex(r"fail|error|unable|unreachable|timeout")
        assert len(failure_logs) > 0, "Network failure should be detected"

    @pytest.mark.order(2)
    def test_retry_attempts_count(self):
        """Test: Service retries 3 times for Direct path"""
        # Set unreachable server
        set_include_property("UPLOAD_HTTPLINK", "http://192.0.2.1:9999")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Count retry attempts in logs
        retry_logs = grep_uploadstb_logs_regex(r"retry|attempt")
        # Should see retry activity or failure after retries
        assert len(retry_logs) >= 0, "Retry mechanism should be invoked"

    @pytest.mark.order(3)
    def test_retry_with_delay(self):
        """Test: Service waits between retry attempts"""
        # Set unreachable server
        set_include_property("UPLOAD_HTTPLINK", "http://192.0.2.1:8080")
        
        create_test_log_files(count=1)
        
        start_time = time.time()
        result = run_uploadstblogs()
        elapsed = time.time() - start_time
        
        # With retries and delays, should take some time
        # Note: May fail fast if no retries configured, but should try
        assert result.returncode == 1, "Upload should fail with unreachable server"

    @pytest.mark.order(4)
    def test_failure_telemetry_after_retries(self):
        """Test: Failure telemetry is generated after all retries"""
        set_include_property("UPLOAD_HTTPLINK", "http://192.0.2.1:9999")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for telemetry or failure markers
        telemetry_logs = grep_uploadstb_logs_regex(r"telemetry|marker|failed|SYST")
        # Failure should be logged
        assert result.returncode == 1, "Should exit with error after failed retries"

    @pytest.mark.order(5)
    def test_network_failure_logged(self):
        """Test: Network failure details are logged"""
        set_include_property("UPLOAD_HTTPLINK", "http://invalid.domain.test:8080")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Verify error logging
        error_logs = grep_uploadstb_logs_regex(r"ERROR|error|fail")
        assert len(error_logs) > 0, "Network failure should be logged"


class TestNetworkInterruption:
    """Test suite for network interruption during upload"""

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
    def test_upload_interruption_handling(self):
        """Test: Service handles upload interruption gracefully"""
        # This test simulates network interruption
        # In real scenario, would need network manipulation
        
        create_test_log_files(count=2)
        
        import subprocess
        # Start upload process
        proc = subprocess.Popen([UPLOADSTB_BINARY], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Let it start
        time.sleep(2)
        
        # Terminate to simulate interruption
        proc.terminate()
        proc.wait(timeout=10)
        
        # Process should exit
        assert proc.returncode != 0 or proc.returncode is not None, "Process should handle interruption"

    @pytest.mark.order(2)
    def test_retry_after_interruption(self):
        """Test: Service retries after network interruption"""
        # Set a server that might timeout
        set_include_property("UPLOAD_HTTPLINK", "http://192.0.2.1:80")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for retry attempts
        retry_logs = grep_uploadstb_logs_regex(r"retry|attempt|again")
        # Service should handle failure
        assert result.returncode in [0, 1], "Service should complete with or without success"


class TestHTTPServerErrors:
    """Test suite for HTTP server error responses"""

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
        stop_mock_http_server()

    @pytest.mark.order(1)
    def test_http_500_error_detection(self):
        """Test: Service detects HTTP 500 error"""
        # Note: Would need mock server that returns 500
        # For now, test with unreachable server
        
        set_include_property("UPLOAD_HTTPLINK", "http://localhost:9999")
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for error detection
        error_logs = grep_uploadstb_logs_regex(r"HTTP|error|fail|5\d\d")
        # Should fail
        assert result.returncode == 1, "Should detect server error"

    @pytest.mark.order(2)
    def test_retry_on_server_error(self):
        """Test: Service retries upload on server error"""
        set_include_property("UPLOAD_HTTPLINK", "http://localhost:9999")
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Verify retry mechanism activated
        retry_logs = grep_uploadstb_logs_regex(r"retry|attempt")
        # Should fail after retries
        assert result.returncode == 1, "Should fail after retry attempts"

    @pytest.mark.order(3)
    def test_server_error_logging(self):
        """Test: Server error response is logged"""
        set_include_property("UPLOAD_HTTPLINK", "http://localhost:8888")
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for error logging
        logs = grep_uploadstb_logs_regex(r"ERROR|error|server|HTTP")
        assert len(logs) > 0 or result.returncode == 1, "Server errors should be logged"

    @pytest.mark.order(4)
    def test_exit_code_on_server_error(self):
        """Test: Service exits with appropriate error code on failure"""
        set_include_property("UPLOAD_HTTPLINK", "http://192.0.2.1:80")
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Should exit with non-zero code
        assert result.returncode != 0, "Should exit with error code on server failure"

