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

Feature: uploadSTBLogs Upload Strategies

  @ondemand_upload @positive
  Scenario: On-Demand Upload Strategy
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And log files are available for upload
    When on-demand upload strategy is triggered
    Then the service should execute immediate upload
    And the service should not wait for scheduled time
    And logs should be collected and archived immediately
    And logs should be uploaded without delay
    And upload success telemetry should be generated
    And the operation should complete within expected time

  @reboot_upload @positive
  Scenario: Upload on Reboot Strategy
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And the device has recently rebooted
    And uploadOnReboot flag is set to true
    And log files are available for upload
    When reboot upload strategy is triggered
    Then the service should detect reboot condition
    And the service should collect logs from previous session
    And logs should be archived with reboot timestamp
    And logs should be uploaded to server
    And reboot upload success telemetry should be generated
    And temporary files should be cleaned up

  @dcm_scheduled_upload @positive
  Scenario: DCM Scheduled Upload Strategy
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And DCM schedule configuration is present
    And scheduled upload time has been reached
    And log files are available for upload
    When DCM scheduled upload strategy is triggered
    Then the service should verify schedule trigger
    And the service should collect logs according to configuration
    And logs should be archived and uploaded
    And upload success telemetry should be generated
    And next schedule should be updated
