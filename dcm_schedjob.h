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

#ifndef _DCM_SCHEDJOB_H_
#define _DCM_SCHEDJOB_H_
#ifdef __cplusplus
extern "C"
{
#endif

typedef VOID (*DCMSchedCB)(const INT8* profileName, VOID *pUsrData);

typedef struct _dcmScheduler
{
    INT8           *name;
    BOOL            terminated;
    BOOL            startSched;
    dcmCronExpr     parseData;
    pthread_t       tId;
    pthread_mutex_t tMutex;
    pthread_cond_t  tCond;
    DCMSchedCB      pDcmCB;
    VOID           *pUserData;

}DCMScheduler;

INT32 dcmSchedInit();
VOID  dcmSchedUnInit();
INT32 dcmSchedParseJobs();
VOID* dcmSchedAddJob(INT8 *pJobName, DCMSchedCB pDcmCB, VOID *pUsrData);
VOID  dcmSchedRemoveJob(VOID *pHandle);
INT32 dcmSchedStartJob(VOID *pHandle, INT8 *pCronPattern);
INT32 dcmSchedStopJob(VOID *pHandle);

#ifdef __cplusplus
}
#endif
#endif //_DCM_SCHEDJOB_H_
