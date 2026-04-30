#!/bin/sh
####################################################################################
# If not stated otherwise in this file or this component's Licenses.txt file the
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

# Test runner for uploadSTBLogs L2 tests

export top_srcdir=`pwd`
RESULT_DIR="/tmp/l2_test_report/uploadstblogs"
TEST_DIR="functional-tests/tests"

# Create result directory
mkdir -p "$RESULT_DIR"

# Setup debug logging
echo "LOG.RDK.DEFAULT" >> /etc/debug.ini

# Ensure properties files exist

if grep -q '^RDK_PROFILE=' /etc/device.properties; then
  sed -i 's/^RDK_PROFILE=.*/RDK_PROFILE=TV/' /etc/device.properties
else
  echo 'RDK_PROFILE=TV' >> /etc/device.properties
fi

if ! grep -q "LOG_PATH=/opt/logs/" /etc/include.properties; then
    echo "LOG_PATH=/opt/logs/" >> /etc/include.properties
fi

if ! grep -q "PERSISTENT_PATH=/opt/" /etc/include.properties; then
    echo "PERSISTENT_PATH=/opt/" >> /etc/include.properties
fi

# Ensure device properties exist
if [ ! -f /etc/device.properties ]; then
    touch /etc/device.properties
fi

if ! grep -q "DEVICE_TYPE=" /etc/device.properties; then
    echo "DEVICE_TYPE=mediaclient" >> /etc/device.properties
fi

if ! grep -q "BUILD_TYPE=" /etc/device.properties; then
    echo "BUILD_TYPE=dev" >> /etc/device.properties
fi

echo "AA:BB:CC:dd:EE:FF" >> /tmp/.estb_mac

cd /usr/common_utilities
sed -i '/file_upload\.sslverify/s/= 1;/= 0;/' uploadutils/mtls_upload.c
sed -i 's/\(ret_code = setCommonCurlOpt(curl, s3url, NULL, \)true\()\)/\1false\2/g' uploadutils/uploadUtil.c
sed -i '/if (auth) {/,/}/s/^/\/\/ /' uploadutils/uploadUtil.c
cd -

echo pwd

# Create log directories
mkdir -p /opt/logs
mkdir -p /opt/logs/PreviousLogs
touch /opt/logs/PreviousLogs/logupload.log

echo "====================================="
echo "Running uploadSTBLogs L2 Test Suite"
echo "====================================="

# Run test suites

echo ""
echo "1. Running usbLogupload Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/usb_logupload.json test/functional-tests/tests/test_usb_logupload.py

echo ""
echo "2. Running UploadLogsNow Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/uploadLogsNow.json test/functional-tests/tests/test_uploadLogsNow.py

echo ""
echo "3. Running Error Handling Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/error_handling.json test/functional-tests/tests/test_uploadstblogs_error_handling.py


mkdir -p /opt/logs
mkdir -p /opt/logs/PreviousLogs

echo ""
echo "4. Running Normal Upload Tests..."
mkdir -p /opt/logs/PreviousLogs
pytest -v --json-report --json-report-summary \
        --json-report-file $RESULT_DIR/upload_normal.json test/functional-tests/tests/test_uploadstblogs_normal_upload.py


echo ""
echo "5. Running Retry Logic Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/retry_logic.json test/functional-tests/tests/test_uploadstblogs_retry_logic.py

echo ""
echo "6. Running Security Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/security.json test/functional-tests/tests/test_uploadstblogs_security.py

echo ""
echo "7. Running Resource Management Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/resource_management.json test/functional-tests/tests/test_uploadstblogs_resource_management.py

echo ""
echo "8. Running Upload Strategy Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/upload_strategies.json test/functional-tests/tests/test_uploadstblogs_upload_strategies.py


echo ""
echo "9. Running backup Engine Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/backup_engine.json test/functional-tests/tests/test_backup_engine.py

echo ""
echo "10. Running backup configuration manager Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/backuplog_config_manager.json test/functional-tests/tests/test_backuplog_config_manager.py

echo ""
echo "11. Running backup systemintegration Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/backuplogs_system_integration.json test/functional-tests/tests/test_backuplogs_system_integration.py


echo ""
echo "12. Running backup special file Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/test_backuplogs_special_files.json test/functional-tests/tests/test_backuplogs_special_files.py

echo ""
echo "====================================="
echo "Test Execution Complete"
echo "====================================="
echo "Results saved to: $RESULT_DIR"
echo ""
