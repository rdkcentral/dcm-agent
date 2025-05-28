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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "dcm_types.h"
#include "dcm_utils.h"
#include "dcm_parseconf.h"
#include "dcm.h"
#include "dcm_rbus.h"
#include "dcm_cronparse.h"
#include "dcm_schedjob.h"

static DCMDHandle *g_pdcmHandle = NULL;

/** @brief Call back function from Scheduler. This function
 *         Initiates the job
 *
 *  @param[in]  profileName  Scheduler name
 *
 *  @return  None.
 *  @retval  None.
 */
static VOID dcmRunJobs(const INT8* profileName, VOID *pHandle)
{
    DCMDHandle *pdcmHandle = (DCMDHandle *)pHandle;
    if(pHandle == NULL) {
        DCMError("Input Handle is NULL\n");
        return;
    }
    if(dcmSettingsGetMMFlag()) {
        DCMInfo("Maintenance manager enabled device - Cron job schedules for maintenance activities are disabled\n");
        return;
    }

    INT8 *pRDKPath = dcmSettingsGetRDKPath(pdcmHandle->pDcmSetHandle);
    INT8 *pExecBuff = pdcmHandle->pExecBuff;

    if(pRDKPath == NULL) {
        DCMWarn("RDK Patch is NULL, using %s\n", DCM_LIB_PATH);
        pRDKPath = DCM_LIB_PATH;
    }

    if(strcmp(profileName, DCM_LOGUPLOAD_SCHED) == 0) {
        INT8 *pPrctl = dcmSettingsGetUploadProtocol(pdcmHandle->pDcmSetHandle);
        INT8 *pURL   = dcmSettingsGetUploadURL(pdcmHandle->pDcmSetHandle);

        if(pPrctl == NULL) {
            DCMWarn("Log Upload protocol is NULL, using HTTP\n");
            pPrctl = "HTTP";
        }
        if(pURL == NULL) {
            DCMWarn("Log Upload protocol is NULL, using %s\n", DCM_DEF_LOG_URL);
            pURL = DCM_DEF_LOG_URL;
        }

        DCMInfo("\nStart log upload Script\n");
        snprintf(pExecBuff, EXECMD_BUFF_SIZE, "nice -n 19 /bin/busybox sh %s/uploadSTBLogs.sh %s 0 1 0 %s %s &",
                                               pRDKPath, DCM_LOG_TFTP, pPrctl, pURL);
    }
    else if(strcmp(profileName, DCM_DIFD_SCHED) == 0) {
        DCMInfo("Start FW update Script\n");
        snprintf(pExecBuff, EXECMD_BUFF_SIZE, "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
                                               pRDKPath);
    }

    dcmUtilsSysCmdExec(pExecBuff);
}

/** @brief Signal handler, un-intializes the module before exiting
 *
 *  @param[in]  sig  signal type
 *
 *  @return  None.
 *  @retval  None.
 */
static VOID sig_handler(INT32 sig)
{
    INT32 ret = DCM_SUCCESS;

    if ( sig == SIGINT  || sig == SIGTERM ||
         sig == SIGKILL || sig == SIGABRT) {
        DCMDebug("SIGINT received!\n");
        // As per the script, send MAINT_DCM_ERROR, MAINT_RFC_ERROR, MAINT_FWDOWNLOAD_ERROR,
        // MAINT_LOGUPLOAD_ERROR
        ret = dcmIARMEvntSend(DCM_IARM_ERROR);
        if(ret) {
            DCMWarn("Failed to send Event\n");
        }
        dcmDaemonMainUnInit(g_pdcmHandle);
        exit(1);
    }
}

/** @brief Initializes all modules
 *
 *  @param[in]  None
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on Success, DCM_FAILURE otherwise.
 */
INT32 dcmDaemonMainInit(DCMDHandle *pdcmHandle)
{
    INT32 ret = DCM_SUCCESS;
    INT8  t2_ver[32];

    /* Check if the Daemon is already running */
    ret = dcmUtilsCheckDaemonStatus();
    if(ret) {
        pdcmHandle->isDCMRunning = true;
        DCMError("DCM Daemon is already running\n");
        return ret;
    }

    /* Initialize the Parser */
    ret = dcmSettingsInit(&pdcmHandle->pDcmSetHandle);
    if(ret) {
        DCMError("Failed to init settings\n");
        return ret;
    }

    /* register the signals */
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGABRT, sig_handler);

    ret = dcmRbusInit(&pdcmHandle->pRbusHandle);
    if (ret != DCM_SUCCESS) {
        DCMError("Failed to init rbus\n");
        return ret;
    }

    DCMInfo("T2 is enabled\n");

    ret = dcmRbusGetT2Version(pdcmHandle->pRbusHandle, t2_ver);
    DCMInfo("T2 Version: %s\n", t2_ver);

    /* Initialize Rbus */
    ret = dcmRbusSubscribeEvents(pdcmHandle->pRbusHandle);
    if (ret != DCM_SUCCESS) {
        DCMError("Failed to init rbus\n");
        return ret;
    }

    pdcmHandle->pExecBuff = malloc(EXECMD_BUFF_SIZE);
    if(pdcmHandle->pExecBuff == NULL) {
        DCMError("Failed to allocate memeory\n");
        return DCM_FAILURE;
    }

    /* Initialize Schecduler */
    ret = dcmSchedInit();
    if(ret) {
        DCMError("Failed to init scheduler\n");
        return ret;
    }

    /* Add log upload job to Schecduler */
    pdcmHandle->pLogSchedHandle  = dcmSchedAddJob(DCM_LOGUPLOAD_SCHED,
                                                  (DCMSchedCB)dcmRunJobs,
                                                  (VOID *) pdcmHandle);
    if(pdcmHandle->pLogSchedHandle == NULL) {
        DCMError("Failed to Add Log Scheduler jobs\n");
        return DCM_FAILURE;
    }

    /* Add FW update job to Schecduler */
    pdcmHandle->pDifdSchedHandle = dcmSchedAddJob(DCM_DIFD_SCHED,
                                                  (DCMSchedCB)dcmRunJobs,
                                                  (VOID *) pdcmHandle);
    if(pdcmHandle->pDifdSchedHandle == NULL) {
        DCMError("Failed to Add FW Updater Scheduler jobs\n");
        return DCM_FAILURE;
    }

    return ret;
}

/** @brief De-Initializes all modules
 *
 *  @param[in]  None
 *
 *  @return  None.
 *  @retval  None.
 */
VOID dcmDaemonMainUnInit(DCMDHandle *pdcmHandle)
{
    if(pdcmHandle->isDCMRunning == false) {
        dcmUtilsRemovePIDfile();
    }
    if(pdcmHandle->pExecBuff) {
        free(pdcmHandle->pExecBuff);
    }

    dcmSettingsUnInit(pdcmHandle->pDcmSetHandle);
    dcmRbusUnInit(pdcmHandle->pRbusHandle);
    dcmSchedStopJob(pdcmHandle->pLogSchedHandle);
    dcmSchedStopJob(pdcmHandle->pDifdSchedHandle);
    dcmSchedRemoveJob(pdcmHandle->pLogSchedHandle);
    dcmSchedRemoveJob(pdcmHandle->pDifdSchedHandle);
    dcmSchedUnInit();

    memset(pdcmHandle, 0, sizeof(DCMDHandle));
}

/** @brief Main function
 *
 *  @param[in]  argc   No. of arguments
 *  @param[in]  argv   arguments array
 *
 *  @return  status.
 *  @retval  status.
 */
int main(int argc, char* argv[])
{
    pid_t process_id       = 0;
    pid_t sid              = 0;
    INT32 ret              = DCM_SUCCESS;
    INT32 count            = 0;

    DCMLOGInit();

    g_pdcmHandle = malloc(sizeof(DCMDHandle));
    if(g_pdcmHandle == NULL) {
        DCMError("Memory allocation failed, Closing the daemon\n");
        return DCM_FAILURE;
    }

    memset(g_pdcmHandle, 0, sizeof(DCMDHandle));

    g_pdcmHandle->isDebugEnabled = true;

    DCMInfo("Starting DCM Process: %d\n", getpid());

    /* Create child process */
    process_id = fork();
    if (process_id < 0) {
        DCMError("fork failed!\n");
        ret = DCM_FAILURE;
        goto exit2;
    } else if (process_id > 0) {
        DCMInfo("Exiting the main process\n");
        return 0;
    }

    /* unmask the file mode */
    umask(0);

    /* set new session */
    sid = setsid();
    if (sid < 0) {
        DCMError("setsid failed!\n");
        ret = DCM_FAILURE;
        goto exit2;
    }

    /* Change the current working directory to root. */
    chdir("/");

    if (g_pdcmHandle->isDebugEnabled != true) {
        /* Close stdin. stdout and stderr */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    DCMDebug("Initializing DCM Component: %d\n", getpid());

    ret = dcmDaemonMainInit(g_pdcmHandle);

    if(ret) {
        DCMError("Initialization of DCM Process Failed, closing the DCM Process!!!\n");
        goto exit1;
    }

    DCMDebug("Initializing DCM Component Done\n");

    #ifdef DCM_DEFAULT_BOOTCONFIG
    DCMInfo("Loading the Default config from : %s\n",DCM_DEFAULT_BOOTCONFIG);
    ret = dcmSettingDefaultBoot(DCM_DEFAULT_BOOTCONFIG);
    if(ret != DCM_SUCCESS) {
        DCMError("Failed to load default config file\n");
    }
    #endif

    while(1) {
        if(count > 10) {
            DCMInfo("Waiting for Telemetry to up and running to Subscribe the events\n");
            count = 0;
        }
        count++;
        if(dcmRbusGetEventSubStatus(g_pdcmHandle->pRbusHandle)) {
            break;
        }
        sleep(1);
    }

    DCMInfo("Telemetry Events subscriptions is success\n");

    ret = dcmRbusSendEvent(g_pdcmHandle->pRbusHandle);
    if(ret) {
        DCMError("Reload config event failed!!!\n");
    }

    DCMInfo("Sent Event to telemetry for configuraion path\n");

    while(1) {
        INT32 ret = DCM_SUCCESS;
        INT8 *pconfPath = NULL;

        /* Wait for event Device.DCM.Processconfig */
        if(dcmRbusSchedJobStatus(g_pdcmHandle->pRbusHandle)) {
            DCMInfo("Start Scheduling\n");

            pconfPath = dcmRbusGetConfPath(g_pdcmHandle->pRbusHandle);

            if(pconfPath == NULL) {
                DCMWarn("conf file pointer is null\n");
                sleep(1);
                continue;
            }

            ret = dcmSettingParseConf(g_pdcmHandle->pDcmSetHandle, pconfPath,
                                      g_pdcmHandle->logCron,
                                      g_pdcmHandle->difdCron);
            if(ret == DCM_SUCCESS) {
                dcmSchedStartJob(g_pdcmHandle->pLogSchedHandle, g_pdcmHandle->logCron);
                dcmSchedStartJob(g_pdcmHandle->pDifdSchedHandle, g_pdcmHandle->difdCron);

                ret = dcmIARMEvntSend(DCM_IARM_COMPLETE);
                if(ret) {
                    DCMError("Failed to send Event\n");
                }
            }
            else {
                DCMWarn("Failed to parse the conf file\n");
            }

            dcmRbusSchedResetStatus(g_pdcmHandle->pRbusHandle);
        }
        sleep(1);
    } //while(1)

    DCMInfo("Exiting DCM Component\n");

exit1:
    dcmDaemonMainUnInit(g_pdcmHandle);
exit2:
    if(g_pdcmHandle) {
        free(g_pdcmHandle);
        g_pdcmHandle = NULL;
    }
    if(ret) {
        ret = dcmIARMEvntSend(DCM_IARM_ERROR);
        if(ret) {
            DCMError("Failed to send Event\n");
        }
    }
    return ret;
}
