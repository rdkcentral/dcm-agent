#
## Copyright 2023 Comcast Cable Communications Management, LLC
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

# Initialize Autoconf
AC_INIT([dcm_main_gtest], [1.0])
AC_CONFIG_SRCDIR([dcm_gtest.cpp])
AC_CONFIG_MACRO_DIRS([m4])

# Initialize Automake
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Checks for programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

# Initialize Libtool
LT_INIT

# RDKE build option
AC_ARG_ENABLE([rdke],
    AS_HELP_STRING(
        [--enable-rdke],
        [build RDKE-compatible DCM unit tests without scheduler tests]),
    [
        case "${enableval}" in
            yes)
                IS_RDKE_ENABLED=true
                RDKE_FLAG="-DRDKE"
                ;;
            no)
                IS_RDKE_ENABLED=false
                RDKE_FLAG=""
                ;;
            *)
                AC_MSG_ERROR(
                    [bad value ${enableval} for --enable-rdke])
                ;;
        esac
    ],
    [
        IS_RDKE_ENABLED=false
        RDKE_FLAG=""
    ])

AM_CONDITIONAL(
    [IS_RDKE_ENABLED],
    [test "x$IS_RDKE_ENABLED" = "xtrue"])

AC_SUBST([RDKE_FLAG])

# Check for necessary headers
AC_CHECK_HEADERS([
    gtest/gtest.h
    gmock/gmock.h
])

# Checks for libraries
AC_CHECK_LIB([stdc++], [main])

# Checks for header files
AC_INCLUDES_DEFAULT

# Checks for typedefs, structures, and compiler characteristics
AC_C_CONST

# Checks for library functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC

# Generate the Makefile
AC_CONFIG_FILES([Makefile])

# Generate the configure script
AC_OUTPUT
