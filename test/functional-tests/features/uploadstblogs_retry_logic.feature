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

Feature: uploadSTBLogs Resource Management and Cleanup

  @cleanup @resource_management @positive
  Scenario: System Resource Cleanup
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And log files are available for upload
    And temporary directory is available for operations
    When log upload operation completes
    Then the service should collect and archive log files
    And the service should upload archive to server
    And the upload should complete successfully
    And all temporary files and resources should be properly cleaned up
    And the temporary archive file should be deleted
    And the lock file should be removed
    And all file handles should be closed
    And all memory allocations should be freed
    And no orphaned resources should remain in the system
    And the temporary directory should be empty or removed

  @memory_constraints @resource_management @positive
  Scenario: Memory Constraint Operation
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And log files are available for upload
    When device is under heavy memory load during upload
    Then the service should allocate memory for operation
    And service should operate within memory limits without exhaustion
    And the service should not exceed configured memory threshold
    And the service should successfully complete upload operation
    And the service should free all allocated memory after completion
    And no memory leaks should be detected
    And upload success telemetry should be generated

  @concurrent_requests @stability @negative
  Scenario: Concurrent Upload Request Handling
    Given the uploadSTBLogs service is initialized and running
    And the device properties file is present and valid
    And the HTTP upload server is accessible
    And log files are available for upload
    And a first upload request is currently in progress
    When second upload request arrives during active upload
    Then the service should detect existing upload lock file
    And service should queue/reject request and maintain stability
    And the second request should fail with lock acquisition error
    And the first upload should continue uninterrupted
    And the first upload should complete successfully
    And appropriate error message should be logged for second request
    And the service should not crash or become unstable
