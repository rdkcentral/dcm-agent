bin_PROGRAMS = logupload

logupload_SOURCES = upload.c rdkv_cdl_log_wrapper.c

logupload_CFLAGS = -Wall -DIS_LIBRDKCONFIG_ENABLED -DIS_LIBRDKCERTSEL_ENABLED -DRDK_LOGGER\
                     -I${top_srcdir}/logupload

AM_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)
AM_LDFLAGS += $(curl_LIBS)
AM_LDFLAGS += -lcurl -lrdkloggers -ldwnlutil

if IS_LIBRDKCERTSEL_ENABLED
logupload_CFLAGS += $(LIBRDKCERTSEL_FLAG)
logupload_CFLAGS += $(LIBRDKCERTSEL_FLAG)
if IS_LIBRDKCONFIG_ENABLED
logupload_CFLAGS += $(LIBRDKCONFIG_FLAG)
AM_LDFLAGS += -lRdkCertSelector -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib64 -lrdkconfig
else
AM_LDFLAGS += -lRdkCertSelector
endif
else
if IS_LIBRDKCONFIG_ENABLED
logupload_CFLAGS += $(LIBRDKCONFIG_FLAG)
AM_LDFLAGS += -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib64 -lrdkconfig
endif
endif
