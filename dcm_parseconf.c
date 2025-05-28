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
#include <cjson/cJSON.h>

#include "dcm_types.h"
#include "dcm_utils.h"
#include "dcm_rbus.h"
#include "dcm_parseconf.h"

static INT32 g_bMMEnable = 0;

/** @brief This Function Parse the file and return key value.
 *
 *  @param[in]  file_path  Path to file
 *  @param[in]  key        Key to search
 *  @param[out] value      Key value
 *  @param[out] delim      delimiter
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
static INT32 dcmSettingGetValueFromFile(INT8 *buf, INT8 *file_path,
                                        INT8 *key, INT8 *value, INT8* delim)
{
    FILE *fp        = fopen(file_path, "r");
    INT8 *tempStr   = NULL;
    INT32 ret       = DCM_SUCCESS;
    INT32 flag      = 0;

    if(NULL != fp) {
        while ( fgets( buf, EXECMD_BUFF_SIZE, fp ) != NULL ) {

            if ( strstr( buf, key ) != NULL ) {
                /* Strip off any carriage returns */
                buf[strcspn( buf, "\r" )] = 0;
                buf[strcspn( buf, "\n" )] = 0;
                buf[strcspn( buf, "," )]  = 0;
                tempStr = strstr( buf, delim );
                tempStr++;

                if(tempStr[0] == '\"')
                    tempStr++;

                if(tempStr[strlen(tempStr)-1] == '\"')
                    tempStr[strlen(tempStr)-1] = 0;

                strcpy(value,tempStr);

                flag = 1;
                ret = DCM_SUCCESS;
                break;
            }
        }
        if(flag == 0) {
            *value = 0;
            DCMError("%s is not present in device.properties file\n",key);
            ret = DCM_FAILURE;
        }
    }
    else {
        DCMError("Failed to open file:%s\n", file_path);
        return DCM_FAILURE;
    }

    if(fp) {
        fclose(fp);
    }

    DCMInfo("Key: %s Value: %s\n", key, value);
    return ret;
}

/** @brief This Function Initializes the Json handle.
 *
 *  @param[in]  infile       input json file path
 *  @param[out] jsonHandle   json handle
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
static INT32 dcmSettingJsonInit(DCMSettingsHandle *pdcmSetHandle,
                                INT8 *infile, VOID **jsonHandle)
{
    FILE *fpin      = NULL;
    INT32 ret       = DCM_SUCCESS;
    INT8 *tmp       = NULL;
    INT32 lc        = 0;
    cJSON *pJson    = NULL;
    INT8 *pcJsonStr = pdcmSetHandle->cJsonStr;

    *jsonHandle  = NULL;

    if(infile == NULL) {
        DCMError("Input file is null\n");
        return DCM_FAILURE;
    }

    fpin = fopen(infile, "r");
    if(fpin == NULL) {
        DCMError("Failed to open input file:%s\n", infile);
        return DCM_FAILURE;
    }

    /* Read the Json string to a buffer */
    if(!fgets(pcJsonStr, DCM_JSON_STRSIZE, fpin)) {
        memset(pcJsonStr, 0, DCM_JSON_STRSIZE);
        fclose(fpin);
        return DCM_FAILURE;
    }

    /* Remvoing the telemetry entries */
    tmp = strstr(pcJsonStr, DCM_T2_JSONSTR);

    if(tmp) {
        lc = tmp - pcJsonStr;
        pcJsonStr[lc - 2] = '}';
        pcJsonStr[lc - 1] = 0;
    }

    pJson = cJSON_Parse(pcJsonStr);
    if(pJson == NULL) {
        DCMError("Json Handle is null\n");
        ret = DCM_FAILURE;
    }

    *jsonHandle = pJson;
    fclose(fpin);

    return ret;
}

/** @brief This Function deletes Json handle.
 *
 *  @param[in/Out]  jsonHandle   json handle
 *
 *  @return  None.
 *  @retval  None.
 */
static VOID dcmSettingJsonUnInit(VOID **jsonHandle)
{
    cJSON *pJson = (cJSON *)(*jsonHandle);

    if(pJson) {
        cJSON_Delete(pJson);
        *jsonHandle = NULL;
    }
}

/** @brief This Function gets the value from the Json handle.
 *
 *  @param[in]  jsonHandle   json handle
 *  @param[in]  item         item to search
 *  @param[out] sval         String value
 *  @param[out] ival         interger value
 *  @param[out] type         Type of item
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
static INT32 dcmSettingJsonGetVal(VOID *jsonHandle, INT8 *item, INT8 *sval,
                                  INT32 *ival, INT32 *type)
{
    INT32 ret        = DCM_SUCCESS;
    cJSON *pJson     = (cJSON *)jsonHandle;
    cJSON *pJsonItem = NULL;

    if(pJson == NULL) {
        DCMError("Json Handle is null\n");
        return DCM_FAILURE;
    }

    *type  = DCM_JSONITEM_NULL;
    *ival  = 0;
    *sval  = 0;

    /* Read the object from the json handle and return key value */
    if((pJsonItem = cJSON_GetObjectItem(pJson, item))) {
        if(cJSON_IsBool(pJsonItem)) {
            *type  = DCM_JSONITEM_BOOL;
            *ival  = cJSON_IsTrue(pJsonItem);
        }
        else if(cJSON_IsNumber(pJsonItem)) {
            *type  = DCM_JSONITEM_INT;
            *ival  = pJsonItem->valueint;
        }
        else if(cJSON_IsString(pJsonItem)) {
            *type = DCM_JSONITEM_STR;
            strcpy(sval, pJsonItem->valuestring);
        }
        else if(cJSON_IsNull(pJsonItem)) {
            *type  = DCM_JSONITEM_NULL;
            *ival  = pJsonItem->valueint;
        }
    }
    else {
        ret = DCM_FAILURE;
    }

    return ret;
}

/** @brief This Function parses the settings file and store in tmp and opt folder.
 *
 *  @param[in]   pConffile  settings file path
 *  @param[out]  pTempConf  tmp folder conf path
 *  @param[out]  pOptConf   opt folder conf path
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
static INT32 dcmSettingStoreTempConf(INT8 *pConffile, INT8 *pTempConf, INT8 *pOptConf)
{
    cJSON *pJson   = NULL;
    INT32 file_len = 0;
    INT8 *buff     = NULL;
    INT32 i        = 0;
    INT32 ret      = DCM_SUCCESS;

    FILE *fp_in = fopen(pConffile, "r");
    if (fp_in == NULL) {
        DCMError("Failed to Open input conf file: %s\n", pConffile);
        return DCM_FAILURE;
    }

    FILE *fp_out = fopen(pTempConf, "w");
    if (fp_out == NULL) {
        ret = DCM_FAILURE;
        DCMError("Unable to open tmp file: %s\n", pTempConf);
        goto exit1;
    }

    FILE *fp_out_opt = fopen(pOptConf, "w");
    if (fp_out == NULL) {
        ret = DCM_FAILURE;
        DCMError("Unable to open out file: %s\n", pOptConf);
        goto exit2;
    }

    fseek(fp_in, 0, SEEK_END);
    file_len = ftell(fp_in);
    fseek(fp_in, 0, SEEK_SET);

    buff = calloc(file_len+1, 1);
    if(buff == NULL) {
        ret = DCM_FAILURE;
        DCMError("Unable to allocate memory\n");
        goto exit3;
    }

    fread(buff, 1, file_len, fp_in);

    pJson = cJSON_Parse(buff);
    if (pJson == NULL) {
        ret = DCM_FAILURE;
        printf("Unable to parse the json\n");
        goto exit4;
    }

    for (i = 0; i < cJSON_GetArraySize(pJson); i++) {
        cJSON *item = NULL;
        item = cJSON_GetArrayItem(pJson, i);
        if (cJSON_IsNull(item)) {
            fprintf(fp_out,"%s=null\n", item->string);
        }
        else if (cJSON_IsBool(item)) {
            fprintf(fp_out, "%s=%s\n", item->string, cJSON_IsTrue(item)?"true":"false");
        }
        else if (cJSON_IsNumber(item)) {
            fprintf(fp_out, "%s=%d\n", item->string, item->valueint);
        }
        else if (cJSON_IsString(item)) {
            fprintf(fp_out, "%s=%s\n", item->string, item->valuestring);
        }
        else if (cJSON_IsObject(item)) {
            fprintf(fp_out, "\"%s\":{", item->string);
            fprintf(fp_out_opt, "\"%s\":{", item->string);

            INT32 j = 0;
            for (j = 0; j < cJSON_GetArraySize(item); j++) {
                cJSON *titem = cJSON_GetArrayItem(item, j);

                if (!strcmp(titem->string, "uploadRepository:URL")) {
                    fprintf(fp_out, "\"%s\":\"%s\",", titem->string, titem->valuestring);
                    continue;
                }
                if (cJSON_IsNull(titem)) {
                    fprintf(fp_out, "\"%s\":null,", titem->string);
                    fprintf(fp_out_opt, "\"%s\":null,", titem->string);
                }
                else if (cJSON_IsBool(titem)) {
                    fprintf(fp_out, "\"%s\":%s,", titem->string, cJSON_IsTrue(titem) ? "true" : "false");
                    fprintf(fp_out_opt, "\"%s\":%s,", titem->string, cJSON_IsTrue(titem) ? "true" : "false");
                }
                else if (cJSON_IsNumber(titem)) {
                    fprintf(fp_out, "\"%s\":%d,", titem->string, titem->valueint);
                    fprintf(fp_out_opt, "\"%s\":%d,", titem->string, titem->valueint);
                }
                else if (cJSON_IsString(titem)) {
                    fprintf(fp_out, "\"%s\":\"%s\",", titem->string, titem->valuestring);
                    fprintf(fp_out_opt, "\"%s\":\"%s\",", titem->string, titem->valuestring);
                }
                /* Parse and dump telemetry profile entries */
                else if(cJSON_IsArray(titem)) {
                    INT32 k = 0;

                    fprintf(fp_out, "\"telemetryProfile\":[");
                    fprintf(fp_out_opt, "\"telemetryProfile\":[");
                    for (k = 0; k < cJSON_GetArraySize(titem); k++) {
                        cJSON *tproitem = cJSON_GetArrayItem(titem, k);
                        INT32 l = 0;

                        fprintf(fp_out, "{");
                        fprintf(fp_out_opt, "{");
                        for (l = 0; l < cJSON_GetArraySize(tproitem); l++) {
                            cJSON *tprochitem = cJSON_GetArrayItem(tproitem, l);
                            if ((!strcmp(tprochitem->string, "header")) ||
                                (!strcmp(tprochitem->string, "content")) ||
                                (!strcmp(tprochitem->string, "type"))) {
                                fprintf(fp_out, "\"%s\" : \"%s\",", tprochitem->string, tprochitem->valuestring);
                                fprintf(fp_out_opt, "\"%s\" : \"%s\",", tprochitem->string, tprochitem->valuestring);
                            }
                            else {
                                fprintf(fp_out, "\"%s\":\"%s\",", tprochitem->string, tprochitem->valuestring);
                                fprintf(fp_out_opt, "\"%s\":\"%s\",", tprochitem->string, tprochitem->valuestring);
                            }
                        }
                        fseek(fp_out, -1, SEEK_CUR);
                        fseek(fp_out_opt, -1, SEEK_CUR);
                        fprintf(fp_out, "},");
                        fprintf(fp_out_opt, "},");
                    } //for (k)

                    fseek(fp_out, -1, SEEK_CUR);
                    fseek(fp_out_opt, -1, SEEK_CUR);
                    fprintf(fp_out, "],");
                    fprintf(fp_out_opt, "],");

                } //else if(cJSON_IsArray(titem))
            } //for (j)

            fseek(fp_out, -1, SEEK_CUR);
            fseek(fp_out_opt, -1, SEEK_CUR);
            fprintf(fp_out, "}\n");
            fprintf(fp_out_opt, "}\n");
        } //else if (cJSON_IsObject(item))
    } //for (i)

    cJSON_Delete(pJson);
exit4:
    free(buff);
exit3:
    fclose(fp_out_opt);
exit2:
    fclose(fp_out);
exit1:
    fclose(fp_in);

    return ret;
}

#ifdef HAS_MAINTENANCE_MANAGER
/** @brief This Function stores the maintanance time and zone to conf file.
 *
 *  @param[in]  pCronptr  cron pattern
 *  @param[in]  pTimeZone time zone
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
static INT32 dcmSettingSaveMaintenance(INT8 *pCronptr, INT8* pTimeZone)
{
    FILE *fp = fopen(DCM_MAINT_CONF_PATH, "w");
    INT8 buffhr[4] = {0};
    INT8 buffmin[4] = {0};
    INT8 *ptr;
    INT32 i = 0, count = 0;

    if(fp == NULL) {
        DCMError("Unable to open %s\n", DCM_MAINT_CONF_PATH);
        return DCM_FAILURE;
    }

    ptr = buffmin;
    for(i = 0; i < strlen(pCronptr); i++) {
        if(pCronptr[i] == ' ') {
            count++;
            if(count > 1) {
                break;
            }
            ptr = buffhr;
            continue;
        }
        *ptr++ = pCronptr[i];
    }

    fprintf(fp, "start_hr=\"%d\"\n", atoi(buffhr));
    fprintf(fp, "start_min=\"%d\"\n", atoi(buffmin));
    fprintf(fp, "tz_mode=\"%s\"\n", pTimeZone);

    fclose(fp);

    return DCM_SUCCESS;
}
#endif

/** @brief This Function parses the settings file.
 *
 *  @param[in]   pConffile  settings file path
 *  @param[out]  pLogCron   Log Cron pattern
 *  @param[out]  pDifdCron  FW Update Cron pattern
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmSettingParseConf(VOID *pHandle, INT8 *pConffile,
                          INT8 *pLogCron, INT8 *pDifdCron)
{
    VOID  *pJsonHandle   = NULL;
    INT32  ret           = DCM_SUCCESS;
    INT32  confIntVal    = 0;
    INT8   temp          = 0;
    INT32  type          = 0;
    INT32  uploadCheck   = 0;
    INT8  *pUploadURL    = NULL;
    INT8  *pUploadprtl   = NULL;
    INT8  *pRDKPath      = NULL;
    INT8  *pExBuff       = NULL;
    INT8  *pTimezone     = NULL;

    DCMSettingsHandle *pdcmSetHandle = (DCMSettingsHandle *)pHandle;

    if(pdcmSetHandle == NULL) {
        DCMError("Input Handle is NULL\n");
        return DCM_FAILURE;
    }

    pUploadURL  = pdcmSetHandle->cUploadURL;
    pUploadprtl = pdcmSetHandle->cUploadPrtl;
    pRDKPath    = pdcmSetHandle->cRdkPath;
    pExBuff     = pdcmSetHandle->ctBuff;
    pTimezone   = pdcmSetHandle->cTimeZone;

    ret = dcmSettingJsonInit(pdcmSetHandle, pConffile, &pJsonHandle);
    if(ret) {
        DCMError("Failed to intialize Json\n");
        return DCM_FAILURE;
    }

    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_PROTOCOL,
                               pUploadprtl, &confIntVal, &type);

    if(ret || type != DCM_JSONITEM_STR || strlen(pUploadprtl) == 0) {
        DCMError("%s is not found in DCMSettings.conf, Setting to HTTP\n", DCM_LOGUPLOAD_PROTOCOL);
        strcpy(pUploadprtl, "HTTP");
    }

    DCMInfo("Log Upload protocol: %s\n", pUploadprtl);

    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_URL,
                               pUploadURL, &confIntVal, &type);
    if(ret || type != DCM_JSONITEM_STR || strlen(pUploadURL) == 0) {
        DCMWarn("%s is not found in DCMSettings.conf, Setting to default\n", DCM_LOGUPLOAD_URL);
        strcpy(pUploadURL, DCM_DEF_LOG_URL);
    }

    DCMInfo("Log Upload URL: %s\n", pUploadURL);

    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_TIMEZONE,
                               pTimezone, &confIntVal, &type);
    if(ret || type != DCM_JSONITEM_STR || strlen(pTimezone) == 0) {
        DCMWarn("%s is not found in DCMSettings.conf, Setting to default\n", DCM_TIMEZONE);
        strcpy(pTimezone, DCM_DEF_TIMEZONE);
    }

    DCMInfo("TimeZone : %s\n", pTimezone);

    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_REBOOT,
                               &temp, &uploadCheck, &type);

    DCMInfo("DCM_LOGUPLOAD_REBOOT: %d\n", uploadCheck);

    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_CRON,
                               pLogCron, &confIntVal, &type);
    if(ret || type != DCM_JSONITEM_STR) {
        *pLogCron = 0;
    }

    DCMInfo("DCM_LOGUPLOAD_CRON: %s\n", pLogCron);

    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_DIFD_CRON,
                               pDifdCron, &confIntVal, &type);
    if(ret || type != DCM_JSONITEM_STR) {
        *pDifdCron = 0;
    }

    DCMInfo("DCM_DIFD_CRON: %s\n", pDifdCron);

    if(uploadCheck == 1 && pdcmSetHandle->bRebootFlag == 0) {
        snprintf(pExBuff, EXECMD_BUFF_SIZE, "nice -n 19 /bin/busybox sh %s/uploadSTBLogs.sh %s 1 1 1 %s %s &",
                                                 pRDKPath, DCM_LOG_TFTP, pUploadprtl, pUploadURL);
        dcmUtilsSysCmdExec(pExBuff);
    }
    else if (uploadCheck == 0 && pdcmSetHandle->bRebootFlag == 0) {
        snprintf(pExBuff, EXECMD_BUFF_SIZE, "nice -n 19 /bin/busybox sh %s/uploadSTBLogs.sh %s 1 1 0 %s %s &",
                                                 pRDKPath, DCM_LOG_TFTP, pUploadprtl, pUploadURL);
        dcmUtilsSysCmdExec(pExBuff);
    }
    else {
        DCMWarn ("Nothing to do here for uploadCheck value = %d\n", uploadCheck);
    }

    if(strlen(pLogCron) == 0) {
        DCMWarn ("Uploading logs as DCM response is either null or not present\n");

        snprintf(pExBuff, EXECMD_BUFF_SIZE, "nice -n 19 /bin/busybox sh %s/uploadSTBLogs.sh %s 1 1 0 %s %s &",
                                                 pRDKPath, DCM_LOG_TFTP, pUploadprtl, pUploadURL);
        dcmUtilsSysCmdExec(pExBuff);
    }
    else {
        DCMInfo ("%s is present setting cron jobs\n", DCM_LOGUPLOAD_CRON);
    }

    if(strlen(pDifdCron) == 0) {
        DCMWarn ("difdCron is empty\n");
    }

    dcmSettingJsonUnInit(&pJsonHandle);

    /* Parse the conf file and store in opt and tmp folder which is used by other modules */
    if(dcmSettingStoreTempConf(pConffile, DCM_TMP_CONF, DCM_OPT_CONF)) {
        DCMWarn ("Storing to tmp, opt folder failed\n");
    }
#ifdef HAS_MAINTENANCE_MANAGER
    if(dcmSettingsGetMMFlag()) {
        if(dcmSettingSaveMaintenance(pDifdCron, pTimezone)) {
            DCMWarn ("Storing to rdk_maintenance.conf failed\n");
        }
    }
#endif
    return ret;
}


/** @brief This Function gets Maintenance manager flag.
 *
 *  @param[in]  None
 *
 *  @return  Status of maintenanace manager.
 *  @retval  int.
 */
INT32 dcmSettingsGetMMFlag()
{
    return g_bMMEnable;
}

/** @brief This Function returns the Log upload Protocol.
 *
 *  @param[]  None
 *
 *  @return  Returns the log upload URL.
 *  @retval  Returns Pointer to the URL buffer.
 */
INT8* dcmSettingsGetRDKPath(VOID *pHandle)
{
    DCMSettingsHandle *pdcmSetHandle = (DCMSettingsHandle *)pHandle;
    if(pdcmSetHandle) {
        return pdcmSetHandle->cRdkPath;
    }
    else {
        DCMError("Input handle is null\n");
        return NULL;
    }
}

/** @brief This Function returns the Log upload Protocol.
 *
 *  @param[]  None
 *
 *  @return  Returns the log upload URL.
 *  @retval  Returns Pointer to the URL buffer.
 */
INT8* dcmSettingsGetUploadProtocol(VOID *pHandle)
{
    DCMSettingsHandle *pdcmSetHandle = (DCMSettingsHandle *)pHandle;
    if(pdcmSetHandle) {
        return pdcmSetHandle->cUploadPrtl;
    }
    else {
        DCMError("Input handle is null\n");
        return NULL;
    }
}

/** @brief This Function returns the Log upload URL.
 *
 *  @param[]  None
 *
 *  @return  Returns the log upload URL.
 *  @retval  Returns Pointer to the URL buffer.
 */
INT8* dcmSettingsGetUploadURL(VOID *pHandle)
{
    DCMSettingsHandle *pdcmSetHandle = (DCMSettingsHandle *)pHandle;
    if(pdcmSetHandle) {
        return pdcmSetHandle->cUploadURL;
    }
    else {
        DCMError("Input handle is null\n");
        return NULL;
    }
}

/** @brief This Function Initializes the platform parameters from property files.
 *
 *  @param[]  None
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmSettingsInit(VOID **ppdcmSetHandle)
{
    INT32 ret = DCM_SUCCESS;
    DCMSettingsHandle *pdcmSetHandle = NULL;

    pdcmSetHandle = malloc(sizeof(DCMSettingsHandle));
    if(pdcmSetHandle == NULL) {
        DCMError("Memory allocation failed\n");
        return DCM_FAILURE;
    }

    memset(pdcmSetHandle, 0, sizeof(DCMSettingsHandle));

    *ppdcmSetHandle = pdcmSetHandle;

    if(dcmSettingGetValueFromFile(pdcmSetHandle->ctBuff, INCLUDE_PROP_FILE, "RDK_PATH",
                                  pdcmSetHandle->cRdkPath, "=")) {
        strcpy(pdcmSetHandle->cRdkPath, DCM_LIB_PATH);
    }

    if(dcmSettingGetValueFromFile(pdcmSetHandle->ctBuff, DEVICE_PROP_FILE, "ENABLE_MAINTENANCE",
                                  pdcmSetHandle->ctBuff, "=")) {
        g_bMMEnable = 0;
    }
    else {
        g_bMMEnable = 1;
    }

    return ret;
}

/** @brief This Function Initializes the platform parameters from property files.
 *
 *  @param[]  None
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
VOID dcmSettingsUnInit(VOID *pdcmSetHandle)
{
    if(pdcmSetHandle) {
        free(pdcmSetHandle);
    }
    else {
        DCMError("Input Handle is NULL\n");
    }
}

#ifdef DCM_DEFAULT_BOOTCONFIG
/** @brief This Function Parses the default config file post bootup.
 *
 *  @param[]  None
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */

 INT32 dcmSettingDefaultBoot(INT8 *defaultConfig)
 {
    if (defaultConfig == NULL) {
        DCMError("Input defaultConfig is NULL\n");
        return DCM_FAILURE;
    }
    else {
        return dcmSettingStoreTempConf(defaultConfig, DCM_TMP_CONF, DCM_OPT_CONF);
    }
 }
 #endif //DCM_DEFAULT_BOOTCONFIG
 