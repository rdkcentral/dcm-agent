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
bin_PROGRAMS = dcm_main_gtest

# Common include directories
COMMON_CPPFLAGS = -I/usr/include/cjson -I../ -I../../ -I/usr/include -I../include -I./mocks\
                  -I/usr/include/gtest -I/usr/local/include -I/usr/local/include/gtest -DGTEST_ENABLE -DHAS_MAINTENANCE_MANAGER

AM_CPPFLAGS =  -I$(top_srcdir)/unittest/mocks -I$(top_srcdir)/include -I$(top_srcdir)/mocks -I$(top_srcdir) -I/usr/include 
AM_CXXFLAGS = -std=c++14

# Common libraries
COMMON_LDADD = -lgtest -lgmock -lpthread -lcurl -lcjson -lssl -lcrypto -lgcov

# Common compiler flags
COMMON_CXXFLAGS = -fprofile-arcs -ftest-coverage -fpermissive -Wno-write-strings -Wno-unused-result 

# Define source files for each test

dcm_main_gtest_SOURCES = dcm_utils_gtest.cpp ../dcm_utils.c dcm_schedjob_gtest.cpp ../dcm_schedjob.c dcm_cronparse_gtest.cpp ../dcm_cronparse.c dcm_parseconf_gtest.cpp ../dcm_parseconf.c dcm_rbus_gtest.cpp  ../dcm_rbus.c ./mocks/mockrbus.cpp  dcm_gtest.cpp ./dcm.c 
dcm_main_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
dcm_main_gtest_LDADD = $(COMMON_LDADD)
dcm_main_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
dcm_main_gtest_CFLAGS = $(COMMON_CXXFLAGS)




