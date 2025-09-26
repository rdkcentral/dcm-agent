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
    rbus_set_data("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL", "string", "https://mockxconf:50050/loguploader5/getT2DCMSettings")
    run_dcmd()
    run_telemetry()
    sleep(20)
    assert "Uploading logs as DCM response is either null or not present" in grep_dcmdlogs("Uploading logs as DCM response is either null or not present")

@pytest.mark.run(order=2)
def test_fw_cron_scheduled():
    assert "difdCron is empty" in grep_dcmdlogs("difdCron is empty")

