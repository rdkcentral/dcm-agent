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
import tempfile
import pytest

#Test_telemetry_communication

@pytest.mark.run(order=1)
def test_setConfig_subscription():
    rbus_set_data("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL", "string", "https://mockxconf:50050/loguploader3/getT2DCMSettings")
    run_dcmd()
    run_telemetry()
    sleep(5)
    assert "Subscription Device.DCM.Setconfig event Success" in grep_dcmdlogs("Subscription Device.DCM.Setconfig event")

@pytest.mark.run(order=2)
def test_ProcessConfig_subscription():
    assert "Subscription Device.DCM.Processconfig event Success" in grep_dcmdlogs("Subscription Device.DCM.Processconfig event")

@pytest.mark.run(order=3)
def test_telemetry_communication():
    assert "Telemetry Events subscriptions is success" in grep_dcmdlogs("Telemetry Events subscriptions")


@pytest.mark.run(order=4)
def test_receive_event_setConfig():
    sleep(2)
    assert "Event Name: Device.DCM.Setconfig" in grep_dcmdlogs("Event Name: Device.DCM.Setconfig")

@pytest.mark.run(order=5)
def test_receive_event_ProcessConfig():
    assert "Event Name: Device.DCM.Processconfig" in grep_dcmdlogs("Event Name: Device.DCM.Processconfig")

@pytest.mark.run(order=6)
def test_scheduler_start():
    sleep(2)
    assert "Start Scheduling" in grep_dcmdlogs("Start Scheduling")

@pytest.mark.run(order=7)
def test_parser():
    assert "HTTP" in grep_dcmdlogs("Log Upload protocol:")
    assert "https://secure.s3.bucket.test.url" in grep_dcmdlogs("Log Upload URL:")
    assert "UTC" in grep_dcmdlogs("TimeZone :")
    assert "1" in grep_dcmdlogs("DCM_LOGUPLOAD_REBOOT:")
    assert "*/3" in grep_dcmdlogs("DCM_LOGUPLOAD_CRON:")
    assert "*/5" in grep_dcmdlogs("DCM_DIFD_CRON")


@pytest.mark.run(order=8)
def test_rdk_maintenance_file_presence():
    file_path = '/opt/rdk_maintenance.conf'
    assert os.path.isfile(file_path), f"{file_path} does not exist."

@pytest.mark.run(order=9)
def test_rdk_maintenance_file_presence():
    file_path = '/opt/rdk_maintenance.conf'
    required_strings = ["start_hr", "start_min", "tz_mode"]

    with open(file_path, "r") as f:
        contents = f.read()

    for s in required_strings:
        assert s in contents, f"'{s}' not found in {file_path}"
