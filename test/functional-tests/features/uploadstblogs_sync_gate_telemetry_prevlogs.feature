####################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

Feature: uploadSTBLogs Sync Gate - Telemetry Prevlogs Done Sentinel (REQ-SYNC-004)

  Uses inotify-based wait_for_sentinel() to watch for
  /tmp/.telemetry_prevlogs_done.  This is a SOFT gate — when the timeout
  expires the upload logs a warning and continues rather than aborting.

  Background:
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And PreviousLogs directory exists with log files

  # -----------------------------------------------------------------------
  # Fast path — sentinel already present
  # -----------------------------------------------------------------------

  @sync_gate @telemetry_prevlogs @positive
  Scenario: Telemetry prevlogs sentinel already present — fast path detection
    Given the telemetry prevlogs sentinel /tmp/.telemetry_prevlogs_done is present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should log "Telemetry prevlogs sentinel detected. Proceeding."
    And the service should NOT log "proceeding without telemetry sync"

  # -----------------------------------------------------------------------
  # inotify detection — sentinel appears during wait
  # -----------------------------------------------------------------------

  @sync_gate @telemetry_prevlogs @inotify
  Scenario: Telemetry prevlogs sentinel appears during inotify wait — detected via IN_CREATE
    Given the telemetry prevlogs sentinel /tmp/.telemetry_prevlogs_done is NOT present
    And all other synchronization sentinels are present
    When reboot upload starts waiting for the telemetry prevlogs sentinel
    And the sentinel file /tmp/.telemetry_prevlogs_done is created within the timeout
    Then the inotify watch should detect the file creation
    And the service should log "Telemetry prevlogs sentinel detected"

  # -----------------------------------------------------------------------
  # Soft timeout path — sentinel never appears
  # -----------------------------------------------------------------------

  @sync_gate @telemetry_prevlogs @timeout
  Scenario: Telemetry prevlogs timeout — soft gate logs warning and continues
    Given the telemetry prevlogs sentinel /tmp/.telemetry_prevlogs_done is NOT present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    And the timeout expires without the sentinel appearing
    Then the service should log "Telemetry prevlogs sentinel not present after"
    And the service should log "proceeding without telemetry sync"

  @sync_gate @telemetry_prevlogs @timeout
  Scenario: Telemetry prevlogs sentinel absent — detection log does not appear
    Given the telemetry prevlogs sentinel /tmp/.telemetry_prevlogs_done is NOT present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should NOT log "Telemetry prevlogs sentinel detected. Proceeding."

  @sync_gate @telemetry_prevlogs @timeout
  Scenario: Telemetry prevlogs timeout — archive phase still reached (soft gate)
    Given the telemetry prevlogs sentinel /tmp/.telemetry_prevlogs_done is NOT present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    And the timeout expires without the sentinel appearing
    Then the service should proceed to the archive phase
