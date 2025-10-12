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

ENABLE_COV=true

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

fail=0

for test in \
  ./dcm_utils_gtest \
  ./dcm_schedjob_gtest \
  ./dcm_cronparse_gtest \
  ./dcm_parseconf_gtest \
  ./dcm_rbus_gtest
  
  #./rdm_main_gtest \
  #./rdm_utils_gtest \
  #./rdm_curl_gtest \
  #./rdm_json_gtest \
  #./rdm_download_gtest \
  #./rdm_downloadutils_gtest \
  #./rdm_rbus_gtest \
  #./rdm_openssl_gtest \
  #./rdm_usbinstall_gtest \
  
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
fi

echo "********************"
echo "**** CAPTURE DCM-AGENT COVERAGE DATA ****"
echo "********************"
if [ "$ENABLE_COV" = true ]; then
    echo "Generating coverage report"
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' --output-file coverage.info
    lcov --remove coverage.info './unittest/*' --output-file coverage.info
    lcov --list coverage.info
fi

cd $TOP_DIR
