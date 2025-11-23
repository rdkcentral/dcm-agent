bin_PROGRAMS = logupload

logupload_SOURCES = context_manager.c rbus_interface.c test_context.c    
#logupload_SOURCES = context.c  test_context.c privacy_mode.c mtls_cert_selector.c http_upload.c

logupload_CFLAGS = -Wall -DIS_LIBRDKCONFIG_ENABLED -DIS_LIBRDKCERTSEL_ENABLED -DLIBRDKCONFIG_BUILD -DLIBRDKCERTSELECTOR\
                     -I${top_srcdir}/logupload \
                     -I${top_srcdir}/logupload/include

AM_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)
AM_LDFLAGS += $(curl_LIBS)
AM_LDFLAGS += -lcurl -lrdkloggers -ldwnlutil -lRdkCertSelector -lrbus -lcjson -lsecure_wrapper -lfwutils

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
