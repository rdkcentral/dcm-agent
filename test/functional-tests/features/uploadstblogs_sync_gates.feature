Feature: uploadSTBLogs Log Upload Synchronization Gates

  Background:
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And PreviousLogs directory exists with log files

  # REQ-SYNC-001: backup_logs completion gate
  @sync_gate @backup_logs @positive
  Scenario: Reboot upload waits for backup_logs completion sentinel
    Given backup_logs has completed and written the done flag
    When reboot upload strategy is triggered
    Then the service should detect the backup_logs sentinel
    And proceed with log upload

  @sync_gate @backup_logs @negative
  Scenario: Reboot upload aborts when backup_logs sentinel is absent
    Given backup_logs has NOT completed (done flag absent)
    When reboot upload strategy is triggered
    Then the service should abort the upload
    And log a warning about backup_logs not done

  # REQ-SYNC-002: NTP sync gate with fallback
  @sync_gate @ntp @positive
  Scenario: Reboot upload proceeds when NTP sentinel is present
    Given the NTP sync sentinel file exists
    When reboot upload strategy is triggered
    Then the service should detect NTP sync
    And use current system time for archive naming

  @sync_gate @ntp @fallback
  Scenario: NTP absent but internet available - use systimemgr fallback
    Given the NTP sync sentinel file does NOT exist
    And internet connectivity is available
    And systimemgr clock file contains a valid epoch
    When reboot upload strategy is triggered
    Then the service should apply last-known-good time from systimemgr
    And proceed with log upload

  @sync_gate @ntp @degraded
  Scenario: NTP absent and no internet - proceed with system time
    Given the NTP sync sentinel file does NOT exist
    And internet connectivity is NOT available
    When reboot upload strategy is triggered
    Then the service should proceed with current system time
    And log a warning about missing NTP

  # REQ-SYNC-003: Reboot reason sentinel
  @sync_gate @reboot_reason @positive
  Scenario: Reboot reason sentinel already present
    Given the reboot reason sentinel file exists
    When reboot upload waits for reboot reason
    Then the wait should return immediately
    And proceed with log upload

  @sync_gate @reboot_reason @inotify
  Scenario: Reboot reason sentinel appears during inotify wait
    Given the reboot reason sentinel file does NOT exist
    When reboot upload starts waiting for reboot reason
    And the sentinel file is created within the timeout
    Then the inotify watch should detect the file creation
    And proceed with log upload

  @sync_gate @reboot_reason @timeout
  Scenario: Reboot reason sentinel timeout triggers update request
    Given the reboot reason sentinel file does NOT exist
    When reboot upload waits for reboot reason
    And the timeout expires without the sentinel appearing
    Then the service should trigger a reboot info update
    And continue with log upload regardless

  # REQ-SYNC-004: Telemetry previous logs sentinel
  @sync_gate @telemetry_prevlogs @positive
  Scenario: Telemetry prevlogs sentinel already present
    Given the telemetry prevlogs done sentinel exists
    When reboot upload waits for telemetry prevlogs
    Then the wait should return immediately

  @sync_gate @telemetry_prevlogs @inotify
  Scenario: Telemetry prevlogs sentinel appears during inotify wait
    Given the telemetry prevlogs done sentinel does NOT exist
    When reboot upload starts waiting for telemetry prevlogs
    And the sentinel file is created within the timeout
    Then the inotify watch should detect the file creation
    And proceed with log upload

  @sync_gate @telemetry_prevlogs @timeout
  Scenario: Telemetry prevlogs timeout - soft gate continues upload
    Given the telemetry prevlogs done sentinel does NOT exist
    When reboot upload waits for telemetry prevlogs
    And the timeout expires without the sentinel appearing
    Then the service should log a warning
    And continue with log upload regardless (soft gate)

  # Combined synchronization sequence
  @sync_gate @sequence @positive
  Scenario: Full reboot upload synchronization sequence succeeds
    Given all synchronization sentinels are present
    When reboot upload strategy is triggered
    Then the service should pass all sync gates
    And proceed to archive and upload phases

  @sync_gate @sequence @partial_failure
  Scenario: Reboot upload continues with partial sync gate failures
    Given backup_logs sentinel is present
    And NTP sentinel is absent but internet is available
    And reboot reason sentinel appears after short delay
    And telemetry prevlogs sentinel times out
    When reboot upload strategy is triggered
    Then the service should handle each gate appropriately
    And still proceed to archive and upload phases
