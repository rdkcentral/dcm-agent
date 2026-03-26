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

Feature: backup_logs Special Files Handling
  Corresponds to test_special_files.py - covers special file configuration parsing and operations  

  Background:
    Given the backup_logs service is available
    And the /opt/logs directory exists
    And the /opt/logs/PreviousLogs directory exists

  @special_files @config_parsing @positive
  Scenario: Special files configuration is parsed successfully
    Given the /etc/backup_logs/special_files.conf file exists
    And the config file contains valid file paths
    When backup_logs reads the special files configuration
    Then all configured file paths should be loaded
    And the special files list should be available for processing

  @special_files @copy_operation @positive
  Scenario: Special files are copied to PreviousLogs
    Given the special_files.conf contains "/var/log/system.log"
    And the file /var/log/system.log exists with content
    When backup_logs processes special files
    Then system.log should be copied to /opt/logs/PreviousLogs
    And the original file should remain in /var/log/
    And the copied file should have identical content

  @special_files @move_operation @positive
  Scenario: Special files are moved to PreviousLogs when configured
    Given the special_files.conf contains "/tmp/temp_log.txt" 
    And the file /tmp/temp_log.txt exists with content
    And the configuration specifies move operation for temp files
    When backup_logs processes special files
    Then temp_log.txt should be moved to /opt/logs/PreviousLogs 
    And the original file should be removed from /tmp/
    And the moved file should retain original content

  @special_files @missing_source @negative
  Scenario: Missing special files are handled gracefully
    Given the special_files.conf contains "/nonexistent/missing.log"
    And the file /nonexistent/missing.log does not exist  
    When backup_logs processes special files
    Then the missing file should be skipped without error
    And an appropriate warning should be logged
    And processing should continue with other special files

  @special_files @missing_config @negative
  Scenario: Missing special files configuration is handled gracefully
    Given the /etc/backup_logs/special_files.conf file does not exist
    When backup_logs attempts to process special files
    Then the system should skip special files processing
    And backup should continue with normal log file operations
    And an info message should be logged about missing config

  @special_files @invalid_permissions @negative
  Scenario: Special files with invalid permissions are handled
    Given the special_files.conf contains "/root/protected.log"
    And the file /root/protected.log exists but is not readable
    When backup_logs processes special files
    Then the protected file should be skipped
    And an appropriate permission error should be logged  
    And processing should continue with accessible files

  @special_files @conditional_check @positive
  Scenario: Special files conditional checks work correctly
    Given the special_files.conf contains conditional entries
    And some conditions evaluate to true and others to false
    When backup_logs processes special files with conditions
    Then only files meeting the true conditions should be processed
    And conditional checks should be logged appropriately

  @special_files @multiple_files @positive
  Scenario: Multiple special files are processed in sequence
    Given the special_files.conf contains multiple file entries
    And all specified files exist with different content
    When backup_logs processes all special files
    Then all files should be processed according to their configuration
    And each file operation should be logged separately
    And the processing order should follow configuration order

  @special_files @comment_handling @positive  
  Scenario: Configuration file comments and blank lines are ignored
    Given the special_files.conf contains comments and blank lines
    And valid file paths are mixed with comments
    When backup_logs parses the special files configuration
    Then comments should be ignored during parsing
    And blank lines should be skipped
    And only valid file paths should be processed
