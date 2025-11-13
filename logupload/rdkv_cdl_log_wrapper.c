/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
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
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>


#include "rdkv_cdl_log_wrapper.h"

int log_init( ) {
    printf("RDKLOG init completed\n");
#if defined(RDK_LOGGER)
    rdk_logger_init(DEBUG_INI_NAME);
#endif

    return 0;
}

void log_exit( ) {
    printf("RDKLOG deinit\n");
#if defined(RDK_LOGGER)
    rdk_logger_deinit();
#endif
}

void swLog(unsigned int level, const char *msg, ...) {
    va_list arg;
    char *pTempChar = NULL;
    int ret = 0;
    int messageLen;

    va_start(arg, msg);
    messageLen = vsnprintf(NULL, 0, msg, arg);
    va_end(arg);
    if( messageLen > 0 )
    {
        messageLen++;
        pTempChar = (char *) malloc(messageLen);
        if(pTempChar) {
            va_start(arg, msg);
            ret = vsnprintf(pTempChar, messageLen, msg, arg);
            if(ret < 0) {
                perror(pTempChar);
            }
            va_end(arg);
            printf("LOG.RDK.FWUPG : %s\n", pTempChar);
            free(pTempChar);
        }
    }
}
