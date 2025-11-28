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
#
# SPDX-License-Identifier: Apache-2.0

AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
AM_CFLAGS += -Wall -Werror $(curl_CFLAGS) $(CFLAGS) -DLIBRDKCERTSELECTOR

bin_PROGRAMS = uploadutil_test
uploadutil_test_SOURCES = main.c

uploadutil_test_CFLAGS = -I${top_srcdir}

uploadutil_test_LDADD = -lrfcapi -lrdkloggers -lpthread $(curl_LIBS) -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -luploadutil

