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

# Define the test executables
bin_PROGRAMS = special_files_gtest

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

.PHONY: check clean-coverage
clean-coverage:
        rm -f *.gcda *.gcno *.gcov
