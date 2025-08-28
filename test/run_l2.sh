#!/bin/sh

####################################################################################
# ...existing copyright header...
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

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_reboot_false_test.json test/functional-tests/tests/test_log_upload_onreboot_true_case.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_reboot_true_test.json test/functional-tests/tests/test_log_upload_onreboot_false_case.py

if ! grep -q "ENABLE_MAINTENANCE=" /etc/device.properties; then
    echo "ENABLE_MAINTENANCE=true" >> /etc/device.properties
fi

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_reboot_MM_test.json test/functional-tests/tests/test_log_upload_onreboot_MM_case.py

pytest -v --json-report --json-report-summary --json-report-file $RESULT_DIR/log_upload_cron_NULL_test.json test/functional-tests/tests/test_log_upload_cron_NULL_case.py
