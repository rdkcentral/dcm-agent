####################################################################################
# If not stated otherwise in this file or this component's LICENSE file
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

Feature: backup_logs Configuration Management
  Corresponds to test_config_manager.py - covers configuration loading and property parsing

  Background:
    Given the backup_logs service is available
    And the device properties files exist
    And the /opt/logs directory exists

  @config_loading @device_properties @positive
  Scenario: Device properties file is loaded successfully
    Given the device.properties file exists with valid content
    When backup_logs initializes the configuration
    Then device properties should be loaded without error
    And the configuration should be accessible to the backup system

  @config_loading @hdd_enabled_true @positive
  Scenario: HDD_ENABLED property set to true is parsed correctly
    Given the device.properties file contains "HDD_ENABLED=true"
    When backup_logs reads the configuration
    Then the HDD_ENABLED property should be parsed as true
    And the system should use HDD-enabled backup strategy

  @config_loading @hdd_enabled_false @positive  
  Scenario: HDD_ENABLED property set to false is parsed correctly
    Given the device.properties file contains "HDD_ENABLED=false"
    When backup_logs reads the configuration
    Then the HDD_ENABLED property should be parsed as false
    And the system should use HDD-disabled backup strategy with rotation

  @config_loading @log_path @positive
  Scenario: LOG_PATH property is loaded from include.properties
    Given the include.properties file contains "LOG_PATH=/opt/logs"
    When backup_logs reads the configuration
    Then the LOG_PATH should be set to "/opt/logs"
    And log file operations should use the configured path

  @config_loading @missing_property @negative
  Scenario: Missing HDD_ENABLED property defaults to false
    Given the device.properties file exists but does not contain HDD_ENABLED
    When backup_logs reads the configuration  
    Then the HDD_ENABLED property should default to false
    And the system should use HDD-disabled backup strategy

  @config_loading @invalid_hdd_value @negative
  Scenario: Invalid HDD_ENABLED value defaults to false
    Given the device.properties file contains "HDD_ENABLED=invalid"
    When backup_logs reads the configuration
    Then the HDD_ENABLED property should default to false
    And the system should log a warning about invalid property value

  @config_loading @missing_config_file @negative
  Scenario: Missing device.properties file is handled gracefully
    Given the device.properties file does not exist
    When backup_logs attempts to read the configuration
    Then the system should handle the missing file gracefully
    And all properties should use default values
    And an appropriate error should be logged

  @config_loading @property_validation @positive
  Scenario: Configuration validation ensures required directories exist
    Given valid device properties are loaded
    When backup_logs validates the configuration
    Then all required directories should be verified or created
    And the system should log successful configuration validation

  @config_reloading @property_change @positive
  Scenario: Configuration changes are detected on reload
    Given backup_logs has loaded initial configuration
    And the device.properties file is updated with new values
    When the configuration is reloaded
    Then the new property values should be applied
    And the appropriate backup strategy should be selected based on new config
