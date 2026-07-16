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

Feature: uploadSTBLogs Sync Gate - NTP Synchronization (REQ-SYNC-002)

  If the NTP sentinel (/tmp/stt_received) is absent the upload strategy falls
  back through two sub-paths:
    - Internet available  → apply last-known-good time from systimemgr
    - No internet         → proceed with current system time (log warning)

  Background:
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And PreviousLogs directory exists with log files

  # -----------------------------------------------------------------------
  # Positive path — sentinel present
  # -----------------------------------------------------------------------

  @sync_gate @ntp @positive
  Scenario: NTP sentinel present — detection logged, no fallback attempted
    Given the NTP sync sentinel /tmp/stt_received is present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should log "NTP sync sentinel detected. Proceeding."
    And the service should NOT log "NTP absent"

  @sync_gate @ntp @positive
  Scenario: NTP sentinel present — absent log does not appear
    Given the NTP sync sentinel /tmp/stt_received is present
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should NOT log "NTP absent"

  # -----------------------------------------------------------------------
  # Fallback paths — sentinel absent
  # -----------------------------------------------------------------------

  @sync_gate @ntp @fallback
  Scenario: NTP absent but internet available — systimemgr fallback used
    Given the NTP sync sentinel /tmp/stt_received is NOT present
    And internet connectivity is available
    And the systimemgr clock file contains a valid epoch
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should log "NTP absent"

  @sync_gate @ntp @degraded
  Scenario: NTP absent and no internet — proceed with current system time
    Given the NTP sync sentinel /tmp/stt_received is NOT present
    And internet connectivity is NOT available
    And the systimemgr clock file is absent
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should log "NTP absent"
    And the service should NOT abort the upload due to NTP absence

  # -----------------------------------------------------------------------
  # Edge cases — invalid or missing systimemgr data
  # -----------------------------------------------------------------------

  @sync_gate @ntp @edge_case
  Scenario: NTP absent with invalid systimemgr epoch — handled gracefully
    Given the NTP sync sentinel /tmp/stt_received is NOT present
    And the systimemgr clock file contains a non-numeric value
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should complete without crashing

  @sync_gate @ntp @edge_case
  Scenario: NTP absent with missing systimemgr clock file — handled gracefully
    Given the NTP sync sentinel /tmp/stt_received is NOT present
    And the systimemgr clock file /opt/secure/clock.txt does NOT exist
    And all other synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should complete without crashing
