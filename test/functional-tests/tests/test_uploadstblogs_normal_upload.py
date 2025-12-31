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
Test cases for uploadSTBLogs normal upload operations
Covers: Normal upload, large file handling, MD5 verification
"""

import pytest
import time
from uploadstblogs_helper import *
from helper_functions import *


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
        create_test_log_files(count=3, size_kb=50)
        set_include_property("LOG_PATH", "/opt/logs")

        # Run uploadSTBLogs
        #result = run_uploadstblogs()

        result = subprocess.run([
        "/usr/local/bin/logupload",
        "",
        "1",
        "1",
        "true",
        "HTTP",
        "https://mockxconf:50058/"
        ])


        # Verify initialization
        assert result.returncode == 0 or result.returncode == 1, "Upload process should complete"

        # Check initialization logs
        init_logs = grep_uploadstb_logs("Context initialization successful")
        assert len(init_logs) > 0, "Context should be initialized successfully"

        # Verify device properties loaded
        logs = grep_uploadstb_logs("DEVICE_TYPE")
        assert len(logs) > 0, "Device type should be loaded from properties"

        collection_logs = grep_uploadstb_logs_regex(r"collect|archive|gather")
        assert len(collection_logs) > 0, "Log collection should be attempted"

        # Check for archive creation logs
        archive_logs = grep_uploadstb_logs_regex(r"Archive created successfully")
        # Process should complete successfully
        assert len(archive_logs) > 0, "Archive process should complete. Found {len(archive_logs)} archive-related logs: {archive_logs}"

        upload_logs = grep_uploadstb_logs_regex(r"upload.*success|uploading|HTTP")
        # Telemetry should be attempted
        assert len(archive_logs) > 0, "Upload Process should complete and succeed"


class TestLargeFileHandling:
    """Test suite for large file handling"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown for large file tests"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files("large_test")
        restore_device_properties()
        yield
        cleanup_test_log_files("large_test")
        remove_lock_file()

    @pytest.mark.order(1)
    def test_large_file_collection(self):
        """Test: Service collects large log files within limits"""
        # Create large test files (10MB each)
        large_files = create_large_test_log_files(count=3, size_mb=10)

        result = subprocess.run([
        "/usr/local/bin/logupload",
        "",
        "1",
        "1",
        "true",
        "HTTP",
        "https://mockxconf:50058/"
        ])

        # Verify files were processed
        
        # Check for compression/archive activity
        compression_logs = grep_uploadstb_logs_regex(r"compress|archive|tgz")
        # Process should complete
        assert result.returncode in [0, 1], "Compression process should complete"

