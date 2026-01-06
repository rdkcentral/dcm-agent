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

import subprocess
import os
import time
import re
import json
import hashlib

# Extended helper functions for uploadSTBLogs testing

UPLOADSTB_LOG = "/opt/logs/logupload.log.0"
DCMD_LOG = "/opt/logs/dcmd.log.0"
UPLOADSTB_BINARY = "/usr/local/bin/logupload"
LOCK_FILE = "/tmp/.log-upload.lock"
DEVICE_PROPERTIES = "/etc/device.properties"
INCLUDE_PROPERTIES = "/etc/include.properties"

def run_uploadstblogs(args=""):
    """Execute uploadSTBLogs with optional arguments"""
    cmd = f"{UPLOADSTB_BINARY} {args}" if args else UPLOADSTB_BINARY
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=300)
    return result

def grep_uploadstb_logs(search_pattern, log_file=UPLOADSTB_LOG):
    """Search for pattern in uploadSTBLogs log file"""
    search_result = []
    pattern = re.compile(re.escape(search_pattern), re.IGNORECASE)
    try:
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as file:
            for line_number, line in enumerate(file, start=1):
                if pattern.search(line):
                    search_result.append(line.strip())
    except Exception as e:
        print(f"Could not read file {log_file}: {e}")
    return search_result

def grep_uploadstb_logs_regex(regex_pattern, log_file=UPLOADSTB_LOG):
    """Search using regex pattern in uploadSTBLogs log file"""
    search_result = []
    pattern = re.compile(regex_pattern, re.IGNORECASE)
    try:
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as file:
            for line in file:
                if pattern.search(line):
                    search_result.append(line.strip())
    except Exception as e:
        print(f"Could not read file {log_file}: {e}")
    return search_result

def clear_uploadstb_logs():
    """Clear uploadSTBLogs log file"""
    try:
        subprocess.run(f"echo '' > {UPLOADSTB_LOG}", shell=True)
        return True
    except:
        return False

def check_lock_file_exists():
    """Check if upload lock file exists"""
    return os.path.exists(LOCK_FILE)

def remove_lock_file():
    """Remove upload lock file"""
    try:
        if os.path.exists(LOCK_FILE):
            os.remove(LOCK_FILE)
        return True
    except:
        return False

def get_uploadstblogs_pid():
    """Get PID of running uploadSTBLogs process"""
    result = subprocess.run("pidof uploadSTBLogs", shell=True, capture_output=True, text=True)
    return result.stdout.strip()

def kill_uploadstblogs(signal=9):
    """Kill uploadSTBLogs process"""
    pid = get_uploadstblogs_pid()
    if pid:
        subprocess.run(f"kill -{signal} {pid}", shell=True)
        time.sleep(1)
        return True
    return False

def create_test_log_files(count=5, size_kb=100):
    """Create test log files in /opt/logs"""
    log_dir = "/opt/logs/PreviousLogs"
    created_files = []
    for i in range(count):
        filename = f"{log_dir}/test_log_{i}.log"
        # Create file with specified size
        subprocess.run(f"dd if=/dev/urandom of={filename} bs=1024 count={size_kb} 2>/dev/null", shell=True)
        created_files.append(filename)
    return created_files

def create_large_test_log_files(count=3, size_mb=10):
    """Create large test log files"""
    log_dir = "/opt/logs"
    created_files = []
    for i in range(count):
        filename = f"{log_dir}/large_test_log_{i}.log"
        subprocess.run(f"dd if=/dev/urandom of={filename} bs=1M count={size_mb} 2>/dev/null", shell=True)
        created_files.append(filename)
    return created_files

def cleanup_test_log_files(pattern="test_log"):
    """Remove test log files"""
    subprocess.run(f"rm -f /opt/logs/{pattern}*.log", shell=True)

def check_archive_exists(pattern="*.tgz"):
    """Check if archive file exists"""
    result = subprocess.run(f"ls /tmp/{pattern} 2>/dev/null", shell=True, capture_output=True, text=True)
    return bool(result.stdout.strip())

def get_archive_path():
    """Get path to created archive file"""
    result = subprocess.run("ls -t /tmp/*.tgz 2>/dev/null | head -1", shell=True, capture_output=True, text=True)
    return result.stdout.strip()

def calculate_file_md5(filepath):
    """Calculate MD5 checksum of file"""
    try:
        md5_hash = hashlib.md5()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                md5_hash.update(chunk)
        return md5_hash.hexdigest()
    except:
        return None

def check_http_server_reachable(url="http://localhost:8080"):
    """Check if HTTP server is reachable"""
    result = subprocess.run(f"curl -s -o /dev/null -w '%{{http_code}}' --max-time 5 {url}", 
                          shell=True, capture_output=True, text=True)
    return result.stdout.strip() != "000"

def start_mock_http_server(port=8080):
    """Start a simple mock HTTP server for testing"""
    cmd = f"python3 -m http.server {port} --directory /tmp > /dev/null 2>&1 &"
    subprocess.run(cmd, shell=True)
    time.sleep(2)
    return True

def stop_mock_http_server():
    """Stop mock HTTP server"""
    subprocess.run("pkill -f 'python3 -m http.server'", shell=True)
    time.sleep(1)

def set_device_property(key, value):
    """Set a device property"""
    # Remove existing entry
    subprocess.run(f"sed -i '/^{key}=/d' {DEVICE_PROPERTIES}", shell=True)
    # Add new entry
    subprocess.run(f"echo '{key}={value}' >> {DEVICE_PROPERTIES}", shell=True)

def get_device_property(key):
    """Get a device property value"""
    result = subprocess.run(f"grep '^{key}=' {DEVICE_PROPERTIES} | cut -d'=' -f2", 
                          shell=True, capture_output=True, text=True)
    return result.stdout.strip()

def set_include_property(key, value):
    """Set an include property"""
    subprocess.run(f"sed -i '/^{key}=/d' {INCLUDE_PROPERTIES}", shell=True)
    subprocess.run(f"echo '{key}={value}' >> {INCLUDE_PROPERTIES}", shell=True)

def corrupt_device_properties():
    """Corrupt device properties file"""
    subprocess.run(f"echo 'INVALID@@@SYNTAX###' > {DEVICE_PROPERTIES}", shell=True)

def restore_device_properties():
    """Restore device properties to valid state"""
    subprocess.run(f"sed -i '/INVALID/d' {DEVICE_PROPERTIES}", shell=True)
    set_device_property("DEVICE_TYPE", "mediaclient")
    set_device_property("BUILD_TYPE", "dev")

def check_telemetry_marker(marker):
    """Check if telemetry marker was sent"""
    # This would check T2 logs or telemetry output
    result = subprocess.run(f"grep '{marker}' /opt/logs/telemetry*.log 2>/dev/null", 
                          shell=True, capture_output=True, text=True)
    return bool(result.stdout.strip())

def setup_mtls_certificates():
    """Setup mTLS certificates for testing"""
    cert_dir = "/tmp/certs"
    subprocess.run(f"mkdir -p {cert_dir}", shell=True)
    
    # Generate self-signed test certificates
    subprocess.run(f"""
        openssl req -x509 -newkey rsa:2048 -keyout {cert_dir}/client.key \
        -out {cert_dir}/client.crt -days 365 -nodes \
        -subj "/C=US/ST=Test/L=Test/O=Test/CN=test" 2>/dev/null
    """, shell=True)
    
    subprocess.run(f"""
        openssl req -x509 -newkey rsa:2048 -keyout {cert_dir}/ca.key \
        -out {cert_dir}/ca.crt -days 365 -nodes \
        -subj "/C=US/ST=Test/L=Test/O=TestCA/CN=testca" 2>/dev/null
    """, shell=True)
    
    return cert_dir

def cleanup_mtls_certificates():
    """Cleanup test certificates"""
    subprocess.run("rm -rf /tmp/certs", shell=True)

def check_memory_usage(process_name="uploadSTBLogs"):
    """Get memory usage of process in KB"""
    cmd = f"ps aux | grep {process_name} | grep -v grep | awk '{{print $6}}'"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    memory = result.stdout.strip()
    return int(memory) if memory else 0

def count_log_lines_containing(pattern, log_file=UPLOADSTB_LOG):
    """Count lines containing pattern in log file"""
    result = subprocess.run(f"grep -c '{pattern}' {log_file} 2>/dev/null || echo 0", 
                          shell=True, capture_output=True, text=True)
    return int(result.stdout.strip())

def wait_for_log_pattern(pattern, timeout=30, log_file=UPLOADSTB_LOG):
    """Wait for pattern to appear in log file"""
    start_time = time.time()
    while time.time() - start_time < timeout:
        if grep_uploadstb_logs(pattern, log_file):
            return True
        time.sleep(1)
    return False

def get_file_size(filepath):
    """Get file size in bytes"""
    try:
        return os.path.getsize(filepath)
    except:
        return 0

def trigger_upload_via_rbus(trigger_type="ondemand"):
    """Trigger upload via RBUS"""
    cmd = f"rbuscli set Device.DCM.TriggerUpload string {trigger_type}"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return result.returncode == 0

