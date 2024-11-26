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

#ifndef _DCM_H_
#define _DCM_H_

#define DCM_LOGUPLOAD_SCHED    "DCM_LOG_UPLOAD"
#define DCM_DIFD_SCHED         "DCM_FW_UPDATE"

typedef struct _dcmdHandle
{
    BOOL  isDebugEnabled;
    BOOL  isDCMRunning;
    VOID *pRbusHandle;
    VOID *pDcmSetHandle;
    VOID *pLogSchedHandle;
    VOID *pDifdSchedHandle;
    INT8 *pExecBuff;
    INT8  logCron[16];
    INT8  difdCron[16];

} DCMDHandle;

INT32  dcmDaemonMainInit(DCMDHandle *pdcmHandle);
VOID   dcmDaemonMainUnInit(DCMDHandle *pdcmHandle);
#endif //_DCM_H_
