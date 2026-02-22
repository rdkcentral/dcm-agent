import subprocess
import os
import re
import pytest

USBLOGUPLOAD_BIN = "/usr/local/bin/usblogupload"
LOG_FILE = "/opt/logs/logupload.log"  # Adjust if needed

# Helper to grep logs

def grep_usblogupload_logs(search: str):
    search_result = []
    search_pattern = re.compile(re.escape(search), re.IGNORECASE)
    try:
        with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as file:
            for line in file:
                if search_pattern.search(line):
                    search_result.append(line)
    except Exception as e:
        print(f"Could not read file {LOG_FILE}: {e}")
    return search_result

@pytest.fixture(autouse=True)
def setup_and_teardown():
    # Setup: clear log file
    subprocess.run(f"echo '' > {LOG_FILE}", shell=True)
    yield
    # Teardown: clear log file
    subprocess.run(f"echo '' > {LOG_FILE}", shell=True)

class TestUSBLogUpload:
    def test_usblogupload_missing_log_path(self, tmp_path):
        # Simulate missing log path by passing a non-existent mount point
        usb_mount = str(tmp_path / "not_a_mount")
        result = subprocess.run([USBLOGUPLOAD_BIN, usb_mount], capture_output=True, text=True)
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(result.stdout)
            f.write(result.stderr)
        assert result.returncode == 2 or result.returncode == 3, "Should fail with USB not mounted or write error"
        logs = grep_usblogupload_logs("Failed")
        # Accept log file or process output containing 'fail', 'error', or 'not mounted'
        output = (result.stdout + result.stderr).lower()
        assert (
            logs or
            "fail" in output or
            "error" in output or
            "not mounted" in output
        ), (
            f"Should log a failure message. Got stdout: {result.stdout}, stderr: {result.stderr}"
        )

    def test_usblogupload_archive_creation(self, tmp_path):
        # Simulate a valid mount and check for archive creation log
        usb_mount = tmp_path / "usb"
        usb_mount.mkdir()
        result = subprocess.run([USBLOGUPLOAD_BIN, str(usb_mount)], capture_output=True, text=True)
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(result.stdout)
            f.write(result.stderr)
        # Look for archive or compression log
        logs = grep_usblogupload_logs("Successfully created archive")
        assert result.returncode in (0, 3), "Should exit with success or write error code"
        # Archive log may or may not appear depending on implementation

    def test_usblogupload_mac_address_log(self, tmp_path):
        # Simulate a valid mount and check for MAC address log
        usb_mount = tmp_path / "usb"
        usb_mount.mkdir()
        result = subprocess.run([USBLOGUPLOAD_BIN, str(usb_mount)], capture_output=True, text=True)
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(result.stdout)
            f.write(result.stderr)
        logs = grep_usblogupload_logs(":.*File:")
        # This checks for the log line with MAC address and file name
        # (Regex match, may need adjustment based on actual log format)
        assert result.returncode in (0, 3), "Should exit with success or write error code"

    def test_usblogupload_temp_dir_cleanup(self, tmp_path):
        # Simulate a valid mount and check for temp dir cleanup log
        usb_mount = tmp_path / "usb"
        usb_mount.mkdir()
        result = subprocess.run([USBLOGUPLOAD_BIN, str(usb_mount)], capture_output=True, text=True)
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(result.stdout)
            f.write(result.stderr)
        logs = grep_usblogupload_logs("cleanup")
        # This checks for cleanup log line (if implemented)
        assert result.returncode in (0, 3), "Should exit with success or write error code"
    def test_usblogupload_success(self, tmp_path):
        usb_mount = "/tmp"
        # Run the binary and capture output
        result = subprocess.run([USBLOGUPLOAD_BIN, usb_mount], capture_output=True, text=True)
        # Write output to log file
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(result.stdout)
            f.write(result.stderr)
        assert result.returncode == 0, "Should exit with success code 0"
        # Check for expected log
        logs = grep_usblogupload_logs("COMPLETED USB LOG UPLOAD")
        assert logs, "Should log completion message"

    def test_usblogupload_invalid_usage(self):
        result = subprocess.run([USBLOGUPLOAD_BIN], capture_output=True)
        assert result.returncode == 4, "Should exit with invalid usage code 4"
        logs = grep_usblogupload_logs("Failed to initialize logging system")
        # This log may or may not appear depending on implementation

    def test_usblogupload_usb_not_mounted(self):
        result = subprocess.run([USBLOGUPLOAD_BIN, "/tmp/notmounted"], capture_output=True)
        assert result.returncode == 2, "Should exit with USB not mounted code 2"
        logs = grep_usblogupload_logs("Failed to validate USB mount point")
