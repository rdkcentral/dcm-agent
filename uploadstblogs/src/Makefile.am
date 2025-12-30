bin_PROGRAMS = logupload

logupload_SOURCES = uploadstblogs.c context_manager.c validation.c strategy_selector.c strategy_handler.c strategy_ondemand.c strategy_reboot.c strategy_dcm.c upload_engine.c path_handler.c retry_logic.c archive_manager.c file_operations.c event_manager.c cleanup_handler.c verification.c rbus_interface.c md5_utils.c

logupload_CFLAGS = -Wall -DIS_LIBRDKCONFIG_ENABLED -DIS_LIBRDKCERTSEL_ENABLED -DLIBRDKCONFIG_BUILD -DLIBRDKCERTSELECTOR -DEN_MAINTENANCE_MANAGER -DIARM_ENABLED -DT2_EVENT_ENABLED \
                     -I${top_srcdir} \
                     -I${top_srcdir}/uploadstblogs \
                     -I${top_srcdir}/uploadstblogs/include \
                     -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include \
                     -I$(PKG_CONFIG_SYSROOT_DIR)/usr/include/upload_util \
                     -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
                     -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/sysmgr \
                     -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal

logupload_LDFLAGS = -L$(PKG_CONFIG_SYSROOT_DIR)/$(libdir)
logupload_LDFLAGS += $(curl_LIBS)
logupload_LDFLAGS += -lcurl -lrdkloggers -ldwnlutil -lRdkCertSelector -lrbus -lcjson -lsecure_wrapper -lfwutils -lcrypto -lrfcapi -lz -lIARMBus -lt2utils -ltelemetry_msgsender -L$(PKG_CONFIG_SYSROOT_DIR)/usr/lib -luploadutil
