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
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <stdlib.h>

#include "dcm_types.h"
#include "dcm_utils.h"
#include "dcm_parseconf.h"

#ifdef RDK_LOGGER_ENABLED
INT32  g_rdk_logger_enabled = 0;
#endif

#define IARM_BUS_DCM_NAME "DCM"

void DCMLOGInit()
{
#ifdef RDK_LOGGER_ENABLED
    if (0 == rdk_logger_init(DEBUG_INI_NAME)) {
        g_rdk_logger_enabled = 1;
    }
#endif
}

/** @brief This Function checks if the file present or not.
 *
 *  @param[in]  file_name  file path
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS if present, DCM_FAILURE otherwise.
 */
INT32 dcmUtilsFilePresentCheck(const INT8 *file_name)
{
    INT32 ret = DCM_SUCCESS;
    if(file_name == NULL) {
        DCMError("Invalid Parameter\n");
        return DCM_FAILURE;
    }
    struct stat sfile;
    memset(&sfile, '\0', sizeof(sfile));

    if(stat(file_name, &sfile)) {
        return DCM_FAILURE;
    }

    return ret;
}

/** @brief This Function executes the system command and copies the output.
 *
 *  @param[in]  cmd  command to be executed
 *  @param[out] out  Output buffer
 *  @param[in]  len  Output buffer length
 *
 *  @return  None.
 *  @retval  None.
 */
VOID dcmUtilsCopyCommandOutput (INT8 *cmd, INT8 *out, INT32 len)
{
    FILE *fp;

    if(out != NULL)
        out[0] = 0;

    fp = popen (cmd, "r");
    if (fp) {
        if(out) {
            if (fgets (out, len, fp) != NULL) {
                size_t len = strlen (out);
                if ((len > 0) && (out[len - 1] == '\n'))
                    out[len - 1] = 0;
            }
        }
        pclose (fp);
    }
    else {
        DCMWarn("Failed to run the command: %s\n", cmd);
    }
}

/** @brief This Function executes the system command.
 *
 *  @param[in]  cmd  command to be executed
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmUtilsSysCmdExec(INT8 *cmd)
{
    INT32 ret = DCM_SUCCESS;

    if (cmd == NULL) {
        DCMError("parameter is NULL\n");
        return DCM_FAILURE;
    }

    dcmUtilsCopyCommandOutput(cmd, NULL, 0);

    return ret;
}

/** @brief This Function checks if daemon is already running or not.
 *
 *  @param[in]  None
 *
 *  @return  Returns the status of the daemon.
 *  @retval  Returns DCM_SUCCESS if daemon not running, DCM_FAILURE otherwise.
 */
INT32 dcmUtilsCheckDaemonStatus()
{
    FILE *fp = NULL;
    INT8 PID[32];
    INT8 filePath[64];

    fp = fopen(DCM_PID_FILE, "r");

    if (NULL != fp) {
        /* exit if an instance is already running */
        fgets(PID, sizeof(PID), fp);
        fclose(fp);

        snprintf(filePath, sizeof(filePath), "/proc/%s", PID);

        if (dcmUtilsFilePresentCheck(filePath) == 0) {
            DCMWarn("Daemon is already running %s\n", filePath);
            return DCM_FAILURE;
        }
        DCMInfo("pid file not present %s\n", filePath);
    }

    DCMInfo("Opening new pid file\n");
    fp = fopen(DCM_PID_FILE, "w");
    if(fp == NULL) {
        DCMWarn("Failed to open PID file %s\n", DCM_PID_FILE);
        return DCM_FAILURE;
    }

    fprintf(fp, "%d", getpid());
    fclose(fp);

    return DCM_SUCCESS;
}

/** @brief This Function removes the dcm PID file.
 *
 *  @param[in]  None
 *
 *  @return  None.
 *  @retval  None.
 */
VOID dcmUtilsRemovePIDfile()
{
    FILE *fp = NULL;

    fp = fopen(DCM_PID_FILE, "r");
    if(fp) {
        fclose(fp);
        remove(DCM_PID_FILE);
    }
}

/** @brief This Function sends events to MM using IARM bus.
 *
 *  @param[in]  status Event Status
 *
 *  @return  Returns the status of the operation.
 *  @retval  Returns DCM_SUCCESS on success, DCM_FAILURE otherwise.
 */
INT32 dcmIARMEvntSend(INT32 status)
{
    INT32 ret = DCM_SUCCESS;
    return ret;
}
