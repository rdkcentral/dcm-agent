#
## Copyright 2020 RDK Management
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
bin_PROGRAMS = usb_log_file_manager_gtest usb_log_main_gtest usb_log_validation_gtest

# Common include directories
COMMON_CPPFLAGS = -I/usr/include/gtest -I/usr/local/include -I/usr/local/include/gtest \
                  -I../include -I../ -I/usr/include \
                  -DGTEST_ENABLE

AM_CPPFLAGS = $(COMMON_CPPFLAGS)
AM_CXXFLAGS = -std=c++14

# Common libraries
COMMON_LDADD = -lgtest -lgmock -lpthread -lcurl -lcjson -lssl -lcrypto -lgcov -lrdkloggers -lfwutils

# Common compiler flags
COMMON_CXXFLAGS = -fprofile-arcs -ftest-coverage -fpermissive -Wno-write-strings -Wno-unused-result

# Define source files for each test

# USB Log File Manager GTest
usb_log_file_manager_gtest_SOURCES = usb_log_file_manager_gtest.cpp \
                                     ../src/usb_log_file_manager.c ../src/usb_log_utils.c  ../../uploadstblogs/src/file_operations.c
#../../uploadstblogs/unittest/mocks/mock_file_operations.cpp

usb_log_file_manager_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
usb_log_file_manager_gtest_LDADD = $(COMMON_LDADD)
usb_log_file_manager_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
usb_log_file_manager_gtest_CFLAGS = $(COMMON_CXXFLAGS)

# USB Log Main GTest
usb_log_main_gtest_SOURCES = usb_log_main_gtest.cpp \
                             ../src/usb_log_main.c \
                             ../src/usb_log_validation.c \
                             ../src/usb_log_file_manager.c \
                             ../src/usb_log_archive.c \
                             ../src/usb_log_utils.c \
                             ../../uploadstblogs/unittest/mocks/mock_file_operations.cpp

usb_log_main_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
usb_log_main_gtest_LDADD = $(COMMON_LDADD)
usb_log_main_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
usb_log_main_gtest_CFLAGS = $(COMMON_CXXFLAGS)

# USB Log Validation GTest
usb_log_validation_gtest_SOURCES = usb_log_validation_gtest.cpp \
                                   ../src/usb_log_validation.c \
                                   ../src/usb_log_utils.c

usb_log_validation_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
usb_log_validation_gtest_LDADD = $(COMMON_LDADD)
usb_log_validation_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
usb_log_validation_gtest_CFLAGS = $(COMMON_CXXFLAGS)
