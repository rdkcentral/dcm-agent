#
## Copyright 2025 RDK Management
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
AC_INIT([uploadstblogs_gtest], [1.0])

# Initialize Automake
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Check for necessary headers
AC_CHECK_HEADERS([gtest/gtest.h gmock/gmock.h])

# Checks for programs
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries
AC_CHECK_LIB([stdc++], [main])
AC_CHECK_LIB([gtest], [main])
AC_CHECK_LIB([gmock], [main])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([curl], [curl_easy_init])
AC_CHECK_LIB([ssl], [SSL_library_init])
AC_CHECK_LIB([crypto], [MD5_Init])

# Check for RDK libraries
AC_CHECK_LIB([rdkloggers], [rdk_logger_init])
AC_CHECK_LIB([rbus], [rbus_open])
AC_CHECK_LIB([fwutils], [GetEstbMac])

# Checks for header files
AC_INCLUDES_DEFAULT
AC_CHECK_HEADERS([curl/curl.h])
AC_CHECK_HEADERS([openssl/md5.h openssl/evp.h])
AC_CHECK_HEADERS([rdk_debug.h])
AC_CHECK_HEADERS([rbus.h])

# Checks for typedefs, structures, and compiler characteristics
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strchr strdup strerror strstr])
AC_CHECK_FUNCS([access stat mkdir unlink])

# Enable coverage if requested
AC_ARG_ENABLE([coverage],
    [AS_HELP_STRING([--enable-coverage],
        [Enable code coverage reporting])],
    [coverage=${enableval}],
    [coverage=no])

if test "x$coverage" = "xyes"; then
    CXXFLAGS="$CXXFLAGS -fprofile-arcs -ftest-coverage"
    CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"
    LDFLAGS="$LDFLAGS -lgcov"
fi

# Generate the Makefile
AC_CONFIG_FILES([Makefile])

# Generate the configure script
AC_OUTPUT