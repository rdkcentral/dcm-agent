AC_INIT([usbLogUpload-unittest], [1.0], [support@example.com])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([cjson], [cJSON_Parse])
AC_CHECK_LIB([curl], [curl_easy_init])
AC_CHECK_LIB([ssl], [SSL_library_init])
AC_CHECK_LIB([crypto], [CRYPTO_new_ex_data])
AC_CHECK_LIB([gtest], [main])
AC_CHECK_LIB([gmock], [main])

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h sys/types.h sys/stat.h])

# Output files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
