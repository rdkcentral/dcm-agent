####################################################################################
# If not stated otherwise in this file or this component's LICENSE file
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

Feature: backup_logs Integration and Lifecycle Testing  
  Corresponds to test_integration.py - covers full initialization sequence, backup execution lifecycle, systemd notification, and cleanup behavior

  Background:
    Given the backup_logs service is available
    And the required system directories exist
    And system configuration files are accessible

  @initialization @lifecycle @positive
  Scenario: backup_logs initialization completes successfully
    Given all required directories and files are present
    When backup_logs initializes the system
    Then the initialization should complete without error
    And the backup_logs.log should contain "Backup system initialization completed successfully"
    And the system should return exit code 0

  @initialization @error_handling @negative
  Scenario: Initialization with invalid invocation is handled gracefully
    Given backup_logs is invoked with invalid parameters
    When the system attempts initialization
    Then no segfault or crash should occur  
    And the backup_logs.log should not contain "segfault", "core dump", or "signal 11"
    And the system should handle the error gracefully

  @execution @lifecycle @positive
  Scenario: Backup execution process starts and is logged
    Given backup_logs has initialized successfully
    And log files are present for backup
    When the backup execution process starts
    Then the execution start should be logged in backup_logs.log
    And the backup process should begin processing files

  @execution @lifecycle @positive
  Scenario: Complete backup execution returns success
    Given backup_logs has initialized successfully
    And log files are available for backup
    When the complete backup execution runs
    Then the backup should complete successfully
    And the system should return exit code 0
    And all expected backup operations should be performed

  @systemd @notification @positive
  Scenario: Systemd notification is sent on successful completion
    Given backup_logs is running in systemd environment
    And the backup operation completes successfully
    When the backup process finishes
    Then a systemd notification should be sent
    And the notification should indicate successful completion
    And systemd should be aware of the service status

  @disk_threshold @resource_management @positive
  Scenario: Disk threshold check is performed before backup
    Given the disk threshold check script is available
    And sufficient disk space exists for backup operations
    When backup_logs performs disk space validation
    Then the disk threshold script should be executed
    And available space should be validated against requirements
    And backup should proceed if space is adequate

  @disk_threshold @insufficient_space @negative
  Scenario: Backup is prevented when insufficient disk space
    Given the disk threshold check script is available
    And insufficient disk space exists for backup operations  
    When backup_logs performs disk space validation
    Then the disk threshold check should fail
    And backup operations should be prevented
    And an appropriate error message should be logged

  @cleanup @resource_management @positive
  Scenario: System cleanup performs proper resource cleanup
    Given backup_logs has completed backup operations
    And temporary files and resources were created during backup
    When the cleanup process runs
    Then all temporary files should be properly cleaned up
    And system resources should be freed
    And no orphaned processes or files should remain
    And cleanup completion should be logged

  @cleanup @file_handles @positive
  Scenario: File handles are properly closed after operations
    Given backup_logs has opened files for backup operations
    When the backup operations complete
    Then all file handles should be properly closed
    And no file handle leaks should occur
    And the system should release all file resources

  @end_to_end @full_cycle @positive
  Scenario: Complete end-to-end backup lifecycle
    Given the system is in initial state
    And configuration is properly set up
    And log files are available for backup
    When a complete backup cycle is executed
    Then initialization should complete successfully
    And configuration should be loaded and validated
    And backup strategy should be selected based on configuration
    And log files should be processed according to strategy
    And special files should be handled if configured
    And cleanup should complete successfully
    And systemd notification should be sent
    And the system should return to ready state
