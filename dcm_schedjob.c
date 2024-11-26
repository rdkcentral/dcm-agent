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

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>

#include "dcm_types.h"
#include "dcm_utils.h"
#include "dcm_parseconf.h"
#include "dcm_utils.h"
#include "dcm_cronparse.h"
#include "dcm_schedjob.h"

/** @brief Scheduler thread
 *
 *  @param[in]  arg  Scheduler handle
 *
 *  @return  Returns NULL.
 *  @retval  Returns NULL.
 */
void* dcmSchedulerThread(void *arg)
{
    DCMScheduler *pDCMSched = (DCMScheduler *)arg;
    INT32 n = 0;
    struct timespec _now;
    time_t timeOffset, currentTime;

    while(!pDCMSched->terminated) {
        pthread_mutex_lock(&pDCMSched->tMutex);

        if(!pDCMSched->startSched) {
            n = pthread_cond_wait(&pDCMSched->tCond, &pDCMSched->tMutex);
        }
        else {
            memset(&_now, 0, sizeof(struct timespec));

            clock_gettime(CLOCK_REALTIME, &_now);
            currentTime = _now.tv_sec;

            timeOffset = dcmCronParseGetNext(&pDCMSched->parseData, currentTime);
            _now.tv_sec += (timeOffset - currentTime);

            n = pthread_cond_timedwait(&pDCMSched->tCond, &pDCMSched->tMutex, &_now);

            if(n == ETIMEDOUT) {
                DCMInfo("Scheduling %s Job handle: %p\n", pDCMSched->name, pDCMSched->pUserData);
                if(pDCMSched->pDcmCB) {
                    pDCMSched->pDcmCB(pDCMSched->name, pDCMSched->pUserData);
                }
                else {
                    DCMWarn("%s Scheduler call back not registered\n", pDCMSched->name);
                }
            }
            else if (n == 0) {
                DCMInfo("%s Interrupted before TIMEOUT for profile\n", pDCMSched->name);
            }
            else {
                DCMWarn("%s pthread_cond_timedwait ERROR!!!\n", pDCMSched->name);
            }
        }
        pthread_mutex_unlock(&pDCMSched->tMutex);
    }
    return NULL;
}

/** @brief This function wakes up the sleeping thread
 *
 *  @param[in]  pHandle       Scheduler handle
 *  @param[in]  pCronPattern  Cron pattern
 *
 *  @return  Returns Status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmSchedStartJob(VOID *pHandle, INT8 *pCronPattern)
{
    DCMScheduler *pSchedHandle = pHandle;
    INT32  ret                 = DCM_SUCCESS;

    if(pHandle == NULL) {
        DCMError("Input Handle is NULL\n");
        return DCM_FAILURE;
    }

    if(pCronPattern == NULL) {
        DCMError("Input Cron pattern is NULL\n");
        return DCM_FAILURE;
    }

    /* Start the scheduler */
    pthread_mutex_lock(&pSchedHandle->tMutex);
    ret = dcmCronParseExp(pCronPattern, &pSchedHandle->parseData);
    if(ret == DCM_SUCCESS) {
        pSchedHandle->startSched = 1;
        pthread_cond_signal(&pSchedHandle->tCond);
    }
    else {
        pSchedHandle->startSched = 0;
        ret = DCM_FAILURE;
        DCMWarn ("Failed to parse log upload cron: %s \n", pCronPattern);
    }
    pthread_mutex_unlock(&pSchedHandle->tMutex);

    return ret;
}

/** @brief This function stops the scheduler
 *
 *  @param[in]  pHandle       Scheduler handle
 *
 *  @return  Returns Status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmSchedStopJob(VOID *pHandle)
{
    DCMScheduler *pSchedHandle = pHandle;
    INT32  ret                 = DCM_SUCCESS;

    if(pHandle == NULL) {
        DCMError("Input Handle is NULL\n");
        return DCM_FAILURE;
    }

    /* Stop the scheduler */
    pthread_mutex_lock(&pSchedHandle->tMutex);
    pSchedHandle->startSched = 0;
    pthread_cond_signal(&pSchedHandle->tCond);
    pthread_mutex_unlock(&pSchedHandle->tMutex);

    return ret;
}

/** @brief This Function adds the job to the Scheduler.
 *
 *  @param[in]  pJobName Scheduler name
 *  @param[in]  pDcmCB   Scheduler Callback function
 *  @param[in]  pUsrData arguments to callback function
 *
 *  @return  Returns the handle of the scheduler.
 *  @retval  Returns handle on success, NELL otherwise.
 */
VOID* dcmSchedAddJob(INT8 *pJobName, DCMSchedCB pDcmCB, VOID *pUsrData)
{
    DCMScheduler *pSchedHandle = NULL;
    INT32 ret = 0;

    if(pJobName == NULL) {
        DCMError("Name of the Job is NULL\n");
        return pSchedHandle;
    }

    /* Allocate memory for Scheduler handle */
    pSchedHandle = malloc(sizeof(DCMScheduler));
    if(pSchedHandle == NULL) {
        DCMError("Failed to allocate memeory\n");
        return pSchedHandle;
    }
    memset(pSchedHandle, 0, sizeof(DCMScheduler));

    /* Initialzing Scheduler */
    pSchedHandle->name       = pJobName;
    pSchedHandle->pDcmCB     = pDcmCB;
    pSchedHandle->pUserData  = pUsrData;
    pSchedHandle->terminated = false;
    pSchedHandle->startSched = false;

    if(pthread_mutex_init(&pSchedHandle->tMutex, NULL) != 0) {
        DCMError("Mutex init has failed\n");
        goto exit;
    }
    ret = pthread_cond_init(&pSchedHandle->tCond, NULL);
    if(ret) {
        DCMError("Conditional variable init has failed\n");
        goto exit1;
    }

    ret = pthread_create(&pSchedHandle->tId, NULL, dcmSchedulerThread, (void*)pSchedHandle);
    if(ret) {
        DCMError("Failed to create thread\n");
        goto exit2;
    }

    return pSchedHandle;

exit2:
    pthread_cond_destroy(&pSchedHandle->tCond);
exit1:
    pthread_mutex_destroy(&pSchedHandle->tMutex);
exit:
    if(pSchedHandle) {
        free(pSchedHandle);
        pSchedHandle = NULL;
    }

    return NULL;
}

/** @brief This Function removes the job from the Scheduler.
 *
 *  @param[in]  pHandle Scheduler handle
 *
 *  @return  Returns the handle of the scheduler.
 *  @retval  Returns handle on success, NELL otherwise.
 */
VOID dcmSchedRemoveJob(VOID *pHandle)
{
    DCMScheduler *pSchedHandle = pHandle;

    if(pHandle == NULL) {
        DCMError("Input Handle is NULL\n");
        return;
    }

    pthread_mutex_lock(&pSchedHandle->tMutex);
    pSchedHandle->startSched = false;
    pSchedHandle->terminated = true;
    pthread_cond_signal(&pSchedHandle->tCond);
    pthread_mutex_unlock(&pSchedHandle->tMutex);
    pthread_join(pSchedHandle->tId, NULL);

    pthread_mutex_destroy(&pSchedHandle->tMutex);
    pthread_cond_destroy(&pSchedHandle->tCond);
    pthread_detach(pSchedHandle->tId);

    free(pSchedHandle);
    pSchedHandle = NULL;
}

/** @brief This Function Initializes the Scheduler.
 *
 *  @param[]  None
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmSchedInit()
{
    //For Future use
    return DCM_SUCCESS;
}

/** @brief This Function de-Initializes the Scheduler.
 *
 *  @param[]  None
 *
 *  @return  Returns None.
 *  @retval  Returns None.
 */
VOID dcmSchedUnInit()
{
    //For Future use
}
