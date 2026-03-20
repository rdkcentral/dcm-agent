##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2024 Comcast Cable Communications Management, LLC
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

AUTOMAKE_OPTIONS = foreign

# Binary program
bin_PROGRAMS = backup_logs

backup_logs_SOURCES = \
    src/backup_logs.c \
    src/backup_engine.c \
    src/config_manager.c \
    src/special_files.c \
    src/sys_integration.c

backup_logs_CPPFLAGS = -I$(top_srcdir)/include \
                       -I$(top_srcdir)/backup_logs/include \
                       -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include \
                       -DRDK_LOGGER_EXT

backup_logs_CFLAGS = -Wall -Wextra -std=c99

backup_logs_LDADD = -lm -lrdkloggers -lfwutils -lsystemd -lsecure_wrapper

backup_logs_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib \
                      -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)

