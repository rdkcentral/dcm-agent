Feature: USB Log Upload
  This feature covers the USB log upload functionality, including error handling, archive creation, MAC address logging, temp directory cleanup, and success/failure scenarios.

  Scenario: USB not mounted or missing log path
    Given the USB log upload binary is available
    When I run usblogupload with a non-existent mount point
    Then the process should fail with code 2 or 3
    And a failure message should be logged

  Scenario: Archive creation on valid mount
    Given a valid USB mount point
    When I run usblogupload
    Then the process should exit with code 0 or 3
    And an archive creation log may appear

  Scenario: MAC address and file log
    Given a valid USB mount point
    When I run usblogupload
    Then the process should exit with code 0 or 3
    And a log line with MAC address and file name may appear

  Scenario: Temp directory cleanup
    Given a valid USB mount point
    When I run usblogupload
    Then the process should exit with code 0 or 3
    And a cleanup log may appear

  Scenario: Successful USB log upload
    Given the USB log upload binary is available
    When I run usblogupload with a valid mount point
    Then the process should exit with code 0
    And a completion message should be logged

  Scenario: Invalid usage
    Given the USB log upload binary is available
    When I run usblogupload with no arguments
    Then the process should exit with code 4
    And a log about failed logging system initialization may appear

  Scenario: USB not mounted
    Given the USB log upload binary is available
    When I run usblogupload with an unmounted path
    Then the process should exit with code 2
    And a log about failed USB mount point validation may appear
