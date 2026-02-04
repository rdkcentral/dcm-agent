##############################################################################
# Copyright 2020 RDK Management
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
##############################################################################

# Automake file for USB Log Upload module

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = usblogupload

# Main executable
usblogupload_SOURCES = \
    src/usb_log_main.c \
    src/usb_log_validation.c \
    src/usb_log_file_manager.c \
    src/usb_log_archive.c \
    src/usb_log_utils.c

usblogupload_CPPFLAGS = -I$(top_srcdir)/include
usblogupload_CFLAGS = -Wall -Wextra -std=c99

# Install headers
include_HEADERS = \
    include/usb_log_main.h \
    include/usb_log_validation.h \
    include/usb_log_file_manager.h \
    include/usb_log_archive.h \
    include/usb_log_utils.h

# Documentation
EXTRA_DIST = \
    docs/usb-log-upload-requirements.md \
    docs/usb-log-upload-hld.md \
    docs/usb-log-upload-flowcharts.md

# Test programs (optional)
if BUILD_TESTS
check_PROGRAMS = test_usblogupload

test_usblogupload_SOURCES = \
    src/test/test_main.c \
    src/usb_log_validation.c \
    src/usb_log_file_manager.c \
    src/usb_log_archive.c \
    src/usb_log_utils.c

test_usblogupload_CPPFLAGS = -I$(top_srcdir)/include
test_usblogupload_CFLAGS = -Wall -Wextra -std=c99

TESTS = test_usblogupload
endif