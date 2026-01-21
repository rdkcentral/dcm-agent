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

export top_srcdir=`pwd`
RESULT_DIR="/tmp/l2_test_report"
LOCAL_DIR="/usr/local"
RBUS_INSTALL_DIR="/usr/local"

mkdir -p "$RESULT_DIR"
echo "LOG.RDK.DCM = ALL FATAL ERROR WARNING NOTICE INFO DEBUG" >> /etc/debug.ini

if ! grep -q "LOG_PATH=/opt/logs/" /etc/include.properties; then
    echo "LOG_PATH=/opt/logs/" >> /etc/include.properties
fi

if ! grep -q "PERSISTENT_PATH=/opt/" /etc/include.properties; then
    echo "PERSISTENT_PATH=/opt/" >> /etc/include.properties
fi

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/run_dcm-agent.json test/functional-tests/tests/test_start_dcm-agent.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/bootup_sequence.json test/functional-tests/tests/test_bootup_sequence.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/file_existence.json test/functional-tests/tests/test_existence_of_dcmsettingsFile.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_reboot_true_test.json test/functional-tests/tests/test_log_upload_onreboot_true_case.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_reboot_false_test.json test/functional-tests/tests/test_log_upload_onreboot_false_case.py

if ! grep -q "ENABLE_MAINTENANCE=" /etc/device.properties; then
    echo "ENABLE_MAINTENANCE=true" >> /etc/device.properties
fi

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_reboot_MM_test.json test/functional-tests/tests/test_log_upload_onreboot_MM_case.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_cron_NULL_test.json test/functional-tests/tests/test_log_upload_cron_NULL_case.py
