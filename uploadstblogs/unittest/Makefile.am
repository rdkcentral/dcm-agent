##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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
##########################################################################

AUTOMAKE_OPTIONS = subdir-objects

# Define the test executables
bin_PROGRAMS = context_manager_gtest md5_utils_gtest validation_gtest strategy_selector_gtest \
               path_handler_gtest archive_manager_gtest upload_engine_gtest \
               cleanup_handler_gtest verification_gtest \
               rbus_interface_gtest uploadstblogs_gtest event_manager_gtest \
               retry_logic_gtest strategies_gtest \
               strategy_handler_gtest uploadlogsnow_gtest

# Common include directories
COMMON_CPPFLAGS = -std=c++11 -I. -I/usr/include/cjson -I../ -I../../ -I/usr/include -I../include -I./mocks \
                  -I../src -I$(top_srcdir)/include -I$(top_srcdir)/../common_utilities/utils \
                  -I$(top_srcdir)/../common_utilities/parsejson -I$(top_srcdir)/../common_utilities/dwnlutils \
                  -I$(top_srcdir)/../common_utilities/uploadutil \
                  -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/dbus-1.0 \
                  -I${PKG_CONFIG_SYSROOT_DIR}$(libdir)/dbus-1.0/include \
                  -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rbus \
                  -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
                  -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/sysmgr \
                  -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal \
                  -I/usr/include/gtest -I/usr/local/include -I/usr/local/include/gtest -DGTEST_ENABLE -DGTEST_BASIC -DEN_MAINTENANCE_MANAGER -DIARM_ENABLED

AM_CPPFLAGS = -I$(top_srcdir)/unittest/mocks -I$(top_srcdir)/include -I$(top_srcdir)/mocks -I$(top_srcdir) -I/usr/include 
AM_CXXFLAGS = -std=c++11

# Common libraries
COMMON_LDADD = -lgtest -lgmock -lpthread -lcurl -lcjson -lssl -lcrypto -lgcov -lz -lrbus \
               -lfwutils -lrdkloggers

# Common compiler flags
COMMON_CXXFLAGS = -frtti -fprofile-arcs -ftest-coverage -fpermissive -Wno-write-strings -Wno-unused-result -Wno-error -Wno-format-truncation

# Define source files for each test

context_manager_gtest_SOURCES = context_manager_gtest.cpp ./mocks/mock_rdk_utils.cpp ./mocks/mock_rbus.cpp ./mocks/mock_file_operations.cpp
context_manager_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
context_manager_gtest_LDADD = $(COMMON_LDADD)
context_manager_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
context_manager_gtest_CFLAGS = $(COMMON_CXXFLAGS)

md5_utils_gtest_SOURCES = md5_utils_gtest.cpp
md5_utils_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
md5_utils_gtest_LDADD = $(COMMON_LDADD)
md5_utils_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
md5_utils_gtest_CFLAGS = $(COMMON_CXXFLAGS)

validation_gtest_SOURCES = validation_gtest.cpp ./mocks/mock_rdk_utils.cpp ./mocks/mock_file_operations.cpp
validation_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
validation_gtest_LDADD = $(COMMON_LDADD)
validation_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
validation_gtest_CFLAGS = $(COMMON_CXXFLAGS)

strategy_selector_gtest_SOURCES = strategy_selector_gtest.cpp ./mocks/mock_rdk_utils.cpp ./mocks/mock_file_operations.cpp
strategy_selector_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
strategy_selector_gtest_LDADD = $(COMMON_LDADD)
strategy_selector_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
strategy_selector_gtest_CFLAGS = $(COMMON_CXXFLAGS)

path_handler_gtest_SOURCES = path_handler_gtest.cpp ./mocks/mock_curl.cpp
path_handler_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
path_handler_gtest_LDADD = $(COMMON_LDADD)
path_handler_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
path_handler_gtest_CFLAGS = $(COMMON_CXXFLAGS)

archive_manager_gtest_SOURCES = archive_manager_gtest.cpp ./mocks/mock_file_operations.cpp
archive_manager_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
archive_manager_gtest_LDADD = $(COMMON_LDADD)
archive_manager_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
archive_manager_gtest_CFLAGS = $(COMMON_CXXFLAGS)

upload_engine_gtest_SOURCES = upload_engine_gtest.cpp ./mocks/mock_curl.cpp
upload_engine_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
upload_engine_gtest_LDADD = $(COMMON_LDADD)
upload_engine_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
upload_engine_gtest_CFLAGS = $(COMMON_CXXFLAGS)

cleanup_handler_gtest_SOURCES = cleanup_handler_gtest.cpp ./mocks/mock_file_operations.cpp
cleanup_handler_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
cleanup_handler_gtest_LDADD = $(COMMON_LDADD)
cleanup_handler_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
cleanup_handler_gtest_CFLAGS = $(COMMON_CXXFLAGS)

verification_gtest_SOURCES = verification_gtest.cpp
verification_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
verification_gtest_LDADD = $(COMMON_LDADD)
verification_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
verification_gtest_CFLAGS = $(COMMON_CXXFLAGS)

rbus_interface_gtest_SOURCES = rbus_interface_gtest.cpp ./mocks/mock_rbus.cpp
rbus_interface_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
rbus_interface_gtest_LDADD = $(COMMON_LDADD)
rbus_interface_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
rbus_interface_gtest_CFLAGS = $(COMMON_CXXFLAGS)

uploadstblogs_gtest_SOURCES = uploadstblogs_gtest.cpp ./mocks/mock_rdk_utils.cpp ./mocks/mock_rbus.cpp ./mocks/mock_curl.cpp
uploadstblogs_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
uploadstblogs_gtest_LDADD = $(COMMON_LDADD)
uploadstblogs_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
uploadstblogs_gtest_CFLAGS = $(COMMON_CXXFLAGS)

event_manager_gtest_SOURCES = event_manager_gtest.cpp
event_manager_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
event_manager_gtest_LDADD = $(COMMON_LDADD)
event_manager_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
event_manager_gtest_CFLAGS = $(COMMON_CXXFLAGS)

retry_logic_gtest_SOURCES = retry_logic_gtest.cpp
retry_logic_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
retry_logic_gtest_LDADD = $(COMMON_LDADD)
retry_logic_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
retry_logic_gtest_CFLAGS = $(COMMON_CXXFLAGS)

strategies_gtest_SOURCES = strategies_gtest.cpp
strategies_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
strategies_gtest_LDADD = $(COMMON_LDADD)
strategies_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
strategies_gtest_CFLAGS = $(COMMON_CXXFLAGS)

strategy_handler_gtest_SOURCES = strategy_handler_gtest.cpp
strategy_handler_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
strategy_handler_gtest_LDADD = $(COMMON_LDADD)
strategy_handler_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
strategy_handler_gtest_CFLAGS = $(COMMON_CXXFLAGS)

uploadlogsnow_gtest_SOURCES = uploadlogsnow_gtest.cpp ./mocks/mock_file_operations.cpp
uploadlogsnow_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
uploadlogsnow_gtest_LDADD = $(COMMON_LDADD)
uploadlogsnow_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
uploadlogsnow_gtest_CFLAGS = $(COMMON_CXXFLAGS)
