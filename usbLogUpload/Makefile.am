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

usblogupload_CPPFLAGS = -I$(top_srcdir)/include \
                        -I${top_srcdir}/usbLogUpload/include \
                        -I$(top_srcdir)/uploadstblogs/include \
                        -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include \
                        -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/upload_util \
                        -DRDK_LOGGER_EXT

usblogupload_CFLAGS = -Wall -Wextra -std=c99

usblogupload_LDADD = \
    -luploadstblogs \
    -lrdkloggers \
    -ldwnlutil \
    -lfwutils \
    -lz \
    -lpthread

usblogupload_LDFLAGS = \
    -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir) \
    -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib

# Install headers
include_HEADERS = \
    include/usb_log_main.h \
    include/usb_log_validation.h \
    include/usb_log_file_manager.h \
    include/usb_log_archive.h \
    include/usb_log_utils.h
