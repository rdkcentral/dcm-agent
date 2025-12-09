# Library
lib_LTLIBRARIES = libuploadstblogs.la

libuploadstblogs_la_SOURCES = context_manager.c validation.c strategy_selector.c strategy_handler.c \
                               strategy_ondemand.c strategy_reboot.c strategy_dcm.c upload_engine.c \
                               path_handler.c retry_logic.c archive_manager.c log_collector.c \
                               file_operations.c event_manager.c cleanup_handler.c cleanup_manager.c \
                               verification.c rbus_interface.c md5_utils.c telemetry.c uploadstblogs.c

libuploadstblogs_la_CFLAGS = -Wall -DEN_MAINTENANCE_MANAGER -DIARM_ENABLED -DT2_EVENT_ENABLED -DIS_LIBRDKCONFIG_ENABLED -DIS_LIBRDKCERTSEL_ENABLED -DLIBRDKCONFIG_BUILD -DLIBRDKCERTSELECTOR \
                              -I${top_srcdir} \
                              -I${top_srcdir}/uploadstblogs \
                              -I${top_srcdir}/uploadstblogs/include \
                              -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include \
                              -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/upload_util \
                              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
                              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/sysmgr \
                              -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal

libuploadstblogs_la_LDFLAGS = -version-info 0:0:0 -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)
libuploadstblogs_la_LIBADD = $(curl_LIBS) -lcurl -lrdkloggers -ldwnlutil -lRdkCertSelector -lrbus \
                              -lcjson -lsecure_wrapper -lfwutils -lcrypto -lrfcapi -lz -lIARMBus \
                              -lt2utils -ltelemetry_msgsender -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -luploadutil

# Binary
bin_PROGRAMS = logupload

logupload_SOURCES = uploadstblogs.c

logupload_CFLAGS = -Wall -DEN_MAINTENANCE_MANAGER -DIARM_ENABLED -DT2_EVENT_ENABLED \
                    -DUPLOADSTBLOGS_BUILD_BINARY \
                    -I${top_srcdir} \
                    -I${top_srcdir}/uploadstblogs \
                    -I${top_srcdir}/uploadstblogs/include \
                    -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include \
                    -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/upload_util \
                    -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
                    -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/sysmgr \
                    -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal


logupload_LDADD = libuploadstblogs.la
