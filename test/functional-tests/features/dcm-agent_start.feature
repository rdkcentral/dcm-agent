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

Feature: dcm-agent should start creating the conf file and wait for telemetry events

  Scenario: dcm-agent bootup sequence
    Given When the dcm-agent is not already running
    Then check whether the Telemetry2_0 is enabled
    Then dcm-agent should be running after the Telemetry2_0
    Then Initialize DCM Component 
    Then Load the Default config and fetch the Default Boot config from the persistent location
    Then register Device.X_RDKCENTREL-COM.Reloadconfig
    When the config is not received for event Device.DCM.Setconfig
    Then wait for Telemetry to be up and send the events
    When the config is not received for event Device.DCM.Processconfig
    Then wait for Telemetry to be up and send the events

