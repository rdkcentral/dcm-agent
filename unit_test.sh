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
automake --add-missing
autoreconf --install

./configure

make clean
make

cd ../uploadstblogs/unittest
git clone https://github.com/rdkcentral/iarmmgrs.git
cp iarmmgrs/sysmgr/include/sysMgr.h /usr/local/include
cp iarmmgrs/maintenance/include/maintenanceMGR.h /usr/local/include

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
  ./../uploadstblogs/unittest/log_collector_gtest \
  ./../uploadstblogs/unittest/retry_logic_gtest \
  ./../uploadstblogs/unittest/strategy_dcm_gtest \
  ./../uploadstblogs/unittest/strategy_handler_gtest \
  ./../uploadstblogs/unittest/strategy_ondemand_gtest
  
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
