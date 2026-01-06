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

Feature: uploadSTBLogs Error Handling and Edge Cases

  @invalid_config @error_handling @negative
  Scenario: Invalid Configuration Handling
    Given the uploadSTBLogs service is initialized
    And the device properties file is corrupted or malformed
    When device properties file is corrupted during upload request
    Then the service should attempt to read device properties
    And the service should detect configuration parsing failure
    And service should log error and fail gracefully without system crash
    And the service should log configuration error details
    And the service should exit with configuration error code
    And no upload attempt should be made
    And failure telemetry should be generated with config error type

  @empty_logs @edge_case @negative
  Scenario: No Log Files Available for Upload
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And no log files are available for upload
    When log upload request is triggered
    Then the service should attempt to collect log files
    And the service should detect no files found
    And the service should log no files available message
    And no upload attempt should be made
    And appropriate telemetry should be generated
    And the service should exit gracefully
