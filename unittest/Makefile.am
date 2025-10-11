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

AUTOMAKE_OPTIONS = subdir-objects 
ACLOCAL_AMFLAGS = -I m4

# Define the test executables
#bin_PROGRAMS = dcm_cronparse_gtest dcm_utils_gtest
bin_PROGRAMS = dcm_utils_gtest dcm_schedjob_gtest dcm_cronparse_gtest dcm_parseconf_gtest
# Common include directories
COMMON_CPPFLAGS = -I/usr/include/cjson -I./include  -I../ -I../../ -I/usr/include -I../include -I../mocks -I../src -I../include    \
                  -I/usr/include/gtest -I/usr/local/include/gtest -DGTEST_ENABLE 

AM_CPPFLAGS =  -I$(top_srcdir)/unittest/mocks -I$(top_srcdir)/include -I$(top_srcdir)/mocks -I$(top_srcdir) -I/usr/include 
AM_CXXFLAGS = -std=c++14

# Common libraries
COMMON_LDADD = -lgtest -lgmock\
               -lpthread -lcurl -lcjson -lssl -lcrypto -lgcov

# Common compiler flags
COMMON_CXXFLAGS = -fprofile-arcs -ftest-coverage -fpermissive -Wno-write-strings -Wno-unused-result 
#-Wno-jump-misses-init

# Define source files for each test
#dcm_cronparse_gtest_SOURCES = dcm_cronparse_gtest.cpp ../dcm_cronparse.c
dcm_utils_gtest_SOURCES = dcm_utils_gtest.cpp ../dcm_utils.c
dcm_schedjob_gtest_SOURCES = dcm_schedjob_gtest.cpp ../dcm_schedjob.c
dcm_cronparse_gtest_SOURCES = dcm_cronparse_gtest.cpp 
#../dcm_cronparse.c
dcm_parseconf_gtest_SOURCES = dcm_parseconf_gtest.cpp 
#../dcm_parseconf.c

#rdm_main_gtest_SOURCES = rdm_main_gtest.cpp ../rdm_main.c 
#rdm_curl_gtest_SOURCES = rdm_curl_gtest.cpp  ../src/rdm_curldownload.c
#rdm_utils_gtest_SOURCES = rdm_utils_gtest.cpp mocks/mock_iarm_bus.cpp  ../src/rdm_utils.c
#rdm_json_gtest_SOURCES = rdm_json_gtest.cpp ../src/rdm_jsonquery.c
#rdm_download_gtest_SOURCES = rdm_download_gtest.cpp mocks/mock_iarm_bus.cpp  ../rdm_main.c ../src/rdm_download.c ../src/rdm_downloadmgr.c ../src/rdm_utils.c ../src/rdm_downloadutils.c ../src/rdm_rbus.c ../src/rdm_jsonquery.c ../src/rdm_downloadverapp.c ../src/rdm_packagemgr.c
#rdm_downloadutils_gtest_SOURCES = rdm_downloadutils_gtest.cpp ../rdm_main.c mocks/mock_iarm_bus.cpp ../src/rdm_downloadutils.c ../src/rdm_rbus.c ../src/rdm_utils.c
#rdm_rbus_gtest_SOURCES = rdm_rbus_gtest.cpp ../src/rdm_rbus.c
#rdm_openssl_gtest_SOURCES = rdm_openssl_gtest.cpp ../rdm_main.c  ../src/rdm_openssl.c
#rdm_usbinstall_gtest_SOURCES = rdm_usbinstall_gtest.cpp ../src/rdm_usbinstall.c
#dcm_cronparse_gtest_SOURCES = dcm_cronparse_gtest.cpp ../dcm_cronparse.c

dcm_cronparse_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
dcm_cronparse_gtest_LDADD = $(COMMON_LDADD)
dcm_cronparse_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
dcm_cronparse_gtest_CFLAGS = $(COMMON_CXXFLAGS)

dcm_utils_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
dcm_utils_gtest_LDADD = $(COMMON_LDADD)
dcm_utils_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
dcm_utils_gtest_CFLAGS = $(COMMON_CXXFLAGS)

dcm_schedjob_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
dcm_schedjob_gtest_LDADD = $(COMMON_LDADD)
dcm_schedjob_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
dcm_schedjob_gtest_CFLAGS = $(COMMON_CXXFLAGS)

dcm_parseconf_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
dcm_parseconf_gtest_LDADD = $(COMMON_LDADD)
dcm_parseconf_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
dcm_parseconf_gtest_CFLAGS = $(COMMON_CXXFLAGS)

#rdm_curl_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
#rdm_curl_gtest_LDADD = $(COMMON_LDADD)
#rdm_curl_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
#rdm_curl_gtest_CFLAGS = $(COMMON_CXXFLAGS)

