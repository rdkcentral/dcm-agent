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

#ifndef VIDEO_DWNLUTILS_DOWNLOADUTIL_H_
#define VIDEO_DWNLUTILS_DOWNLOADUTIL_H_

#define DWNL_FAIL -1
#define DWNL_SUCCESS 1
#define DWNL_UNPAUSE_FAIL -2

#include "urlHelper.h"
/*For TLS 1.2 and mtls*/
//int doHttpFileDownload(void *in_curl, FileDwnl_t *pfile_dwnl, MtlsAuth_t *auth, unsigned int max_dwnl_speed, char *dnl_start_pos, int *out_httpCode );

/* For OAuth */
//int doAuthHttpFileDownload(const char* signedHttpUrl, const char* localFileLocation, const char* oAuthKey,  int* out_httpCode );


int doHttpFileDownload(void *in_curl, FileDwnl_t *pfile_dwnl, MtlsAuth_t *auth, unsigned int max_dwnl_speed, char *dnl_start_pos, int *out_httpCode );
int doAuthHttpFileDownload(void *in_curl, FileDwnl_t *pfile_dwnl, int *out_httpCode);
void *doCurlInit(void);
void doStopDownload(void *curl);
int doInteruptDwnl(void *in_curl, unsigned int max_dwnl_speed);
unsigned int doGetDwnlBytes(void *in_curl);
int setForceStop(int value);
int getJsonRpcData(void *in_curl, FileDwnl_t *pfile_dwnl, char *jsonrpc_auth_token, int *out_httpCode );
int doCurlPutRequest(void *in_curl, FileDwnl_t *pfile_dwnl, char *jsonrpc_auth_token, int *out_httpCode);
//int doHttpFileUpload(void *in_curl, FileDwnl_t *pfile_dwnl, MtlsAuth_t *auth, int *out_httpCode );

#endif /* VIDEO_DWNLUTILS_DOWNLOADUTIL_H_ */
