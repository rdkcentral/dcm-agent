echo "RDK_PROFILE=TV" >> /etc/device.properties


#!/bin/bash

#
## Copyright 2023 Comcast Cable Communications Management, LLC
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
## http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## SPDX-License-Identifier: Apache-2.0
#

ENABLE_COV=false

if [ "x$1" = "x--enable-cov" ]; then
      echo "Enabling coverage options"
      export CXXFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
      export CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
      export LDFLAGS="-lgcov --coverage"
      ENABLE_COV=true
fi
export TOP_DIR=`pwd`
export top_srcdir=`pwd`

cd unittest/
cp mocks/mockrbus.h /usr/local/include
cp ../uploadstblogs/include/*.h /usr/local/include
automake --add-missing
autoreconf --install

./configure

make clean
make

cd ../uploadstblogs/unittest
git clone https://github.com/rdkcentral/iarmmgrs.git
cp iarmmgrs/sysmgr/include/sysMgr.h /usr/local/include
cp iarmmgrs/maintenance/include/maintenanceMGR.h /usr/local/include
git clone https://github.com/rdkcentral/rdk_logger.git
cp rdk_logger/include/rdk_logger.h /usr/local/include

automake --add-missing
autoreconf --install

./configure

make clean
make

fail=0
cd -

for test in \
  ./dcm_utils_gtest \
  ./dcm_schedjob_gtest \
  ./dcm_cronparse_gtest \
  ./dcm_parseconf_gtest \
  ./dcm_rbus_gtest \
  ./dcm_gtest \
  ./../uploadstblogs/unittest/context_manager_gtest \
  ./../uploadstblogs/unittest/archive_manager_gtest \
  ./../uploadstblogs/unittest/md5_utils_gtest \
  ./../uploadstblogs/unittest/validation_gtest \
  ./../uploadstblogs/unittest/strategy_selector_gtest \
  ./../uploadstblogs/unittest/path_handler_gtest \
  ./../uploadstblogs/unittest/upload_engine_gtest \
  ./../uploadstblogs/unittest/cleanup_handler_gtest \
  ./../uploadstblogs/unittest/verification_gtest \
  ./../uploadstblogs/unittest/rbus_interface_gtest \
  ./../uploadstblogs/unittest/uploadstblogs_gtest \
  ./../uploadstblogs/unittest/event_manager_gtest \
  ./../uploadstblogs/unittest/retry_logic_gtest \
  ./../uploadstblogs/unittest/strategies_gtest \
  ./../uploadstblogs/unittest/strategy_handler_gtest \
  ./../uploadstblogs/unittest/uploadlogsnow_gtest \
  ./../usbLogUpload/unittest/usb_log_main_gtest \
  ./../usbLogUpload/unittest/usb_log_file_manager_gtest \
  ./../usbLogUpload/unittest/usb_log_validation_gtest
  
do
    $test
    status=$?
    if [ $status -ne 0 ]; then
        echo "Test $test failed with exit code $status"
        fail=1
    fi
done

if [ $fail -ne 0 ]; then
    echo "Some unit tests failed."
    exit 1
else
    echo "All unit tests passed."
fi

if [ "$ENABLE_COV" = true ]; then
    echo "********************"
    echo "**** CAPTURE DCM-AGENT COVERAGE DATA ****"
    echo "********************"
    echo "Generating coverage report"
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' --output-file coverage.info
    lcov --remove coverage.info "${PWD}/*" --output-file coverage.info
    lcov --list coverage.info
fi






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
echo "RDK_PROFILE=TV" >> /etc/device.properties

# Ensure properties files exist
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
    --json-report-file $RESULT_DIR/usb_logupload.json test/functional-tests/tests/test_usb_logupload.py‎

echo ""
echo "2. Running UploadLogsNow Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/uploadLogsNow.json test/functional-tests/tests/test_uploadLogsNow.py

echo ""
echo "3. Running Error Handling Tests..."
pytest -v --json-report --json-report-summary \
    --json-report-file $RESULT_DIR/error_handling.json test/functional-tests/tests/test_uploadstblogs_error_handling.py

echo "AA:BB:CC:dd:EE:FF" >> /tmp/.estb_mac

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
echo "====================================="
echo "Test Execution Complete"
echo "====================================="
echo "Results saved to: $RESULT_DIR"
echo ""
