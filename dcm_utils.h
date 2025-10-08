/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:

 * Copyright 2024 Comcast Cable Communications Management, LLC
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

#ifndef _DCM_UTILS_H_
#define _DCM_UTILS_H_

#ifdef HAS_MAINTENANCE_MANAGER
#include "libIBus.h"
#include "maintenanceMGR.h"
#endif

#include "rbus.h"

#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "rbus.h"
#endif

#define DCM_LIB_PATH                 "/lib/rdk"
#define DCM_PID_FILE                 "/tmp/.dcm-daemon.pid"
#define DEVICE_PROP_FILE             "/etc/device.properties"
#define TELEMETRY_2_FILE             "/etc/telemetry2_0.properties"
#define INCLUDE_PROP_FILE            "/etc/include.properties"
#define DCM_TMP_CONF                 "/tmp/DCMSettings.conf"
#define DCM_OPT_CONF                 "/opt/.DCMSettings.conf"
#define DCM_RESPONSE_PATH            "/.t2persistentfolder/DCMresponse.txt"
#define PERSISTENT_ENTRY             "PERSISTENT_PATH"
#define DEFAULT_PERSISTENT_PATH      "/opt"

#ifndef DCM_LOG_TFTP // please define your log upload url
#define DCM_LOG_TFTP                 "Fallbacklogupload"
#endif

/* DCM Error Codes */
#define DCM_SUCCESS         0
#define DCM_FAILURE        -1

#define DCM_IARM_COMPLETE   0
#define DCM_IARM_ERROR      1

#define MAX_DEVICE_PROP_BUFF_SIZE           80
#define EXECMD_BUFF_SIZE                    1024
#define MAX_URL_SIZE                        128

#define DEBUG_INI_NAME  "/etc/debug.ini"

#define PREFIX(format)  "[DCM] %s[%d]: " format

#ifdef RDK_LOGGER_ENABLED

extern int g_rdk_logger_enabled;

#define LOG_ERROR(format, ...)   if(g_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.DCM", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_WARN(format, ...)    if(g_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.DCM", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_INFO(format, ...)    if(g_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.DCM", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_DEBUG(format, ...)   if(g_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.DCM", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_TRACE(format, ...)   if(g_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.DCM", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#else

#define LOG_ERROR(format, ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_WARN(format,  ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_INFO(format,  ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_DEBUG(format, ...)            fprintf(stderr, format, __VA_ARGS__)
#define LOG_TRACE(format, ...)            fprintf(stderr, format, __VA_ARGS__)

#endif  //RDK_LOGGER_ENABLED

#define DCMError(format, ...)       LOG_ERROR(PREFIX(format), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DCMWarn(format,  ...)       LOG_WARN(PREFIX(format),  __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DCMInfo(format,  ...)       LOG_INFO(PREFIX(format),  __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DCMDebug(format, ...)       LOG_DEBUG(PREFIX(format), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DCMTrace(format, ...)       LOG_TRACE(PREFIX(format), __FUNCTION__, __LINE__, ##__VA_ARGS__)

INT32 dcmIARMEvntSend(INT32 status);
INT32 dcmUtilsSysCmdExec(INT8 *cmd);
VOID  dcmUtilsCopyCommandOutput (INT8 *cmd, INT8 *out, INT32 len);
INT32 dcmUtilsCheckDaemonStatus();
VOID  dcmUtilsRemovePIDfile();
INT32 dcmUtilsFilePresentCheck(const INT8 *file_name);
INT8* dcmUtilsGetFileEntry(const INT8* fileName, const INT8* searchEntry);

void DCMLOGInit();

#endif //_DCM_UTILS_H


