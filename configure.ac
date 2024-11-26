##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
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
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(DCM, 1.0, bhanuprakash.chintaginjala@sky.uk)
AC_CONFIG_SRCDIR([dcm.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE(DCM, DCM)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AM_PROG_LIBTOOL

# Set shared libraries
AC_DISABLE_STATIC
AC_ENABLE_SHARED

# Checks for header files.

AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
                                #include <stdio.h>
                                #undef PACKAGE
                                #undef VERSION
                                #undef HAVE_STDLIB_H
                                #include "rdk_debug.h"
                ]])
        ],[
                AC_MSG_RESULT([RDK logging enabled: yes])
                dcmd_LDFLAGS="$dcmd_LDFLAGS -lrdkloggers"
                dcmd_CFLAGS="$dcmd_CFLAGS -DRDK_LOGGER_ENABLED"
        ],[
                AC_MSG_RESULT([RDK logging enabled: no])
        ])
AC_CHECK_LIB([rbus], [rbus_open], [dcmd_LDFLAGS="$dcmd_LDFLAGS -lrbus"], [])
AC_CHECK_LIB([cjson],[cJSON_Parse], [dcmd_LDFLAGS="$dcmd_LDFLAGS -lcjson"], [])
AC_SUBST([dcmd_LDFLAGS])
AC_SUBST([dcmd_CFLAGS])

# Checks for typedefs, structures, and compiler characteristics.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
