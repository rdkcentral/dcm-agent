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

#ifndef _DCM_RBUS_H_
#define _DCM_RBUS_H_
#ifdef __cplusplus
extern "C"
{
#endif

#define DCM_RBUS_RECE_NAME        "T2TODCM"
#define DCM_RBUS_SEND_NAME        "DCMTOT2"
#define DCM_RBUS_T2_STATUS        "DCM_T2_GETPARAM"
#define DCM_RBUS_SETCONF_EVENT    "Device.DCM.Setconfig"
#define DCM_RBUS_PROCCONF_EVENT   "Device.DCM.Processconfig"
#define DCM_RBUS_RELOAD_EVENT     "Device.X_RDKCENTREL-COM.Reloadconfig"
#define DCM_RBUS_T2_VERSION       "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version"
#define DCM_RBUS_T2_CONFURL       "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL"
#define DCM_SET_CONFIG            "dcmSetConfig"
#define DCM_RE_CONFIG             "dcmReConfig"
#define DCM_CONF_SIZE             128

typedef struct _dcmRBusHandle
{
    rbusHandle_t pRbusHandle;
    INT32        schedJob;
    INT32        eventSub;
    INT8         confPath[DCM_CONF_SIZE];
} DCMRBusHandle;

INT32  dcmRbusInit(VOID **ppDCMRbusHandle);
INT32  dcmRbusSubscribeEvents(VOID *pDCMRbusHandle);
VOID   dcmRbusUnInit(VOID *pDCMRbusHandle);
INT32  dcmRbusSendEvent(VOID *pDCMRbusHandle);
INT32  dcmRbusSchedJobStatus(VOID *pDCMRbusHandle);
VOID   dcmRbusSchedResetStatus(VOID *pDCMRbusHandle);
INT8   dcmRbusGetEventSubStatus(VOID *pDCMRbusHandle);
INT8*  dcmRbusGetConfPath(VOID *pDCMRbusHandle);
INT32  dcmRbusGetT2Version(VOID *pDCMRbusHandle, VOID *value);

#ifdef __cplusplus
}
#endif
#endif //_DCM_RBUS_H_

