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

bin_PROGRAMS = uploadutil_test

uploadutil_test_SOURCES = main.c

AM_CFLAGS = -Wall -Werror $(curl_CFLAGS) $(CFLAGS) -DLIBRDKCERTSELECTOR
AM_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)

# Include local source dir and sysroot include dirs where DEPENDS-installed headers are staged
uploadutil_test_CFLAGS = -I${top_srcdir} \
    -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include \
    -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/upload_util

#uploadutil_test_LDADD = -shared -fPIC $(GLIB_LIBS)
uploadutil_test_LDADD = -lrdkloggers -lpthread $(curl_LIBS) -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -luploadutil -lrfcapi

#bin_PROGRAMS = uploadutil_test

#uploadutil_test_SOURCES = main.c

#AM_CFLAGS += -Wall -Werror $(curl_CFLAGS) $(CFLAGS) -DLIBRDKCERTSELECTOR
#AM_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)

#uploadutil_test_CFLAGS = -I${top_srcdir}

#uploadutil_test_LDADD = -shared -fPIC $(GLIB_LIBS)
#uploadutil_test_LDADD += -lrdkloggers -lpthread $(curl_LIBS) -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -luploadutil

