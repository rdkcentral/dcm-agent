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

Feature: backup_logs Engine Strategy Testing
  Corresponds to test_backup_engine.py - covers HDD-enabled/disabled strategies and file pattern matching

  Background:
    Given the backup_logs service is available
    And the /opt/logs directory exists
    And the /opt/logs/PreviousLogs directory exists

  @hdd_enabled @strategy @positive
  Scenario: HDD-enabled strategy execution is logged
    Given the device property HDD_ENABLED is set to "true"  
    And log files are present in /opt/logs directory
    When I execute backup_logs service
    Then the backup_logs.log should contain "Executing HDD-enabled backup strategy"
    And the backup operation should complete successfully

  @hdd_enabled @first_backup @positive
  Scenario: First-time backup moves files directly to PreviousLogs
    Given the device property HDD_ENABLED is set to "true"
    And log files are present in /opt/logs directory
    And there is no messages.txt file in /opt/logs/PreviousLogs
    When I execute backup_logs service for the first time
    Then all matching log files should be moved to /opt/logs/PreviousLogs
    And the files should retain their original names without prefixes

  @hdd_enabled @reboot_marker @positive
  Scenario: First-time backup creates last_reboot marker
    Given the device property HDD_ENABLED is set to "true"
    And log files are present in /opt/logs directory  
    When I execute backup_logs service for the first time
    Then a last_reboot marker file should be created in /opt/logs/PreviousLogs

  @hdd_enabled @exclusion @positive
  Scenario: Active backup_logs.log file is never moved
    Given the device property HDD_ENABLED is set to "true"
    And backup_logs.log is actively being written to in /opt/logs
    And other log files are present in /opt/logs directory
    When I execute backup_logs service
    Then backup_logs.log should remain in /opt/logs directory
    And backup_logs.log should not appear in /opt/logs/PreviousLogs

  @hdd_disabled @strategy @positive
  Scenario: HDD-disabled rotation strategy execution is logged
    Given the device property HDD_ENABLED is set to "false"
    And log files are present in /opt/logs directory
    When I execute backup_logs service
    Then the backup_logs.log should contain "Executing HDD-disabled backup strategy with rotation"

  @hdd_disabled @bak1_rotation @positive
  Scenario: Second backup uses bak1_ prefix rotation
    Given the device property HDD_ENABLED is set to "false"
    And log files including messages.txt are present in /opt/logs directory
    And messages.txt exists in /opt/logs/PreviousLogs
    And no bak1_messages.txt exists in /opt/logs/PreviousLogs
    When I execute backup_logs service
    Then the backup_logs.log should contain "Moving logs to bak1_ prefix"

  @hdd_disabled @full_rotation @positive
  Scenario: Full rotation cycle when all slots occupied
    Given the device property HDD_ENABLED is set to "false"
    And log files including messages.txt are present in /opt/logs directory
    And messages.txt, bak1_messages.txt, bak2_messages.txt, and bak3_messages.txt exist in /opt/logs/PreviousLogs
    When I execute backup_logs service
    Then the backup_logs.log should contain "Performing full rotation cycle"

  @pattern_matching @txt_files @positive
  Scenario: Files containing .txt in name are moved to PreviousLogs
    Given the device property HDD_ENABLED is set to "false"
    And a test_app.txt file exists in /opt/logs
    When I execute backup_logs service
    Then test_app.txt should be moved to /opt/logs/PreviousLogs

  @pattern_matching @log_files @positive  
  Scenario: Files containing .log in name are moved to PreviousLogs
    Given the device property HDD_ENABLED is set to "false"
    And a test_app.log file exists in /opt/logs
    When I execute backup_logs service
    Then test_app.log should be moved to /opt/logs/PreviousLogs
    And backup_logs.log should remain in /opt/logs

  @pattern_matching @bootlog @positive
  Scenario: bootlog file is matched and moved
    Given the device property HDD_ENABLED is set to "false"
    And a bootlog file exists in /opt/logs
    When I execute backup_logs service
    Then bootlog should be moved to /opt/logs/PreviousLogs
