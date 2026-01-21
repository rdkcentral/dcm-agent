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

Feature: uploadSTBLogs Normal Upload Operations

  @normal_upload @positive
  Scenario: Normal Log Upload with Valid Configuration
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And log files are available for upload
    When I trigger a log upload request with valid configuration
    Then the service should read device properties successfully
    And the service should collect log files from configured paths
    And the service should create archive of log files
    And logs should be successfully uploaded to HTTP server
    And the upload response should return HTTP 200 status
    And upload success telemetry should be generated
    And temporary files should be cleaned up

  @large_files @performance @positive
  Scenario: Large Log File Handling
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And large log files within size limits are available for upload
    And the log files total size is between 10MB and 50MB
    When uploading large log files within size limits
    Then the service should collect all log files
    And the service should validate total file size
    And service should upload files efficiently
    And the upload response should return HTTP 200 status
