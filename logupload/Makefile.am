bin_PROGRAMS = logupload

logupload_SOURCES = logupload.cpp

logupload_CPPFLAGS = -std=c++11 -Wall -DRDK_LOGGER -DIS_LIBRDKCONFIG_ENABLED -DIS_LIBRDKCERTSEL_ENABLED\
                     -I${top_srcdir}/logupload

AM_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)
AM_LDFLAGS += $(curl_LIBS)
AM_LDFLAGS += -lcurl -lrdkloggers

if IS_LIBRDKCERTSEL_ENABLED
logupload_CPPFLAGS += $(LIBRDKCERTSEL_FLAG)
logupload_CFLAGS = $(LIBRDKCERTSEL_FLAG)
if IS_LIBRDKCONFIG_ENABLED
logupload_CPPFLAGS += $(LIBRDKCONFIG_FLAG)
AM_LDFLAGS += -lRdkCertSelector -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib64 -lrdkconfig
else
AM_LDFLAGS += -lRdkCertSelector
endif
else
if IS_LIBRDKCONFIG_ENABLED
logupload_CPPFLAGS += $(LIBRDKCONFIG_FLAG)
AM_LDFLAGS += -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib64 -lrdkconfig
endif
endif
