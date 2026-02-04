#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([usblogupload], [1.0], [support@rdkcentral.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_SRCDIR([src/usb_log_main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h sys/stat.h sys/types.h dirent.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([mkdir rmdir access strdup strerror])

# Enable debug mode
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debug mode]))

AS_IF([test "x$enable_debug" = "xyes"], [
    AC_DEFINE([DEBUG], [1], [Define to 1 if debug mode is enabled])
    CFLAGS="$CFLAGS -g -O0 -DDEBUG"
], [
    CFLAGS="$CFLAGS -O2 -DNDEBUG"
])

# Add warning flags
CFLAGS="$CFLAGS -Wall -Wextra -Werror -std=c99"

AC_CONFIG_FILES([Makefile.am:Makefile.am.in])
AC_OUTPUT