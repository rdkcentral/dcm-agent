/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cjson/cJSON.h"
#include "privacy_mode.h"
#include "secure_wrapper.h"
#include "rdk_logger.h"
#include "context.h"
#include "rdk_logger_types.h"

char* get_security_token(void)
{
    char *sToken = NULL;
    char pSecurityOutput[256] = {0};
    FILE *pSecurity = v_secure_popen("r", "/usr/bin/WPEFrameworkSecurityUtility");

    if (!pSecurity) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to open security utility\n", __FUNCTION__);
        return NULL;
    }

    if (fgets(pSecurityOutput, sizeof(pSecurityOutput), pSecurity) != NULL) {
        cJSON *root = cJSON_Parse(pSecurityOutput);
        if (root) {
            cJSON *res = cJSON_GetObjectItem(root, "success");
            if (cJSON_IsTrue(res)) {
                cJSON *token = cJSON_GetObjectItem(root, "token");
                if (token && cJSON_IsString(token) && token->valuestring) {
                    sToken = strdup(token->valuestring);
                    if (sToken) {
                        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Security Token retrieved successfully\n", __FUNCTION__);
                    } else {
                        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: strdup failed\n", __FUNCTION__);
                    }
                } else {
                    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: 'token' missing or invalid\n", __FUNCTION__);
                }
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: success != true in token output\n", __FUNCTION__);
            }
            cJSON_Delete(root);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: JSON parse error\n", __FUNCTION__);
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: fgets failed reading security utility\n", __FUNCTION__);
    }

    v_secure_pclose(pSecurity);
    return sToken;
}


static size_t writeCurlResponse(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t total = size * nmemb;
    char **pBuf = (char **)userdata;

    if (total == 0)
        return 0;

    char *old = *pBuf;
    size_t oldLen = old ? strlen(old) : 0;

    char *newBuf = (char *)realloc(old, oldLen + total + 1);
    if (!newBuf) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: realloc failed\n", __FUNCTION__);
        return 0; /* abort transfer */
    }

    memcpy(newBuf + oldLen, ptr, total);
    newBuf[oldLen + total] = '\0';
    *pBuf = newBuf;
    return total;
}

char* getJsonRPCData(const char *postData)
{
    if (!postData) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: postData NULL\n", __FUNCTION__);
        return NULL;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: curl init failed\n", __FUNCTION__);
        return NULL;
    }

    char *token = get_security_token();
    if (!token) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: security token retrieval failed\n", __FUNCTION__);
        curl_easy_cleanup(curl);
        return NULL;
    }

    char authHeader[512];
    snprintf(authHeader, sizeof(authHeader), "Authorization: Bearer %s", token);
    free(token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, authHeader);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char *response = NULL;

    if (curl_easy_setopt(curl, CURLOPT_URL, JSONRPC_URL) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_POST, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCurlResponse) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L) != CURLE_OK) {

        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: curl_easy_setopt failure\n", __FUNCTION__);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(response);
        return NULL;
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: perform error %d (%s)\n",
                __FUNCTION__, res, curl_easy_strerror(res));
        free(response);
        response = NULL;
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: HTTP %ld curl %d\n", __FUNCTION__, http_code, res);
        if (http_code != 200 && http_code != 204) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Non-OK HTTP code %ld\n", __FUNCTION__, http_code);
            /* Keep body for diagnostics */
        }
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return response;
}


static privacy_mode_t privacy_mode_from_string(const char *mode)
{
    if (!mode) return PRIVACY_MODE_UNKNOWN;
    if (strcmp(mode, "SHARE") == 0) return PRIVACY_MODE_SHARE;
    if (strcmp(mode, "DO_NOT_SHARE") == 0) return PRIVACY_MODE_DO_NOT_SHARE;
    return PRIVACY_MODE_UNKNOWN;
}

char* get_privacy_mode_string(void)
{
    const char *req = "{\"jsonrpc\":\"2.0\",\"id\":\"3\",\"method\":\"org.rdk.System.getPrivacyMode\"}";
    char *response = getJsonRPCData(req);
    char *value = NULL;

    if (!response) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: No response\n", __FUNCTION__);
        return NULL;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: JSON parse failed\n", __FUNCTION__);
        free(response);
        return NULL;
    }

    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (result) {
        cJSON *pm = cJSON_GetObjectItem(result, "privacyMode");
        if (pm && cJSON_IsString(pm) && pm->valuestring) {
            value = strdup(pm->valuestring);
            if (value) {
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: privacyMode=%s\n", __FUNCTION__, value);
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: strdup failed\n", __FUNCTION__);
            }
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: privacyMode missing/invalid\n", __FUNCTION__);
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: 'result' missing\n", __FUNCTION__);
    }

    cJSON_Delete(root);
    free(response);
    return value;
}

int get_privacy_mode(privacy_mode_t *outMode)
{
    if (!outMode) return NOK;
    *outMode = PRIVACY_MODE_UNKNOWN;
    char *str = get_privacy_mode_string();
    if (!str) return NOK;
    *outMode = privacy_mode_from_string(str);
    free(str);
    return (*outMode == PRIVACY_MODE_UNKNOWN) ? NOK : OK;
}

int is_privacy_mode_do_not_share(void)
{
    privacy_mode_t mode;
    if (get_privacy_mode(&mode) != OK)
        return 0;
    return (mode == PRIVACY_MODE_DO_NOT_SHARE) ? 1 : 0;
}

/* ---------------- Optional test harness ----------------
#ifdef TEST_PRIVACY_MODE
int main(void)
{
    char *pm = get_privacy_mode_string();
    if (pm) {
        printf("PrivacyMode: %s\n", pm);
        free(pm);
    } else {
        printf("Failed to get privacy mode\n");
    }

    privacy_mode_t em;
    if (get_privacy_mode(&em) == OK) {
        printf("Enum: %d\n", em);
    }
    printf("DO_NOT_SHARE? %d\n", is_privacy_mode_do_not_share());
    return 0;
}
#endif
*/
