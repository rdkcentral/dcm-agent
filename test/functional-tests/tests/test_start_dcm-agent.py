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
from time import sleep
from helper_functions import *


def test_check_dcmd_is_starting():
    print("Starting dcmd process")
    rbus_set_data("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version", "string", "2.0.1")
    rbus_set_data("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL", "string", "https://mockxconf:50050/loguploader3/getT2DCMSettings")
    run_dcmd()
    assert get_pid("dcmd") != ""
    sleep(15)

def test_default_config_fetched():
    assert "Loading the Default config" in grep_dcmdlogs("Loading the Default config")
    assert "/opt//.t2persistentfolder/DCMresponse.txt" in grep_dcmdlogs("Fetching the Default Boot config from")

def test_DCMSettings_file():
    file_path = '/tmp/DCMSettings.conf'
    assert os.path.isfile(file_path), f"{file_path} does not exist."

def test_dcm_waiting_for_telemetry():  
   assert "Waiting for Telemetry to up and running to Subscribe the events" in grep_dcmdlogs("Waiting for Telemetry to up and running to Subscribe the events")

def test_second_telemetry_instance_is_not_started():
    run_dcmd()
    pid1 = get_pid("dcmd")
    run_dcmd()
    pid2 = get_pid("dcmd")
    assert pid1 == pid2
    sleep(2)

def test_tear_down():
    kill_dcmd(9)
    assert get_pid("dcmd") == ""
