##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

AUTOMAKE_OPTIONS = foreign

# Build uploadstblogs binary
bin_PROGRAMS = uploadstblogs

# Source files
uploadstblogs_SOURCES = \
	uploadstblogs.c \
	context_manager.c \
	validation.c \
	strategy_selector.c \
	strategy_handler.c \
	strategy_ondemand.c \
	strategy_reboot.c \
	strategy_dcm.c \
	upload_engine.c \
	path_handler.c \
	retry_logic.c \
	archive_manager.c \
	mtls_handler.c \
	mtls_cert_selector.c \
	oauth_handler.c \
	config_manager.c \
	log_collector.c \
	file_operations.c \
	event_manager.c \
	cert_manager.c \
	tls_handler.c \
	ocsp_validator.c \
	cleanup_handler.c \
	cleanup_manager.c \
	verification.c \
	telemetry.c \
	rbus_interface.c \
	md5_utils.c \
	$(NULL)

# Header files (for distribution)
noinst_HEADERS = \
	uploadstblogs.h \
	uploadstblogs_types.h \
	context_manager.h \
	validation.h \
	strategy_selector.h \
	strategy_handler.h \
	upload_engine.h \
	path_handler.h \
	retry_logic.h \
	archive_manager.h \
	mtls_handler.h \
	mtls_cert_selector.h \
	oauth_handler.h \
	config_manager.h \
	log_collector.h \
	file_operations.h \
	event_manager.h \
	cert_manager.h \
	tls_handler.h \
	ocsp_validator.h \
	cleanup_handler.h \
	cleanup_manager.h \
	verification.h \
	telemetry.h \
	rbus_interface.h \
	md5_utils.h \
	$(NULL)

# Compiler flags
uploadstblogs_CFLAGS = \
	-Wall \
	-Wextra \
	-Werror \
	-fPIC \
	-pthread \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/../common_utilities/utils \
	-I$(top_srcdir)/../common_utilities/parsejson \
	-I$(top_srcdir)/../common_utilities/dwnlutils \
	-I$(top_srcdir)/../common_utilities/uploadutil \
	-I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/dbus-1.0 \
	-I${PKG_CONFIG_SYSROOT_DIR}$(libdir)/dbus-1.0/include \
	-I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rbus \
	-I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmbus \
	-I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs/sysmgr \
	-I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rdk/iarmmgrs-hal \
	$(NULL)

# Linker flags
uploadstblogs_LDFLAGS = \
	-lcurl \
	-lssl \
	-lcrypto \
	-lpthread \
	-lz \
	-lrbus \
	-lfwutils \
	-lrdkloggers \
	-L$(top_builddir)/../common_utilities/utils/.libs \
	-L$(top_builddir)/../common_utilities/uploadutil/.libs \
	-luploadutil \
	$(NULL)

# Additional dependencies (if needed)
# uploadstblogs_LDADD = 

