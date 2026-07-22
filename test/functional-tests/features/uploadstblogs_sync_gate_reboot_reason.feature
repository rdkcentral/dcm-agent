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

Feature: uploadSTBLogs Sync Gate - Reboot Reason Sentinel (REQ-SYNC-003)

  Uses inotify-based wait_for_sentinel() to watch for
  /tmp/Update_rebootInfo_invoked.  On timeout, trigger_reboot_info_update()
  creates /tmp/stt_received so reboot-manager re-populates the reason.
  This is NOT a hard abort gate — upload continues regardless.

  Background:
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And PreviousLogs directory exists with log files

  # -----------------------------------------------------------------------
  # Fast path — sentinel already present
  # -----------------------------------------------------------------------

  @sync_gate @reboot_reason @positive
  Scenario: Reboot reason sentinel already present — fast path detection
    Given the reboot reason sentinel /tmp/Update_rebootInfo_invoked is present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should log "Reboot reason sentinel detected. Proceeding."
    And the service should NOT log "trigger to request immediate update"

  # -----------------------------------------------------------------------
  # inotify detection — sentinel appears during wait
  # -----------------------------------------------------------------------

  @sync_gate @reboot_reason @inotify
  Scenario: Reboot reason sentinel appears during inotify wait — detected via IN_CREATE
    Given the reboot reason sentinel /tmp/Update_rebootInfo_invoked is NOT present
    And all other synchronization sentinels are present
    When reboot upload starts waiting for the reboot reason sentinel
    And the sentinel file /tmp/Update_rebootInfo_invoked is created within the timeout
    Then the inotify watch should detect the file creation
    And the service should log "Reboot reason sentinel detected"

  # -----------------------------------------------------------------------
  # Timeout path — sentinel never appears
  # -----------------------------------------------------------------------

  @sync_gate @reboot_reason @timeout
  Scenario: Reboot reason sentinel timeout — triggers reboot info update
    Given the reboot reason sentinel /tmp/Update_rebootInfo_invoked is NOT present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    And the timeout expires without the sentinel appearing
    Then the service should log "Reboot reason sentinel not present after"
    And the service should log "trigger to request immediate update"

  @sync_gate @reboot_reason @timeout
  Scenario: Reboot reason sentinel absent — detection log does not appear
    Given the reboot reason sentinel /tmp/Update_rebootInfo_invoked is NOT present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should NOT log "Reboot reason sentinel detected. Proceeding."

  @sync_gate @reboot_reason @timeout
  Scenario: Reboot reason sentinel timeout — upload is not hard-aborted
    Given the reboot reason sentinel /tmp/Update_rebootInfo_invoked is NOT present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    And the timeout expires without the sentinel appearing
    Then the service should NOT abort the upload due to reboot reason timeout
