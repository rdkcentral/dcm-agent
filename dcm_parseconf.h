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

#ifndef _DCM_PARSECONF_H_
#define _DCM_PARSECONF_H_

#define DCM_JSON_STRSIZE   2048
#define DCM_JSONITEM_BOOL  0
#define DCM_JSONITEM_INT   1
#define DCM_JSONITEM_STR   2
#define DCM_JSONITEM_NULL  3

#define DCM_T2_JSONSTR         "urn:settings:TelemetryProfile"
#define DCM_LOGUPLOAD_PROTOCOL "urn:settings:LogUploadSettings:UploadRepository:uploadProtocol"
#define DCM_LOGUPLOAD_URL      "urn:settings:LogUploadSettings:UploadRepository:URL"
#define DCM_LOGUPLOAD_REBOOT   "urn:settings:LogUploadSettings:UploadOnReboot"
#define DCM_LOGUPLOAD_CRON     "urn:settings:LogUploadSettings:UploadSchedule:cron"
#define DCM_DIFD_CRON          "urn:settings:CheckSchedule:cron"
#define DCM_TIMEZONE           "urn:settings:TimeZoneMode"

#define DCM_MAINT_CONF_PATH    "/opt/rdk_maintenance.conf"

#ifndef DCM_DEF_LOG_URL // please update with the Default URL
#define DCM_DEF_LOG_URL        "https://falbackurl" 
#endif

#define DCM_DEF_TIMEZONE       "Local Time"

typedef struct _dcmSettingsHandle
{
    INT8  cJsonStr[DCM_JSON_STRSIZE];
    INT8  cUploadURL[MAX_URL_SIZE];
    INT8  cUploadPrtl[8];
    INT8  cTimeZone[16];
    INT8  cRdkPath[MAX_DEVICE_PROP_BUFF_SIZE];
    INT8  ctBuff[EXECMD_BUFF_SIZE];
    INT32 bRebootFlag;
} DCMSettingsHandle;

INT32  dcmSettingsInit(VOID **ppdcmSetHandle);
VOID   dcmSettingsUnInit(VOID *pdcmSetHandle);
INT32  dcmSettingParseConf(VOID *pdcmSetHandle, INT8 *pConffile,
                           INT8 *pLogCron, INT8 *pDifdCron);
INT8*  dcmSettingsGetUploadProtocol(VOID *pdcmSetHandle);
INT8*  dcmSettingsGetUploadURL(VOID *pdcmSetHandle);
INT8*  dcmSettingsGetRDKPath(VOID *pdcmSetHandle);
INT32  dcmSettingsGetMMFlag();

#ifdef DCM_DEFAULT_BOOTCONFIG
INT32 dcmSettingDefaultBoot(INT8 *defaultConfig);
#endif //DCM_DEFAULT_BOOTCONFIG

#endif //_DCM_PARSECONF_H_
