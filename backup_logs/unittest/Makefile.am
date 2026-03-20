##
## Copyright 2024 Comcast Cable Communications Management, LLC
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
##

AUTOMAKE_OPTIONS = subdir-objects

# Extra files to distribute
EXTRA_DIST = Makefile.config_manager \
            README_config_manager_tests.md

# Define the test executables
bin_PROGRAMS = special_files_gtest config_manager_gtest

# Common include directories
COMMON_CPPFLAGS = -I../include -I../../include -I../../../include -I/usr/include/cjson \
                  -I/usr/include -I/usr/include/gtest -I/usr/local/include \
                  -I/usr/local/include/gtest -DGTEST_ENABLE

AM_CPPFLAGS = -I$(top_srcdir)/include -I../include -I../../include -I/usr/include
AM_CXXFLAGS = -std=c++14

# Common libraries
COMMON_LDADD = -lgtest -lgmock -lpthread -lgcov

# Common compiler flags
COMMON_CXXFLAGS = -fprofile-arcs -ftest-coverage -fpermissive -Wno-write-strings -Wno-unused-result

# Define source files for each test
special_files_gtest_SOURCES = special_files_gtest.cpp

special_files_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
special_files_gtest_LDADD = $(COMMON_LDADD)
special_files_gtest_LDFLAGS = -Wl,--wrap=fopen -Wl,--wrap=fgets -Wl,--wrap=fclose -Wl,--wrap=remove
special_files_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
special_files_gtest_CFLAGS = $(COMMON_CXXFLAGS)

# Config manager test configuration
config_manager_gtest_SOURCES = config_manager_gtest.cpp ../src/config_manager.c

config_manager_gtest_CPPFLAGS = $(COMMON_CPPFLAGS) \
                               -DRDK_LOG_FATAL=0 \
                               -DRDK_LOG_ERROR=1 \
                               -DRDK_LOG_WARN=2 \
                               -DRDK_LOG_NOTICE=3 \
                               -DRDK_LOG_INFO=4 \
                               -DRDK_LOG_DEBUG=5 \
                               -DRDK_LOG_TRACE1=6 \
                               -DRDK_LOG_TRACE2=7 \
                               -DRDK_LOG_TRACE3=8 \
                               -DRDK_LOG_TRACE4=9 \
                               -DRDK_LOG_TRACE5=10 \
                               -DRDK_LOG_TRACE6=11 \
                               -DRDK_LOG_TRACE7=12 \
                               -DRDK_LOG_TRACE8=13 \
                               -DRDK_LOG_TRACE9=14 \
                               -DLOG_BACKUP_LOGS=\"LOG.RDK.BACKUPLOGS\" \
                               -DUTILS_SUCCESS=0
config_manager_gtest_LDADD = $(COMMON_LDADD)
config_manager_gtest_LDFLAGS = -Wl,--wrap=RDK_LOG \
                              -Wl,--wrap=getIncludePropertyData \
                              -Wl,--wrap=getDevicePropertyData
config_manager_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
config_manager_gtest_CFLAGS = $(COMMON_CXXFLAGS)
