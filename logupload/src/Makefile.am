bin_PROGRAMS = logupload

logupload_SOURCES = context.c  test_context.c

logupload_CFLAGS = -Wall \
                     -I${top_srcdir}/logupload \
                     -I${top_srcdir}/logupload/include

AM_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)
AM_LDFLAGS += $(curl_LIBS)
AM_LDFLAGS += -lcurl -lrdkloggers -ldwnlutil -lrbus
