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
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <vector>

#include "rbus.h"
#include "dcm_types.h"
#include "dcm_rbus.h"
#include "dcm_utils.h"
#include "dcm_parseconf.h"

static rbusError_t rbusSendEventCB(rbusHandle_t handle, rbusEventSubAction_t action,
                                   const INT8* eventName, rbusFilter_t filter,
                                   int32_t interval, BOOL* autoPublish);

sigset_t blocking_signal;

static INT32 g_eventsub = 0;

static rbusDataElement_t g_dataElements = {
        DCM_RBUS_RELOAD_EVENT, RBUS_ELEMENT_TYPE_EVENT,
        {NULL, NULL, NULL, NULL, rbusSendEventCB, NULL}
};

/** @brief set config Event function.
 *         T2 Sends the Config path
 *
 *  @param[in]  handle        rbus handle
 *  @param[in]  event         event handle
 *  @param[in]  subscription  subscription handle
 *
 *  @return  None.
 *  @retval  None.
 */
static VOID rbusSetConf(rbusHandle_t handle,
                        rbusEvent_t const* event,
                        rbusEventSubscription_t* subscription)
{
    rbusValue_t configPath;
    DCMRBusHandle *pDCMRbusHandle;

    if(event == NULL) {
        DCMError("Rbus Event handle is null\n");
        return;
    }

    if(subscription == NULL) {
        DCMError("Rbus Event subscription is null\n");
        return;
    }

    pDCMRbusHandle = (DCMRBusHandle *)subscription->userData;

    if(pDCMRbusHandle == NULL) {
        DCMError("Rbus Handle is null\n");
        return;
    }

    configPath = rbusObject_GetValue(event->data, DCM_SET_CONFIG);

    if(configPath) {
        const INT8 *filePath = rbusValue_GetString(configPath, NULL);
        strcpy(pDCMRbusHandle->confPath, filePath);
        DCMInfo("configPath: %s\n", filePath);
    }

    DCMInfo("Recieved eventName: %s, Event type: %d, Event Name: %s\n", subscription->eventName, event->type, event->name);

}

/** @brief Process config Event function.
 *
 *  @param[in]  handle        rbus handle
 *  @param[in]  event         event handle
 *  @param[in]  subscription  subscription handle
 *
 *  @return  None.
 *  @retval  None.
 */
static VOID rbusProcConf(rbusHandle_t handle,
                         rbusEvent_t const* event,
                         rbusEventSubscription_t* subscription)
{
    DCMRBusHandle *pDCMRbusHandle;

    if(event == NULL) {
        DCMError("Rbus Event handle is null\n");
        return;
    }

    if(subscription == NULL) {
        DCMError("Rbus Event subscription is null\n");
        return;
    }

    pDCMRbusHandle = (DCMRBusHandle *)subscription->userData;

    if(pDCMRbusHandle == NULL) {
        DCMError("Rbus Handle is null\n");
        return;
    }

    DCMInfo("Recieved eventName: %s, Event type: %d, Event Name: %s\n", subscription->eventName, event->type, event->name);

    pDCMRbusHandle->schedJob = 1;
}

/** @brief Process Event function for Device.X_RDKCENTREL-COM.Reloadconfig
 *         Subscription.
 *
 *  @param[in]  handle        rbus handle
 *  @param[in]  event         event handle
 *  @param[in]  subscription  subscription handle
 *
 *  @return  None.
 *  @retval  None.
 */
static rbusError_t rbusSendEventCB(rbusHandle_t handle, rbusEventSubAction_t action,
                                   const INT8* eventName, rbusFilter_t filter,
                                   int32_t interval, BOOL* autoPublish)
{
    (VOID)handle;
    (VOID)filter;
    (VOID)autoPublish;
    (VOID)interval;

    if(eventName == NULL) {
        DCMError("Rbus Handle is null\n");
        return RBUS_ERROR_BUS_ERROR;
    }

    if(!strcmp(DCM_RBUS_RELOAD_EVENT, eventName)) {
        DCMInfo("rbusSendEvent event registered %s\n", eventName);
        g_eventsub = action == RBUS_EVENT_ACTION_SUBSCRIBE ? 1 : 0;
    }
    else {
        DCMWarn("rbusSendEvent unexpected eventName %s\n", eventName);
    }

    return RBUS_ERROR_SUCCESS;
}

/** @brief Call back for the telemetry evens Subscription.
 *
 *  @param[in]  handle        rbus handle
 *  @param[in]  subscription  subscription handle
 *  @param[in]  error         subscription error
 *
 *  @return  None.
 *  @retval  None.
 */
static void rbusAsyncSubCB(rbusHandle_t handle,
                           rbusEventSubscription_t* subscription,
                           rbusError_t error)
{
    DCMRBusHandle *pDCMRbusHandle;

    (void)(handle);

    if(subscription == NULL) {
        DCMWarn("Subscription Handle is null \n");
        return;
    }

    pDCMRbusHandle = (DCMRBusHandle *)subscription->userData;
    if(pDCMRbusHandle == NULL) {
        DCMError("Rbus Handle is null\n");
        return;
    }

    if(error != RBUS_ERROR_SUCCESS) {
        DCMWarn("Subscription %s event failed erro:%d \n", subscription->eventName, error);
        pDCMRbusHandle->eventSub = 0;
    }
    else {
        DCMInfo("Subscription %s event Success\n", subscription->eventName);
        pDCMRbusHandle->eventSub = 1;
    }
}

/** @brief This Function initializes the receiver rbus.
 *
 *  @param[in/out]  pRecRbusHandle   receiver rbus handle
 *  @param[in/out]  pSendRbusHandle  Sender rbus handle
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmRbusSendEvent(VOID *pDCMRbusHandle)
{
    INT32 ret = DCM_SUCCESS;
    INT32 rc  = RBUS_ERROR_SUCCESS;
    rbusEvent_t  event = {0};
    rbusObject_t data;
    rbusValue_t  value;
    DCMRBusHandle *plDCMRbusHandle = pDCMRbusHandle;
    rbusHandle_t handle;

    DCMInfo("%s ++in\n", __FUNCTION__);

    if(plDCMRbusHandle == NULL) {
        DCMError("Handle is null\n");
        return DCM_FAILURE;
    }

    if(plDCMRbusHandle->pRbusHandle == NULL) {
        DCMError("RBus handle is null\n");
        return DCM_FAILURE;
    }

    if(g_eventsub == 0) {
        DCMError("Reload event is not susbcribed\n");
        return DCM_FAILURE;
    }

    handle = (rbusHandle_t)plDCMRbusHandle->pRbusHandle;

    rbusValue_Init(&value);
    rbusValue_SetString(value, "ReConfig");

    rbusObject_Init(&data, NULL);
    rbusObject_SetValue(data, DCM_RE_CONFIG, value);

    event.name = DCM_RBUS_RELOAD_EVENT;
    event.data = data;
    event.type = RBUS_EVENT_GENERAL;

    rc = rbusEvent_Publish(handle, &event);

    rbusValue_Release(value);
    rbusObject_Release(data);

    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("rbusEvent_Publish DCM_RBUS_RELOAD_EVENT failed: %d\n", rc);
        ret = DCM_FAILURE;
    }

    DCMInfo("%s --out\n", __FUNCTION__);
    return ret;
}

/** @brief This Function get telemetry details.
 *
 *  @param[in/out]  None
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmRbusGetT2Version(VOID *pDCMRbusHandle, VOID *pValue)
{
    INT32           ret = DCM_SUCCESS;
    INT32           rc  = RBUS_ERROR_SUCCESS;
    INT8           *t2_ver = pValue;
    DCMRBusHandle  *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;
    rbusHandle_t    handle;
    rbusValue_t     paramValue = NULL;
    rbusValueType_t rbusValueType;
    INT8           *stringValue = NULL;

    if(plDCMRbusHandle == NULL) {
        DCMError("Handle is null\n");
        return DCM_FAILURE;
    }

    if(t2_ver == NULL) {
        DCMError("Input param is null\n");
        return DCM_FAILURE;
    }

    if(plDCMRbusHandle->pRbusHandle == NULL) {
        DCMError("RBus handle is null\n");
        return DCM_FAILURE;
    }

    handle  = plDCMRbusHandle->pRbusHandle;
    *t2_ver = 0;

    rc = rbus_get(handle, DCM_RBUS_T2_VERSION, &paramValue);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("Unable to get: %s\n", DCM_RBUS_T2_VERSION);
        return DCM_FAILURE;
    }

    rbusValueType = rbusValue_GetType(paramValue);
    if(rbusValueType == RBUS_STRING) {
        stringValue   = rbusValue_ToString(paramValue, NULL, 0);
        if(stringValue == NULL) {
            DCMError("Unable to get Version\n");
            ret = DCM_FAILURE;
            goto exit;
        }
        else {
            strcpy(t2_ver, stringValue);
            DCMInfo("Telemetry 2 Version: %s\n", stringValue);
        }
    }

exit:
    if(paramValue) {
        rbusValue_Release(paramValue);
        paramValue = NULL;
    }
    return ret;
}

/** @brief This Function returns the Event Subscription status.
 *
 *  @param[in]  pDCMRbusHandle - rbus handle
 *
 *  @return  Returns the path of configuration file.
 */
INT8  dcmRbusGetEventSubStatus(VOID *pDCMRbusHandle)
{
    DCMRBusHandle *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;
    if(plDCMRbusHandle == NULL) {
        DCMError("Handle is null\n");
        return 0;
    }

    return plDCMRbusHandle->eventSub;
}

/** @brief This Function returns the Conf file path.
 *
 *  @param[in]  pDCMRbusHandle - rbus handle
 *
 *  @return  Returns the path of configuration file.
 */
INT8*  dcmRbusGetConfPath(VOID *pDCMRbusHandle)
{
    DCMRBusHandle *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;
    if(plDCMRbusHandle == NULL) {
        DCMError("Handle is null\n");
        return NULL;
    }

    return plDCMRbusHandle->confPath;
}

/** @brief This Function returns the Schedule status.
 *
 *  @param[in]  pDCMRbusHandle - rbus handle
 *
 *  @return  Returns the status of the operation.
 */
INT32 dcmRbusSchedJobStatus(VOID *pDCMRbusHandle)
{
    DCMRBusHandle *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;
    if(plDCMRbusHandle == NULL) {
        DCMError("Handle is null\n");
        return 0;
    }

    return plDCMRbusHandle->schedJob;
}

/** @brief This Function returns the Schedule status.
 *
 *  @param[in]  pDCMRbusHandle - rbus handle
 *
 *  @return  Returns the status of the operation.
 */
VOID dcmRbusSchedResetStatus(VOID *pDCMRbusHandle)
{
    DCMRBusHandle *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;

    if(plDCMRbusHandle == NULL) {
        DCMError("Handle is null\n");
        return;
    }
    plDCMRbusHandle->schedJob = 0;
}

/** @brief This Function subscribes to rbus events.
 *
 *  @param[in/out]  pDCMRbusHandle   rbus handle
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32  dcmRbusSubscribeEvents(VOID *pDCMRbusHandle)
{
    INT32 rc  = RBUS_ERROR_SUCCESS;
    INT32 ret = DCM_SUCCESS;
    DCMRBusHandle *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;
    rbusHandle_t handle;

    if(plDCMRbusHandle == NULL) {
        DCMError("rbus handle is NULL\n");
        return DCM_FAILURE;
    }

    handle = plDCMRbusHandle->pRbusHandle;

    /* Subscribe for set config event */
    rc = rbusEvent_SubscribeAsync(handle,
                                  DCM_RBUS_SETCONF_EVENT,
                                  rbusSetConf,
                                  rbusAsyncSubCB,
                                  pDCMRbusHandle,
                                  0);

    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("rbusEvent_Subscribe set configuration failed: %d\n", rc);
        return DCM_FAILURE;
    }

    /* Subscribe for process config event */
    rc = rbusEvent_SubscribeAsync(handle,
                                  DCM_RBUS_PROCCONF_EVENT,
                                  rbusProcConf,
                                  rbusAsyncSubCB,
                                  pDCMRbusHandle,
                                  0);

    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("rbusEvent_Subscribe process configuration failed: %d\n", rc);
        ret = DCM_FAILURE;
        goto exit2;
    }

    /* Register for reload config file event */
    rc = rbus_regDataElements(handle, 1, &g_dataElements);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("rbus_regDataElements failed: %d\n", rc);
        ret = DCM_FAILURE;
        goto exit1;
    }

    return ret;

exit1:
    rbusEvent_Unsubscribe(handle, DCM_RBUS_PROCCONF_EVENT);
exit2:
    rbusEvent_Unsubscribe(handle, DCM_RBUS_SETCONF_EVENT);

    return ret;
}
/** @brief This Function initializes rbus.
 *
 *  @param[in/out]  ppDCMRbusHandle  rbus handle
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmRbusInit(VOID **ppDCMRbusHandle)
{
    INT32 rc  = RBUS_ERROR_SUCCESS;
    INT32 ret = DCM_SUCCESS;
    DCMRBusHandle *pDCMRbusHandle = NULL;
    rbusHandle_t handle;

    rc = rbus_checkStatus();
    if(RBUS_ENABLED != rc) {
        DCMError("rbus is not active: %d\n", rc);
        return DCM_FAILURE;
    }

    pDCMRbusHandle = malloc(sizeof(DCMRBusHandle));
    if(pDCMRbusHandle == NULL) {
        DCMError("Failed to allocate memory for handle\n");
        return DCM_FAILURE;
    }

    memset(pDCMRbusHandle, 0, sizeof(DCMRBusHandle));

    rc = rbus_open(&handle, DCM_RBUS_RECE_NAME);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("rbus_open failed: %d\n", rc);
        ret = DCM_FAILURE;
        goto exit;
    }

    pDCMRbusHandle->pRbusHandle = handle;

    *ppDCMRbusHandle = pDCMRbusHandle;

    return ret;

exit:
    if(pDCMRbusHandle) {
        free(pDCMRbusHandle);
        pDCMRbusHandle = NULL;
    }
    return ret;
}

/** @brief This Function un-initializes the rbus.
 *
 *  @param[in]  rbusHandle  rbus handle
 *
 *  @return  None.
 *  @retval  None.
 */
VOID dcmRbusUnInit (VOID *pDCMRbusHandle)
{
    INT32 rc  = RBUS_ERROR_SUCCESS;
    DCMRBusHandle *plDCMRbusHandle = (DCMRBusHandle *)pDCMRbusHandle;

    if(plDCMRbusHandle == NULL) {
        DCMError("rbus handle is NULL\n");
        return;
    }

    rc = rbusEvent_Unsubscribe(plDCMRbusHandle->pRbusHandle, DCM_RBUS_SETCONF_EVENT);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("Unable to Unsubscribe[%s]: %d\n",DCM_RBUS_SETCONF_EVENT, rc);
    }

    rc = rbusEvent_Unsubscribe(plDCMRbusHandle->pRbusHandle, DCM_RBUS_PROCCONF_EVENT);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("Unable to Unsubscribe[%s]: %d\n",DCM_RBUS_PROCCONF_EVENT, rc);
    }

    rc = rbus_unregDataElements(plDCMRbusHandle->pRbusHandle, 1, &g_dataElements);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("Unable to Unregister: %d\n",rc);
    }

    rc = rbus_close(plDCMRbusHandle->pRbusHandle);
    if(rc != RBUS_ERROR_SUCCESS) {
        DCMError("Unable to Close receiver bus: %d\n", rc);
    }

    plDCMRbusHandle->pRbusHandle = NULL;

    free(plDCMRbusHandle);

}
