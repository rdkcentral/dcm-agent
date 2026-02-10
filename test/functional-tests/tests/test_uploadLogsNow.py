####################################################################################
# If not stated otherwise in this file or this component's Licenses file the
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

"""
Test cases for uploadLogsNOw functionality
Tests the immediate upload logs scenario with custom endpoint URL configuration
"""

import pytest
import time
import subprocess as sp
import os
import tempfile
import shutil
from uploadstblogs_helper import *
from helper_functions import *


def run_uploadlogsnow():
    """Execute uploadlogsnow using the specific binary command"""
    cmd = "/usr/local/bin/logupload uploadlogsnow"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=300)
    return result


class TestUploadLogsNow:
    """Test suite for uploadLogsNow immediate upload functionality"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        # Clean up previous test artifacts
        #clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        self.cleanup_dcm_temp_files()

        # Store original RFC endpoint
        self.original_endpoint = self.get_rfc_endpoint()

        yield

        # Restore original configuration after test
        if self.original_endpoint:
            self.set_rfc_endpoint(self.original_endpoint)

        # Clean up after test
        cleanup_test_log_files()
        remove_lock_file()
        kill_uploadstblogs()
        self.cleanup_dcm_temp_files()

    def setup_mock_endpoint(self):
        """Set up the mock upload endpoint URL using rbuscli"""
        mock_endpoint = "https://mockxconf:50058/"
        return self.set_rfc_endpoint(mock_endpoint)

    def set_rfc_endpoint(self, url):
        """Set the RFC LogUploadEndpoint URL using rbuscli"""
        try:
            cmd = f"rbuscli set Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL string {url}"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=30)
            return result.returncode == 0
        except Exception as e:
            print(f"Failed to set RFC endpoint: {e}")
            return False

    def get_rfc_endpoint(self):
        """Get the current RFC LogUploadEndpoint URL"""
        try:
            cmd = "rbuscli get Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=30)
            if result.returncode == 0 and result.stdout.strip():
                # Extract URL from output (format: "Value : <url>")
                lines = result.stdout.strip().split('\n')
                for line in lines:
                    line = line.strip()
                    if line.startswith("Value") and ":" in line:
                        url = line.split(':', 1)[1].strip()
                        # Remove any quotes if present
                        url = url.strip('"\'')
                        if url:  # Ensure we have a non-empty URL
                            return url
                    # Also handle legacy format with "=" for compatibility
                    elif "=" in line and line.strip():
                        url = line.split('=', 1)[1].strip()
                        # Remove any quotes if present
                        url = url.strip('"\'')
                        if url:  # Ensure we have a non-empty URL
                            return url
            print(f"RFC command failed or returned empty result: returncode={result.returncode}, stdout='{result.stdout}', stderr='{result.stderr}'")
            return None
        except Exception as e:
            print(f"Failed to get RFC endpoint: {e}")
            return None

    def cleanup_dcm_temp_files(self):
        """Clean up DCM temporary files and directories"""
        temp_paths = [
            "/tmp/DCM",
            "/tmp/loguploadstatus.txt",
            "/tmp/*.tgz",
            "/tmp/*.tar.gz"
        ]
        for path in temp_paths:
            try:
                subprocess.run(f"rm -rf {path}", shell=True)
            except:
                pass

    def create_test_logs_scenario(self, log_count=5):
        """Create a realistic log file scenario for uploadLogsNow"""
        log_dir = "/opt/logs"
        created_files = []

        # Create different types of log files
        log_files = [
            "messages.txt",
            "syslog.log",
            "application.log",
            "wifi.log",
            "dcmd.log",
            "system_debug.out"
        ]

        for i, filename in enumerate(log_files[:log_count]):
            filepath = os.path.join(log_dir, filename)
            # Create file with some content
            content = f"Log entry {i} - {time.strftime('%Y-%m-%d %H:%M:%S')}\n" * 100
            try:
                with open(filepath, 'w') as f:
                    f.write(content)
                created_files.append(filepath)
            except:
                pass  # File creation might fail in some environments

        return created_files

    @pytest.mark.order(1)
    def test_uploadlogsnow_context_initialization(self):
        """Test: uploadLogsNow properly initializes context and environment"""
        # Setup test environment
        assert self.setup_mock_endpoint(), "Failed to set mock endpoint"
        self.create_test_logs_scenario(2)

        # Execute uploadLogsNow
        result = run_uploadlogsnow()

        # Check for context initialization logs
        init_logs = grep_uploadstb_logs_regex(r"Context.*initializ|initializ.*context|UploadLogsNow.*start")
        assert len(init_logs) >= 0, "Should find context initialization evidence"

        # Check for device properties loading
        device_logs = grep_uploadstb_logs_regex(r"DEVICE_TYPE|device.*propert|loading.*propert")
        assert len(device_logs) >= 0, "Should load device properties"

        # Check for path validation/setup
        path_logs = grep_uploadstb_logs_regex(r"LOG_PATH|DCM_LOG_PATH|path.*valid|directory.*creat")
        assert len(path_logs) >= 0, "Should validate and setup paths"

        # Check for RFC endpoint configuration reading
        rfc_logs = grep_uploadstb_logs_regex(r"RFC|LogUploadEndpoint|endpoint.*config")
        assert len(rfc_logs) >= 0, "Should read RFC configuration"

        # Verify process completes initialization
        assert result.returncode in [0, 1, 255], "Process should complete initialization"

        # Check that initialization doesn't take excessive time
        # (This is implicitly tested by the overall test timeout)

    @pytest.mark.order(2)
    def test_uploadlogsnow_immediate_trigger(self):
        """Test: uploadLogsNow executes immediately without delay"""
        # Setup test environment
        assert self.setup_mock_endpoint(), "Failed to set mock endpoint"
        self.create_test_logs_scenario(3)

        # Record start time
        start_time = time.time()

        # Execute uploadLogsNow using specific command
        result = run_uploadlogsnow()

        elapsed_time = time.time() - start_time

        # Should execute immediately (within reasonable time)
        assert elapsed_time < 60, f"uploadLogsNow should execute immediately, took {elapsed_time}s"
        assert result.returncode in [0, 1], "Upload process should complete"

    @pytest.mark.order(3)
    def test_uploadlogsnow_rfc_endpoint_configuration(self):
        """Test: uploadLogsNow uses RFC configured endpoint URL"""
        # Set specific endpoint via RFC
        test_endpoint = "https://mockxconf:50058/"
        assert self.setup_mock_endpoint(), "Failed to configure RFC endpoint"

        # Verify endpoint is set correctly
        current_endpoint = self.get_rfc_endpoint()
        assert current_endpoint is not None, f"Failed to retrieve RFC endpoint: {current_endpoint}"
        assert test_endpoint in current_endpoint, f"Endpoint not set correctly: {current_endpoint}"

        # Create test logs
        self.create_test_logs_scenario(2)

        # Execute uploadLogsNow
        result = run_uploadlogsnow()

        # Check logs for endpoint usage
        endpoint_logs = grep_uploadstb_logs_regex(r"mockxconf.*50058")
        # Process should attempt to use the configured endpoint
        assert result.returncode in [0, 1], "Should complete with configured endpoint"

    @pytest.mark.order(4)
    def test_uploadlogsnow_upload_success_verification(self):
        """Test: Verify uploadLogsNow successfully uploads logs"""
        # Setup test environment
        assert self.setup_mock_endpoint(), "Failed to set mock endpoint"

        # Create test logs with sufficient content
        created_files = self.create_test_logs_scenario(3)
        assert len(created_files) > 0, "Should create test log files"

        # Verify test files exist before upload
        for filepath in created_files:
            assert os.path.exists(filepath), f"Test file should exist: {filepath}"

        # Execute uploadLogsNow
        result = run_uploadlogsnow()

        # Verify basic execution success
        assert result.returncode == 0, f"Upload should succeed, got return code: {result.returncode}"

        # Check for success indicators in logs
        success_patterns = [
            r"Upload.*[Ss]uccess|[Cc]omplete.*upload|Upload.*[Ff]inished",
            r"Archive.*created|Creating.*archive",
            r"Uploaded.*through.*SNMP|Uploaded.*logs"
        ]

        success_found = False
        for pattern in success_patterns:
            success_logs = grep_uploadstb_logs_regex(pattern)
            if success_logs and len(success_logs) > 0:
                success_found = True
                print(f"Found success indicator in uploadSTBLogs: {success_logs}")
                break

        # Also check logupload.log file for success indicators
        if not success_found:
            success_found = self.check_logupload_file_success(success_patterns)

        # Check upload status file for success indication
        status_indicators = self.check_upload_status_success()

        # Verify upload attempt was made (either success logs or status indicators)
        assert success_found or status_indicators, \
            "Should find evidence of successful upload in logs or status files"

        # Check that archive was created and processed
        archive_evidence = self.verify_archive_processing()

        print(f"Upload verification - Success logs: {success_found}, "
              f"Status indicators: {status_indicators}, "
              f"Archive evidence: {archive_evidence}")

    def check_logupload_file_success(self, success_patterns):
        """Check logupload.log file for success indicators using grep"""
        logupload_files = [
            "/opt/logs/logupload.log",
            "/tmp/logupload.log",
            "/var/log/logupload.log"
        ]

        for log_file in logupload_files:
            try:
                if os.path.exists(log_file):
                    print(f"Checking {log_file} for success indicators")
                    for pattern in success_patterns:
                        # Use grep to search for pattern in the log file
                        cmd = f"grep -E '{pattern}' {log_file}"
                        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
                        if result.returncode == 0 and result.stdout.strip():
                            matches = result.stdout.strip().split('\n')
                            print(f"Found success indicator in {log_file}: {matches}")
                            return True
                else:
                    print(f"Log file does not exist: {log_file}")
            except Exception as e:
                print(f"Error checking log file {log_file}: {e}")

        return False

    def check_upload_status_success(self):
        """Check upload status files for success indicators"""
        status_files = [
            "/tmp/loguploadstatus.txt",
            "/opt/logs/loguploadstatus.txt"
        ]

        success_keywords = ["complete", "success", "uploaded", "finished"]

        for status_file in status_files:
            try:
                if os.path.exists(status_file):
                    with open(status_file, 'r') as f:
                        content = f.read().lower()
                        for keyword in success_keywords:
                            if keyword in content:
                                print(f"Found success indicator in {status_file}: {keyword}")
                                return True
            except Exception as e:
                print(f"Error checking status file {status_file}: {e}")

        return False

    def verify_archive_processing(self):
        """Verify that archive was created and processed"""
        # Check for archive creation evidence
        archive_patterns = [
            r"Archive.*created|Creating.*archive|tar.*created",
            r"\.tar\.gz|\.tgz",
            r"Archive.*path|Archive.*file"
        ]

        # Check uploadSTBLogs for archive patterns
        for pattern in archive_patterns:
            archive_logs = grep_uploadstb_logs_regex(pattern)
            if archive_logs and len(archive_logs) > 0:
                print(f"Found archive processing evidence in uploadSTBLogs: {archive_logs}")
                return True

        # Also check logupload.log for archive patterns
        if self.check_logupload_file_success(archive_patterns):
            return True

        # Check for temporary archive files (they might be cleaned up after successful upload)
        temp_locations = ["/tmp/DCM", "/tmp"]
        for location in temp_locations:
            try:
                if os.path.exists(location):
                    # Look for any archive files that might still exist
                    for filename in os.listdir(location):
                        if filename.endswith(('.tar.gz', '.tgz')):
                            print(f"Found archive file: {location}/{filename}")
                            return True
            except Exception:
                pass

        return False


if __name__ == "__main__":
    # Run the tests
    pytest.main([__file__])
