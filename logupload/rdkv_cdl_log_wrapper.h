/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef  _RDKV_CDL_LOG_WRPPER_H_
#define  _RDKV_CDL_LOG_WRPPER_H_

#include <time.h>


#if defined(RDK_LOGGER)
#include "rdk_debug.h"

#define SWLOG_TRACE(format, ...)       RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.FWUPG", format, ##__VA_ARGS__)
#define SWLOG_DEBUG(format, ...)       RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.FWUPG", format, ##__VA_ARGS__)
#define SWLOG_INFO(format, ...)        RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.FWUPG", format, ##__VA_ARGS__)
#define SWLOG_WARN(format, ...)        RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.FWUPG", format, ##__VA_ARGS__)
#define SWLOG_ERROR(format, ...)       RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.FWUPG", format, ##__VA_ARGS__)
#define SWLOG_FATAL(format, ...)       RDK_LOG(RDK_LOG_FATAL,  "LOG.RDK.FWUPG", format, ##__VA_ARGS__)

#define COMMONUTILITIES_TRACE(format, ...)       RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.COMMONUTILITIES", format, ##__VA_ARGS__)
#define COMMONUTILITIES_DEBUG(format, ...)       RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.COMMONUTILITIES", format, ##__VA_ARGS__)
#define COMMONUTILITIES_INFO(format, ...)        RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.COMMONUTILITIES", format, ##__VA_ARGS__)
#define COMMONUTILITIES_WARN(format, ...)        RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.COMMONUTILITIES", format, ##__VA_ARGS__)
#define COMMONUTILITIES_ERROR(format, ...)       RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.COMMONUTILITIES", format, ##__VA_ARGS__)
#define COMMONUTILITIES_FATAL(format, ...)       RDK_LOG(RDK_LOG_FATAL,  "LOG.RDK.COMMONUTILITIES", format, ##__VA_ARGS__)

#else
#define SW_LOG_INFO      (1)
#define COMMON_UTILITIES_INFO     (1)
void swLog(unsigned int level, const char *msg, ...);

#define SWLOG_TRACE(FORMAT...) swLog(SW_LOG_INFO, __FILE__, __LINE__, FORMAT)
#define SWLOG_DEBUG(FORMAT...) swLog(SW_LOG_INFO, __FILE__, __LINE__, FORMAT)
#define SWLOG_INFO(FORMAT...) swLog(SW_LOG_INFO, __FILE__, __LINE__, FORMAT)
#define SWLOG_WARN(FORMAT...) swLog(SW_LOG_INFO, __FILE__, __LINE__, FORMAT)
#define SWLOG_ERROR(FORMAT...) swLog(SW_LOG_INFO, __FILE__, __LINE__, FORMAT)
#define SWLOG_FATAL(FORMAT...) swLog(SW_LOG_INFO, __FILE__, __LINE__, FORMAT)

#define COMMONUTILITIES_TRACE(FORMAT...)  swLog(COMMON_UTILITIES_INFO, __FILE__, __LINE__, FORMAT)
#define COMMONUTILITIES_DEBUG(FORMAT...)  swLog(COMMON_UTILITIES_INFO, __FILE__, __LINE__, FORMAT)
#define COMMONUTILITIES_INFO(FORMAT...)   swLog(COMMON_UTILITIES_INFO, __FILE__, __LINE__, FORMAT)
#define COMMONUTILITIES_WARN(FORMAT...)   swLog(COMMON_UTILITIES_INFO, __FILE__, __LINE__, FORMAT)
#define COMMONUTILITIES_ERROR(FORMAT...)  swLog(COMMON_UTILITIES_INFO, __FILE__, __LINE__, FORMAT)
#define COMMONUTILITIES_FATAL(FORMAT...)  swLog(COMMON_UTILITIES_INFO, __FILE__, __LINE__, FORMAT)

#endif

#define TLS_LOG_FILE "/opt/logs/tlsError.log"
#define DEBUG_INI_NAME  "/etc/debug.ini"

#define TLS_LOG_ERR      (1)
#define TLS_LOG_WARN     (2)
#define TLS_LOG_INFO     (3)
#define tls_debug_level (3)

#define TLSLOG(level, ...) do {  \
			    FILE *fp_tls = fopen(TLS_LOG_FILE, "a"); \
                            if ((fp_tls != NULL) && (level <= tls_debug_level)) { \
                                    if (level == TLS_LOG_ERR) { \
                                        fprintf(fp_tls,"ERROR: %s:%d:", __FILE__, __LINE__); \
                                    } else if (level == TLS_LOG_INFO) { \
                                        fprintf(fp_tls,"INFO: %s:%d:", __FILE__, __LINE__); \
                                    } else { \
                                        fprintf(fp_tls,"DBG: %s:%d:", __FILE__, __LINE__); \
                                    }\
                                fprintf(fp_tls, __VA_ARGS__); \
                                fprintf(fp_tls, "\n"); \
                                fflush(fp_tls); \
                                fclose(fp_tls); \
                            } \
                        } while (0)


int log_init();

void log_exit();

#endif
