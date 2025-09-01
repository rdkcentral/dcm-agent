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
from helper_functions import *
from time import sleep
import os
import pytest
import json

@pytest.mark.run(order=1)
def test_upload_cron_present():
    kill_dcmd(9)
    kill_telemetry(9)
    rbus_set_data("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL", "string", "https://mockxconf:50050/loguploader4/getT2DCMSettings")
    run_dcmd()
    run_telemetry()
    sleep(20)
    assert "urn:settings:LogUploadSettings:UploadSchedule:cron" in grep_dcmdlogs("is present setting cron jobs")

@pytest.mark.run(order=2)
def test_upload_script_started():
    assert "UploadOnReboot=0" in grep_dcmdlogs("Triggered uploadSTBLogs.sh with arguments")
    assert "Called uploadLogOnReboot with false" in grep_dcmdlogs("Called uploadLogOnReboot with false")

@pytest.mark.run(order=3)
def test_fw_cron_scheduled():
    sleep(200)
    assert "Scheduling DCM_FW_UPDATE Job handle" in grep_dcmdlogs("Scheduling DCM_FW_UPDATE Job handle")


@pytest.mark.run(order=4)
def test_upload_cron_scheduled():
    sleep(300)
    assert "Scheduling DCM_LOG_UPLOAD Job handle"  in grep_dcmdlogs("Scheduling DCM_LOG_UPLOAD Job handle")

@pytest.mark.run(order=5)
def test_upload_script_started():
    assert "Start log upload Script"  in grep_dcmdlogs("Start log upload Script")
    assert "Called uploadDCMLogs" in grep_dcmdlogs("Called uploadDCMLogs")
    sleep(60)

@pytest.mark.run(order=6)
def test_fwupdate_script_started():
    assert "Starting SoftwareUpdate Utility Script..." in grep_dcmdlogs("Starting SoftwareUpdate Utility Script...")
    assert "trigger type=" in grep_dcmdlogs("trigger type=")
