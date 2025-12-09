##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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
SUBDIRS = uploadstblogs/src

dcmd_CFLAGS += -fPIC -pthread

bin_PROGRAMS = dcmd

dcmd_SOURCES = dcm.c \
               dcm_utils.c \
               dcm_rbus.c \
               dcm_parseconf.c \
               dcm_schedjob.c \
               dcm_cronparse.c \
               $(NULL)

dcmd_LDFLAGS += -shared -fPIC $(GLIB_LIBS)
dcmd_LDADD = ${top_builddir}/uploadstblogs/src/libuploadstblogs.la

dcmd_CFLAGS += -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/dbus-1.0 \
               -I${PKG_CONFIG_SYSROOT_DIR}$(libdir)/dbus-1.0/include \
               -I${top_srcdir}/include \
               -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rbus \
               -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
               -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/sysmgr \
               -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal
