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

AC_ARG_ENABLE([t2api],
	      AS_HELP_STRING([--enable-t2api],[enables telemetry]),
	      [
	        case "${enableval}" in
                 yes) IS_TELEMETRY2_ENABLED=true
                      T2_EVENT_FLAG=" -DT2_EVENT_ENABLED ";;
                 no)  IS_TELEMETRY2_ENABLED=false ;;
               *) AC_MSG_ERROR([bad value ${enableval} for --enable-t2enable]) ;;
	       esac
	       ],
	   [echo "telemetry is disabled"])
AM_CONDITIONAL([IS_TELEMETRY2_ENABLED], [test x$IS_TELEMETRY2_ENABLED = xtrue])
AC_SUBST(T2_EVENT_FLAG)

IS_LIBRDKCERTSEL_ENABLED=""
IS_LIBRDKCERTSEL_ENABLED=""

AC_ARG_ENABLE([rdkcertselector],
        AS_HELP_STRING([--enable-rdkcertselector],[enables rdkcertselector replacement (default is no)]),
        [
          case "${enableval}" in
           yes) IS_LIBRDKCERTSEL_ENABLED=true
                LIBRDKCERTSEL_FLAG=" -DLIBRDKCERTSELECTOR ";;
           no)  IS_LIBRDKCERTSEL_ENABLED=false ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-rdkcertselector]) ;;
           esac
           ],
        [echo "rdkcertselector is disabled"])
AM_CONDITIONAL([IS_LIBRDKCERTSEL_ENABLED], [test x$IS_LIBRDKCERTSEL_ENABLED = xtrue])
AC_SUBST(LIBRDKCERTSEL_FLAG)

AC_ARG_ENABLE([mountutils],
        AS_HELP_STRING([--enable-mountutils],[enables mountutils replacement (default is no)]),
        [
          case "${enableval}" in
           yes) IS_LIBRDKCONFIG_ENABLED=true
                LIBRDKCONFIG_FLAG=" -DLIBRDKCONFIG_BUILD ";;
           no)  IS_LIBRDKCONFIG_ENABLED=false ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-mountutils]) ;;
           esac
           ],
        [echo "mountutils is disabled"])
AM_CONDITIONAL([IS_LIBRDKCONFIG_ENABLED], [test x$IS_LIBRDKCONFIG_ENABLED = xtrue])
AC_SUBST(LIBRDKCONFIG_FLAG)

AC_CONFIG_FILES([Makefile logupload/src/Makefile])
AC_OUTPUT
