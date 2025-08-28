####################################################################################
# If not stated otherwise in this file or this component's Licenses
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
##################################################################################

Feature: dcm-agent bootup sequence

  Scenario: dcm-agent bootup sequence
    Given When the dcm-agent is not already running
    Then check whether the Telemetry2_0 is enabled
    Then dcm-agent should be running after the Telemetry2_0
    Then Initialize DCM Component 
    Then Load the Default config and fetch the Default Boot config from the persistent location
    Then registers Device.X_RDKCENTREL-COM.Reloadconfig event
    Then Subscribe Device.DCM.Setconfig event and Device.DCM.Processconfig event
    When Telemetry Events subscriptions is success
    Then Sent Event to telemetry for configuraion path
    When Telemetry sends the event data successfully
    Then Receive data for events Device.DCM.Setconfig event and Device.DCM.Processconfig
    Then the conf files are created
    Then scheduler starts
    Then parse the data and start the logupload and fwupdate script based on the schedule given
