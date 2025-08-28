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

LOG_FILE = "/opt/logs/dcmd.log.0"
RBUSCLI_SET_CMD = "rbuscli set "

def run_dcmd():
    return subprocess.run("/usr/local/bin/dcmd", shell=True)

def run_telemetry():
    return subprocess.run("/usr/local/bin/telemetry2_0", shell=True)

def run_shell_silent(command):
    subprocess.run(command, shell=True, capture_output=False, text=False)
    return 

def run_shell_command(command):
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    return result.stdout.strip()

def grep_dcmdlogs(search: str):
    search_result = ""
    search_pattern = re.compile(re.escape(search), re.IGNORECASE)
    try:
        with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as file:
            for line_number, line in enumerate(file, start=1):
                if search_pattern.search(line):
                    search_result = search_result + " \n" + line
    except Exception as e:
        print(f"Could not read file {LOG_FILE}: {e}")
    return search_result

def kill_dcmd(signal: int=9):
    print(f"Received Signal to kill dcmd {signal} with pid {get_pid('dcmd')}")
    resp = subprocess.run(f"kill -{signal} {get_pid('dcmd')}", shell=True, capture_output=True)
    print(resp.stdout.decode('utf-8'))
    print(resp.stderr.decode('utf-8'))
    return ""

def kill_telemetry(signal: int=9):
    print(f"Received Signal to kill telemetry2_0 {signal} with pid {get_pid('telemetry2_0')}")
    resp = subprocess.run(f"kill -{signal} {get_pid('telemetry2_0')}", shell=True, capture_output=True)
    print(resp.stdout.decode('utf-8'))
    print(resp.stderr.decode('utf-8'))
    return ""

def rbus_get_data(param: str):
    return subprocess.run(RBUSCLI_GET_CMD + param, shell=True, capture_output=True).stdout.decode('utf-8')

def rbus_set_data(param: str, type:str, value: str):
    return subprocess.run(f"{RBUSCLI_SET_CMD} {param} {type} {value}", shell=True, capture_output=True).stdout.decode('utf-8')

def get_pid(name: str):                                                                                                                        
    return subprocess.run(f"pidof {name}", shell=True, capture_output=True).stdout.decode('utf-8').strip()

def clear_dcmdlogs():
    subprocess.run(f"echo '' > {LOG_FILE}", shell=True)
