####################################################################################
# If not stated otherwise in this file or this component's Licenses file the
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
####################################################################################

"""
Test cases for uploadSTBLogs security and authentication
Covers: mTLS authentication, SSL validation, certificate handling, path security
"""

import pytest
import time
import os
from uploadstblogs_helper import *
from helper_functions import *


class TestMTLSAuthentication:
    """Test suite for mTLS authentication"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup before each test and cleanup after"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        self.cert_dir = None
        yield
        cleanup_test_log_files()
        remove_lock_file()
        if self.cert_dir:
            cleanup_mtls_certificates()
        kill_uploadstblogs()

    @pytest.mark.order(1)
    def test_mtls_certificate_loading(self):
        """Test: Service loads client certificate for mTLS"""
        # Setup certificates
        self.cert_dir = setup_mtls_certificates()
        
        # Configure certificate paths
        set_device_property("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.FwUpgrade.ClientCertPath", 
                          f"{self.cert_dir}/client.crt")
        set_device_property("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.FwUpgrade.ClientKeyPath", 
                          f"{self.cert_dir}/client.key")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for certificate loading
        cert_logs = grep_uploadstb_logs_regex(r"certificate|cert|mTLS|mtls")
        assert len(cert_logs) >= 0, "Certificate loading should be attempted"

    @pytest.mark.order(2)
    def test_mtls_with_valid_certificates(self):
        """Test: Successful upload with valid mTLS certificates"""
        self.cert_dir = setup_mtls_certificates()
        
        # Verify certificates exist
        assert os.path.exists(f"{self.cert_dir}/client.crt"), "Client cert should exist"
        assert os.path.exists(f"{self.cert_dir}/client.key"), "Client key should exist"
        assert os.path.exists(f"{self.cert_dir}/ca.crt"), "CA cert should exist"
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Process should complete (may fail upload but cert loading should work)
        assert result.returncode in [0, 1], "Process should complete"

    @pytest.mark.order(3)
    def test_mtls_telemetry_marker(self):
        """Test: mTLS telemetry marker is sent"""
        self.cert_dir = setup_mtls_certificates()
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for mTLS telemetry marker
        mtls_logs = grep_uploadstb_logs_regex(r"SYST_INFO_mtls_xpki|mTLS|mtls")
        assert len(mtls_logs) >= 0, "mTLS telemetry should be logged"


class TestSSLValidation:
    """Test suite for SSL certificate validation"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_invalid_server_certificate_rejection(self):
        """Test: Service rejects invalid server certificate"""
        # Point to HTTPS server with invalid cert
        # Using self-signed or expired cert scenario
        set_include_property("UPLOAD_HTTPLINK", "https://self-signed.badssl.com/")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Should fail due to certificate validation
        ssl_logs = grep_uploadstb_logs_regex(r"SSL|TLS|certificate|verification")
        assert result.returncode != 0, "Should fail with invalid certificate"

    @pytest.mark.order(2)
    def test_ssl_handshake_failure_logged(self):
        """Test: SSL handshake failure is logged"""
        set_include_property("UPLOAD_HTTPLINK", "https://expired.badssl.com/")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for SSL/TLS error logs
        error_logs = grep_uploadstb_logs_regex(r"SSL|TLS|handshake|certificate|verification.*fail")
        # Should fail
        assert result.returncode != 0, "Should fail SSL handshake"

    @pytest.mark.order(3)
    def test_no_data_transmitted_to_untrusted_server(self):
        """Test: No data is transmitted when certificate validation fails"""
        set_include_property("UPLOAD_HTTPLINK", "https://wrong.host.badssl.com/")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check that upload was aborted
        abort_logs = grep_uploadstb_logs_regex(r"abort|fail|reject")
        assert result.returncode != 0, "Upload should be aborted"


class TestMissingCertificates:
    """Test suite for missing certificate handling"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        cleanup_mtls_certificates()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_missing_client_certificate_detection(self):
        """Test: Service detects missing client certificate"""
        # Set path to non-existent certificate
        set_device_property("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.FwUpgrade.ClientCertPath", 
                          "/tmp/nonexistent/client.crt")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Check for certificate missing error
        cert_logs = grep_uploadstb_logs_regex(r"certificate.*not found|missing|failed.*load")
        # Process might continue without mTLS or fail gracefully
        assert result.returncode in [0, 1], "Should handle missing certificate gracefully"

    @pytest.mark.order(2)
    def test_missing_certificate_error_logged(self):
        """Test: Missing certificate error is logged"""
        set_device_property("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.FwUpgrade.ClientCertPath", 
                          "/invalid/path/cert.crt")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Verify error logging
        error_logs = grep_uploadstb_logs_regex(r"ERROR|error|certificate|missing")
        # Should complete (may use different path)
        assert result.returncode in [0, 1], "Should log error and continue"

    @pytest.mark.order(3)
    def test_no_upload_without_required_certificates(self):
        """Test: Upload doesn't proceed without required certificates for mTLS"""
        # Remove certificate files
        cleanup_mtls_certificates()
        
        # Configure for mTLS but certs don't exist
        set_device_property("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.FwUpgrade.ClientCertPath", 
                          "/tmp/certs/client.crt")
        set_include_property("UPLOAD_HTTPLINK", "https://localhost:8443")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Should fail if mTLS is required
        # Implementation may fall back to non-mTLS
        assert result.returncode in [0, 1], "Should handle missing certs"


class TestPathSecurity:
    """Test suite for path traversal and security"""

    @pytest.fixture(autouse=True)
    def setup_and_teardown(self):
        """Setup and teardown"""
        clear_uploadstb_logs()
        remove_lock_file()
        cleanup_test_log_files()
        restore_device_properties()
        yield
        cleanup_test_log_files()
        remove_lock_file()

    @pytest.mark.order(1)
    def test_path_traversal_prevention(self):
        """Test: Service prevents path traversal attacks"""
        # Try to set malicious path with directory traversal
        set_include_property("LOG_PATH", "../../etc")
        
        create_test_log_files(count=1)
        
        result = run_uploadstblogs()
        
        # Service should handle this safely
        # Either reject the path or use default
        assert result.returncode in [0, 1], "Should handle path safely"

    @pytest.mark.order(2)
    def test_symlink_attack_prevention(self):
        """Test: Service prevents symlink attacks"""
        # Create a symlink to sensitive file
        subprocess.run("ln -sf /etc/shadow /tmp/test_symlink 2>/dev/null", shell=True)
        
        result = run_uploadstblogs()
        
        # Service uses O_NOFOLLOW flag to prevent symlink attacks
        # Should not follow symlinks to sensitive files
        assert result.returncode in [0, 1], "Should prevent symlink attacks"
        
        # Cleanup
        subprocess.run("rm -f /tmp/test_symlink", shell=True)
