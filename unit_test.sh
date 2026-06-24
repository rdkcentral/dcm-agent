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
export LD_LIBRARY_PATH="/usr/local/lib:$TOP_DIR/uploadstblogs/src/.libs:$LD_LIBRARY_PATH"
echo "RDK_PROFILE=TV" >> /etc/device.properties

cd "$TOP_DIR/unittest" || exit 1
cp mocks/mockrbus.h /usr/local/include
cp "$TOP_DIR"/uploadstblogs/include/*.h /usr/local/include
automake --add-missing
autoreconf --install
./configure
make clean
make

cd "$TOP_DIR" || exit 1
sh cov_build.sh
git clone https://github.com/rdkcentral/iarmmgrs.git
cp iarmmgrs/sysmgr/include/sysMgr.h /usr/local/include
cp iarmmgrs/maintenance/include/maintenanceMGR.h /usr/local/include
git clone https://github.com/rdkcentral/rdk_logger.git
cp rdk_logger/include/rdk_logger.h /usr/local/include

cd "$TOP_DIR/uploadstblogs/unittest" || exit 1
automake --add-missing
autoreconf --install
./configure
make clean
make

cd "$TOP_DIR/usbLogUpload/unittest" || exit 1
automake --add-missing
autoreconf --install
./configure
make clean
make

cd "$TOP_DIR/backup_logs/unittest" || exit 1
automake --add-missing
autoreconf --install
./configure
make clean
make

echo "RDK_PROFILE=TV" >> /etc/device.properties
fail=0
cd $TOP_DIR/unittest/

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
  ./../usbLogUpload/unittest/usb_log_file_manager_gtest \
  ./../usbLogUpload/unittest/usb_log_validation_gtest \
  ./../usbLogUpload/unittest/usb_log_utils_gtest \
  ./../usbLogUpload/unittest/usb_log_archive_gtest \
  ./../usbLogUpload/unittest/usb_log_main_gtest \
  ./../backup_logs/unittest/backup_engine_gtest \
  ./../backup_logs/unittest/backup_logs_gtest \
  ./../backup_logs/unittest/config_manager_gtest \
  ./../backup_logs/unittest/special_files_gtest \
  ./../backup_logs/unittest/sys_integration_gtest
  
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
    echo "Generating coverage report"
    echo "********************"
    COV_DIR="$TOP_DIR/unittest"

    # Per-module capture
    lcov --capture --directory "$TOP_DIR/unittest" --output-file "$COV_DIR/dcm.info"
    lcov --capture --directory "$TOP_DIR/uploadstblogs" --output-file "$COV_DIR/uploadstblogs.info"
    lcov --capture --directory "$TOP_DIR/usbLogUpload" --output-file "$COV_DIR/usblogupload.info"
    lcov --capture --directory "$TOP_DIR/backup_logs" --output-file "$COV_DIR/backup_logs.info"

    # Per-module filter: strip system headers, test drivers, and mocks
    for info in dcm.info uploadstblogs.info usblogupload.info backup_logs.info; do
        lcov --remove "$COV_DIR/$info" '/usr/*' --output-file "$COV_DIR/$info"
        lcov --remove "$COV_DIR/$info" '*_gtest*' --output-file "$COV_DIR/$info"
        lcov --remove "$COV_DIR/$info" '*/mocks/*' --output-file "$COV_DIR/$info"
    done

    # Keep only production C sources for each module
    lcov --extract "$COV_DIR/dcm.info" "$TOP_DIR/dcm*.c" --output-file "$COV_DIR/dcm.info"
    lcov --extract "$COV_DIR/uploadstblogs.info" "$TOP_DIR/uploadstblogs/src/*.c" --output-file "$COV_DIR/uploadstblogs.info"
    lcov --extract "$COV_DIR/usblogupload.info" "$TOP_DIR/usbLogUpload/src/*.c" --output-file "$COV_DIR/usblogupload.info"
    lcov --extract "$COV_DIR/backup_logs.info" "$TOP_DIR/backup_logs/src/*.c" --output-file "$COV_DIR/backup_logs.info"

    # Build merge arguments: only include info files that have valid coverage records
    MERGE_ARGS=""
    for info in dcm.info uploadstblogs.info usblogupload.info backup_logs.info; do
        if grep -q "^DA:" "$COV_DIR/$info" 2>/dev/null; then
            MERGE_ARGS="$MERGE_ARGS -a $COV_DIR/$info"
        else
            echo "Skipping $info: no valid coverage records after filtering"
        fi
    done

    # Merge all non-empty modules into a single combined report
    if [ -n "$MERGE_ARGS" ]; then
        # shellcheck disable=SC2086
        lcov $MERGE_ARGS --output-file "$COV_DIR/combined.info"
        lcov --list "$COV_DIR/combined.info"
    else
        echo "WARNING: No coverage data found in any module; skipping combined report generation"
    fi
fi
