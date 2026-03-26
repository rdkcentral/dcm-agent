####################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
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

import subprocess
import os
import time
import re

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

BACKUP_LOGS_BINARY    = "/usr/local/bin/backup_logs"
BACKUP_LOG_FILE       = "/tmp/backup_logs.log.0"
LOG_PATH              = "/opt/logs"
PREV_LOG_PATH         = "/opt/logs/PreviousLogs"
PREV_LOG_BACKUP_PATH  = "/opt/logs/PreviousLogs_backup"
PERSISTENT_PATH       = "/opt/persistent"
DEVICE_PROPERTIES     = "/etc/device.properties"
INCLUDE_PROPERTIES    = "/etc/include.properties"
SPECIAL_FILES_CONF    = "/etc/backup_logs/special_files.conf"
DISK_THRESHOLD_SCRIPT = "/lib/rdk/disk_threshold_check.sh"

# ---------------------------------------------------------------------------
# Binary execution
# ---------------------------------------------------------------------------

def run_backup_logs(args="", timeout=60):
    """Execute the backup_logs binary and return the CompletedProcess result."""
    cmd = f"{BACKUP_LOGS_BINARY} {args}".strip()
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)
    return result

# ---------------------------------------------------------------------------
# Log file helpers
# ---------------------------------------------------------------------------

def grep_backup_logs(search_pattern, log_file=BACKUP_LOG_FILE):
    """Return lines from backup_logs log file that match the literal string."""
    matches = []
    pattern = re.compile(re.escape(search_pattern), re.IGNORECASE)
    try:
        with open(log_file, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if pattern.search(line):
                    matches.append(line.strip())
    except Exception as e:
        print(f"Could not read {log_file}: {e}")
    return matches

def grep_backup_logs_regex(regex_pattern, log_file=BACKUP_LOG_FILE):
    """Return lines from backup_logs log file that match the regex."""
    matches = []
    pattern = re.compile(regex_pattern, re.IGNORECASE)
    try:
        with open(log_file, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if pattern.search(line):
                    matches.append(line.strip())
    except Exception as e:
        print(f"Could not read {log_file}: {e}")
    return matches

def clear_backup_logs():
    """Truncate the backup_logs log file."""
    try:
        subprocess.run(f"echo '' > {BACKUP_LOG_FILE}", shell=True)
        return True
    except Exception:
        return False

# ---------------------------------------------------------------------------
# Directory helpers
# ---------------------------------------------------------------------------

def ensure_dir(path):
    """Create directory (and parents) if it does not exist."""
    os.makedirs(path, exist_ok=True)

def empty_dir(path):
    """Remove all files (not subdirs) inside a directory."""
    if os.path.isdir(path):
        subprocess.run(f"rm -f {path}/*", shell=True)

def remove_dir_contents(path):
    """Remove all contents (files + subdirs) inside a directory."""
    if os.path.isdir(path):
        subprocess.run(f"rm -rf {path}/*", shell=True)

def setup_log_directories():
    """Create the standard backup_logs directory layout."""
    for d in [LOG_PATH, PREV_LOG_PATH, PREV_LOG_BACKUP_PATH, PERSISTENT_PATH]:
        ensure_dir(d)

def cleanup_log_directories():
    """Empty test log files and backup directories."""
    for d in [PREV_LOG_PATH, PREV_LOG_BACKUP_PATH]:
        remove_dir_contents(d)
    # Remove test log files but not backup_logs.log itself
    subprocess.run(f"find {LOG_PATH} -maxdepth 1 -name 'test_*.log' -delete", shell=True)
    subprocess.run(f"find {LOG_PATH} -maxdepth 1 -name 'test_*.txt' -delete", shell=True)
    subprocess.run(f"find {LOG_PATH} -maxdepth 1 -name 'bootlog' -delete", shell=True)

# ---------------------------------------------------------------------------
# Log file creation
# ---------------------------------------------------------------------------

def create_test_log_files(directory=LOG_PATH, count=3, size_kb=10):
    """Create numbered test .log files in directory."""
    ensure_dir(directory)
    created = []
    for i in range(count):
        path = os.path.join(directory, f"test_{i}.log")
        subprocess.run(
            f"dd if=/dev/urandom of={path} bs=1024 count={size_kb} 2>/dev/null",
            shell=True
        )
        created.append(path)
    return created

def create_test_txt_files(directory=LOG_PATH, count=3):
    """Create numbered test .txt files in directory."""
    ensure_dir(directory)
    created = []
    for i in range(count):
        path = os.path.join(directory, f"test_{i}.txt")
        with open(path, "w") as f:
            f.write(f"test txt content {i}\n")
        created.append(path)
    return created

def create_messages_txt(directory=LOG_PATH):
    """Create the sentinel messages.txt file used in rotation checks."""
    path = os.path.join(directory, "messages.txt")
    with open(path, "w") as f:
        f.write("system log content\n")
    return path

def create_bootlog(directory=LOG_PATH):
    """Create a bootlog file."""
    path = os.path.join(directory, "bootlog")
    with open(path, "w") as f:
        f.write("boot log content\n")
    return path

def create_last_reboot_marker(directory=PREV_LOG_PATH):
    """Touch last_reboot marker in directory."""
    path = os.path.join(directory, "last_reboot")
    subprocess.run(f"touch {path}", shell=True)
    return path

def remove_last_reboot_marker(directory=PREV_LOG_PATH):
    """Remove last_reboot marker."""
    path = os.path.join(directory, "last_reboot")
    if os.path.exists(path):
        os.remove(path)

def file_exists_in(directory, filename):
    """Return True if filename exists in directory."""
    return os.path.exists(os.path.join(directory, filename))

def list_files(directory):
    """Return list of filenames (not dirs) in directory."""
    if not os.path.isdir(directory):
        return []
    return [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]

def list_subdirs(directory):
    """Return list of subdirectory names in directory."""
    if not os.path.isdir(directory):
        return []
    return [d for d in os.listdir(directory) if os.path.isdir(os.path.join(directory, d))]

# ---------------------------------------------------------------------------
# Property helpers
# ---------------------------------------------------------------------------

def set_device_property(key, value):
    """Upsert a key=value line in /etc/device.properties."""
    subprocess.run(f"sed -i '/^{key}=/d' {DEVICE_PROPERTIES}", shell=True)
    subprocess.run(f"echo '{key}={value}' >> {DEVICE_PROPERTIES}", shell=True)

def get_device_property(key):
    """Read a property value from /etc/device.properties."""
    result = subprocess.run(
        f"grep '^{key}=' {DEVICE_PROPERTIES} | cut -d'=' -f2",
        shell=True, capture_output=True, text=True
    )
    return result.stdout.strip()

def set_include_property(key, value):
    """Upsert a key=value line in /etc/include.properties."""
    subprocess.run(f"sed -i '/^{key}=/d' {INCLUDE_PROPERTIES}", shell=True)
    subprocess.run(f"echo '{key}={value}' >> {INCLUDE_PROPERTIES}", shell=True)

def restore_default_properties():
    """Restore HDD_ENABLED and LOG_PATH to safe defaults."""
    set_device_property("HDD_ENABLED", "false")
    set_include_property("LOG_PATH", LOG_PATH)

# ---------------------------------------------------------------------------
# Process helpers
# ---------------------------------------------------------------------------

def get_backup_logs_pid():
    result = subprocess.run("pidof backup_logs", shell=True, capture_output=True, text=True)
    return result.stdout.strip()

def kill_backup_logs(signal=9):
    pid = get_backup_logs_pid()
    if pid:
        subprocess.run(f"kill -{signal} {pid}", shell=True)
        time.sleep(1)
        return True
    return False
