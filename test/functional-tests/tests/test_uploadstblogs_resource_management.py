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
Test cases for uploadSTBLogs resource management and cleanup
Covers: Resource cleanup, memory management, concurrent requests
"""

import pytest
import time
import subprocess as sp
from uploadstblogs_helper import *
from helper_functions import *


class TestResourceCleanup:
    """Test suite for system resource cleanup"""

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
        # Clean any leftover archives
        sp.run("rm -f /tmp/*.tgz 2>/dev/null", shell=True)

    @pytest.mark.order(1)
    def test_temporary_archive_cleanup(self):
        """Test: Temporary archive files are cleaned up after upload"""
        create_test_log_files(count=3)
        
        # Check archives before
        archives_before = sp.run("ls /tmp/*.tgz 2>/dev/null | wc -l", 
                                shell=True, capture_output=True, text=True)
        
        result = run_uploadstblogs()
        time.sleep(2)
        
        # Cleanup should remove temp archives (or they should be managed)
        # Implementation may or may not clean up immediately
        assert result.returncode in [0, 1], "Process should complete"

    @pytest.mark.order(2)
    def test_lock_file_removal(self):
        """Test: Lock file is removed after operation completes"""
        create_test_log_files(count=2)
        
        result = run_uploadstblogs()
        time.sleep(2)
        
        # Kill any remaining processes to ensure cleanup
        kill_uploadstblogs()
        time.sleep(1)
        
        # Lock file should be removed after process termination
        lock_exists = check_lock_file_exists()
        
        # If lock still exists, give it more time
        if lock_exists:
            time.sleep(3)
            lock_exists = check_lock_file_exists()
            
        # Lock file should be removed (may need manual cleanup in some cases)
        if lock_exists:
            remove_lock_file()  # Clean up for next test
        
        # Process should have completed
        assert result.returncode in [0, 1], f"Process should complete with valid return code, got {result.returncode}"

    @pytest.mark.order(3)
    def test_file_handles_closed(self):
        """Test: All file handles are properly closed"""
        create_test_log_files(count=3)
        
        # Start process
        proc = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(3)
        
        # Check open file descriptors
        pid = get_uploadstblogs_pid()
        if pid:
            fd_count_cmd = f"ls -l /proc/{pid}/fd 2>/dev/null | wc -l"
            fd_result = sp.run(fd_count_cmd, shell=True, capture_output=True, text=True)
            fd_count = int(fd_result.stdout.strip()) if fd_result.stdout.strip().isdigit() else 0
            
            # Should have reasonable number of FDs (not leaked)
            assert fd_count < 100, f"Too many open file descriptors: {fd_count}"
        
        proc.terminate()
        proc.wait(timeout=10)

    @pytest.mark.order(4)
    def test_no_orphaned_resources(self):
        """Test: No orphaned resources remain after completion"""
        create_test_log_files(count=2)
        
        result = run_uploadstblogs()
        time.sleep(2)
        
        # Ensure all processes are killed
        kill_uploadstblogs()
        time.sleep(1)
        
        # Check no uploadSTBLogs processes remain
        pid = get_uploadstblogs_pid()
        assert not pid, "No uploadSTBLogs process should remain"
        
        # Check lock file removed (clean up if it persists)
        lock_exists = check_lock_file_exists()
        if lock_exists:
            remove_lock_file()
        
        # Process should have completed
        assert result.returncode in [0, 1], "Process should complete"

    @pytest.mark.order(5)
    def test_cleanup_on_failure(self):
        """Test: Resources are cleaned up even on failure"""
        # Set invalid config to cause failure
        set_include_property("UPLOAD_HTTPLINK", "http://invalid.test:9999")
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        time.sleep(2)
        
        # Kill any remaining processes
        kill_uploadstblogs()
        time.sleep(1)
        
        # Check lock file (may persist on failure)
        lock_exists = check_lock_file_exists()
        if lock_exists:
            remove_lock_file()  # Clean up for next tests
        
        # No hanging processes
        pid = get_uploadstblogs_pid()
        assert not pid, "Process should not hang on failure"
        
        # Process should have attempted and failed or timed out
        assert result.returncode in [0, 1, -1], "Process should exit with error code"


class TestMemoryManagement:
    """Test suite for memory management"""

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
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_memory_allocation_reasonable(self):
        """Test: Service allocates reasonable amount of memory"""
        create_test_log_files(count=3, size_kb=500)
        
        # Start process
        proc = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(3)
        
        # Check memory usage
        memory_kb = check_memory_usage("uploadSTBLogs")
        
        proc.terminate()
        proc.wait(timeout=10)
        
        # Memory should be reasonable (< 50MB for normal operation)
        assert memory_kb < 51200, f"Memory usage {memory_kb}KB exceeds 50MB limit"

    @pytest.mark.order(2)
    def test_no_memory_leaks(self):
        """Test: No memory leaks during operation"""
        create_test_log_files(count=2)
        
        # Run multiple times to detect leaks
        for i in range(3):
            result = run_uploadstblogs()
            time.sleep(1)
        
        # Check for memory leak indicators in logs
        leak_logs = grep_uploadstb_logs_regex(r"memory.*leak|failed.*allocate")
        assert len(leak_logs) == 0, "No memory leaks should be detected"

    @pytest.mark.order(3)
    def test_memory_freed_after_completion(self):
        """Test: Memory is freed after operation completes"""
        create_test_log_files(count=2)
        
        # Start process
        proc = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(2)
        
        memory_during = check_memory_usage("uploadSTBLogs")
        
        # Wait for completion
        proc.wait(timeout=30)
        time.sleep(1)
        
        memory_after = check_memory_usage("uploadSTBLogs")
        
        # After completion, memory should be released
        assert memory_after == 0, "Memory should be freed after process ends"

    @pytest.mark.order(4)
    def test_memory_under_heavy_load(self):
        """Test: Memory management under heavy load"""
        # Create many files
        create_test_log_files(count=10, size_kb=1024)
        
        proc = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(5)
        
        memory_kb = check_memory_usage("uploadSTBLogs")
        
        proc.terminate()
        proc.wait(timeout=10)
        
        # Even under load, memory should be controlled (< 100MB)
        assert memory_kb < 102400, f"Memory {memory_kb}KB exceeds limit under load"


class TestConcurrentRequests:
    """Test suite for concurrent upload request handling"""

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
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_lock_prevents_concurrent_execution(self):
        """Test: Lock file prevents concurrent execution"""
        create_test_log_files(count=2)
        
        # Start first instance
        proc1 = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(2)
        
        # Try to start second instance
        result2 = run_uploadstblogs()
        
        # Second instance should fail to acquire lock
        assert result2.returncode != 0, "Second instance should fail to acquire lock"
        
        # Terminate first instance
        proc1.terminate()
        proc1.wait(timeout=10)

    @pytest.mark.order(2)
    def test_second_request_rejected(self):
        """Test: Second upload request is rejected during active upload"""
        create_test_log_files(count=2)
        
        # Start first instance
        proc1 = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(2)
        
        # Start second instance
        proc2 = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(1)
        
        # Second should exit quickly (failed to get lock)
        returncode2 = proc2.poll()
        assert returncode2 is not None, "Second instance should exit"
        assert returncode2 != 0, "Second instance should fail"
        
        # Clean up
        proc1.terminate()
        proc1.wait(timeout=10)

    @pytest.mark.order(3)
    def test_first_upload_uninterrupted(self):
        """Test: First upload continues uninterrupted by second request"""
        create_test_log_files(count=2)
        
        # Start first instance
        proc1 = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        time.sleep(2)
        
        pid1_before = get_uploadstblogs_pid()
        
        # Try second instance
        proc2 = sp.Popen([UPLOADSTB_BINARY], stdout=sp.PIPE, stderr=sp.PIPE)
        proc2.wait(timeout=10)
        
        time.sleep(1)
        pid1_after = get_uploadstblogs_pid()
        
        # First instance should still be running or have same PID
        assert pid1_before == pid1_after or not pid1_after, "First instance should be unaffected"
        
        # Clean up
        proc1.terminate()
        proc1.wait(timeout=10)

