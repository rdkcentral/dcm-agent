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

Feature: uploadSTBLogs Security and Authentication

  @mtls @security @positive
  Scenario: Log Upload with mTLS Authentication
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTPS upload server is accessible
    And client certificate is present at configured path
    And client private key is present at configured path
    And CA certificate is present at configured path
    And log files are available for upload
    When I trigger a secure log upload request with valid certificates
    Then the service should load client certificate
    And the service should load client private key
    And the service should load CA certificate for verification
    And the service should establish TLS handshake with server
    And logs should upload successfully over HTTPS with proper certificate validation
    And the upload response should return HTTP 200 status
    And the TLS connection should be properly closed
    And upload success telemetry should be generated

  @ssl_validation @security @negative
  Scenario: Upload with Invalid Server Certificate
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTPS upload server is accessible
    And the server presents an invalid or expired SSL certificate
    And CA certificate is present at configured path
    And log files are available for upload
    When upload server presents invalid SSL certificate
    Then the service should attempt SSL/TLS handshake
    And the service should perform certificate validation
    And the service should detect certificate validation failure
    And service should abort upload and log security validation failure
    And the service should not proceed with upload
    And security failure telemetry should be generated
    And the service should exit with security error code
    And no data should be transmitted to untrusted server

  @missing_certificates @security @negative
  Scenario: Missing SSL Certificates for mTLS
    Given the uploadSTBLogs service is initialized
    And the device properties file is present and valid
    And the HTTPS upload server is accessible
    And client certificate is missing or not accessible
    And log files are available for upload
    When I trigger a secure log upload request with missing certificates
    Then the service should attempt to load client certificate
    And the service should detect certificate file missing
    And the service should log certificate loading error
    And the service should fail gracefully without crash
    And no upload attempt should be made
    And security failure telemetry should be generated
    And the service should exit with certificate error code

  @path_validation @security @negative
  Scenario: Path Traversal Attack Prevention
    Given the uploadSTBLogs service is initialized
    And the device properties file contains malicious paths
    And the paths contain directory traversal sequences
    When the service attempts to read configuration
    Then the service should validate all file paths
    And the service should detect path traversal attempt
    And the service should reject malicious paths
    And the service should log security violation
    And the service should fail safely without processing malicious paths
    And security failure telemetry should be generated
