/*
 * Copyright 2025 RDK Management
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

/**
 * @file main_upload_test.c
 * @brief Test program for upload utilities
 */

#include <stdio.h>
#include <stdlib.h>
#include "uploadUtil.h"
#include "mtls_upload.h"
//#include "codebig_upload.h"
#include "rdk_debug.h"

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4) {
        printf("Usage: %s <file_path> <codebig_server_type> [upload_url]\n", argv[0]);
        printf("  codebig_server_type: 1=HTTP_SSR_CODEBIG, 3=HTTP_XCONF_CODEBIG, 0=standard_upload\n");
        printf("  upload_url: Required for standard upload (server_type=0)\n");
        return 1;
    }
    
    rdk_logger_init("/etc/debug.ini");
    const char *file_path = argv[1];
    int server_type = atoi(argv[2]);
    const char *upload_url = (argc == 4) ? argv[3] : NULL;

    printf("Testing upload: %s\n", file_path);

    int result = -1;
    
    if (server_type == HTTP_SSR_CODEBIG || server_type == HTTP_XCONF_CODEBIG) {
        printf("Using CodeBig upload with server type: %d\n", server_type);
        result = uploadFileWithCodeBigFlow(file_path, server_type);
    } else if (server_type == 0) {
        if (!upload_url) {
            printf("Error: upload_url required for standard upload (server_type=0)\n");
            return 1;
        }
        printf("Using standard two-stage upload to: %s\n", upload_url);
        result = uploadFileWithTwoStageFlow(upload_url, file_path);
    } else {
        printf("Error: Invalid server type: %d\n", server_type);
        return 1;
    }

    if (result == 0) {
        printf("Upload successful\n");
    } else {
        printf("Upload failed with code: %d\n", result);
    }

    return result;
}



/*
#include <stdio.h>
#include <stdlib.h>
#include "uploadUtil.h"
#include "mtls_upload.h"
#include "rdk_debug.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <upload_url> <file_path>\n", argv[0]);
        return 1;
    }
    rdk_logger_init("/etc/debug.ini");
    const char *upload_url = argv[1];
    const char *file_path = argv[2];

    printf("Testing upload: %s -> %s\n", file_path, upload_url);

    int result = uploadFileWithTwoStageFlow(upload_url, file_path);

    if (result == 0) {
        printf("Upload successful\n");
    } else {
        printf("Upload failed with code: %d\n", result);
    }

    return result;
}
*/
