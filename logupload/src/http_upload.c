#include "http_upload.h"
#include <stdio.h>
#include <string.h>

int doHttpFileUpload(void *in_curl,
                     FileUpload_t *pfile_upload,
                     const MtlsAuth_t *auth,
                     long *out_httpCode)
{
    CURL *curl;
    CURLcode ret_code = CURLE_OK;
    struct curl_slist *headers = NULL;
    FILE *resp_fp = NULL;

    if (out_httpCode) {
        *out_httpCode = 0;
    }

    /* Parameter validation */
    if (in_curl == NULL || pfile_upload == NULL ||
        out_httpCode == NULL || pfile_upload->pathname == NULL ||
        pfile_upload->url == NULL)
    {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Parameter validation failed\n", __FUNCTION__);
        return (int)UPLOAD_FAIL;
    }

    curl = (CURL *)in_curl;

    /* Set common curl options */
    ret_code = setCommonCurlOpt(curl,
                                pfile_upload->url,
                                pfile_upload->pPostFields,
                                pfile_upload->sslverify);
    if (ret_code != CURLE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: setCommonCurlOpt failed: %s\n",
                __FUNCTION__, curl_easy_strerror(ret_code));
        return (int)ret_code;
    }

    /* Apply mTLS if provided */
    if (auth != NULL) {
        ret_code = setMtlsHeaders(curl, auth);
        if (ret_code != CURLE_OK) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: setMtlsHeaders failed: %s\n",
                    __FUNCTION__, curl_easy_strerror(ret_code));
            return (int)ret_code;
        }
    }

    /* Build POST fields: include filename plus any extra fields already set */
    char postfields[512];
    if (pfile_upload->pPostFields && pfile_upload->pPostFields[0] != '\0') {
        snprintf(postfields, sizeof(postfields), "filename=%s&%s",
                 pfile_upload->pathname, pfile_upload->pPostFields);
    } else {
        snprintf(postfields, sizeof(postfields), "filename=%s",
                 pfile_upload->pathname);
    }
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);

    /* Additional headers (hash/time) */
    if (pfile_upload->hashData != NULL) {
        if (pfile_upload->hashData->hashvalue) {
            headers = curl_slist_append(headers, pfile_upload->hashData->hashvalue);
        }
        if (pfile_upload->hashData->hashtime) {
            headers = curl_slist_append(headers, pfile_upload->hashData->hashtime);
        }
        if (headers) {
            ret_code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            if (ret_code != CURLE_OK) {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD,
                        "%s: CURLOPT_HTTPHEADER failed: %s\n",
                        __FUNCTION__, curl_easy_strerror(ret_code));
                curl_slist_free_all(headers);
                return (int)ret_code;
            }
        }
    }

    /* Capture response body */
    resp_fp = fopen("/tmp/httpresult.txt", "wb");
    if (!resp_fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to open response file\n", __FUNCTION__);
        if (headers) curl_slist_free_all(headers);
        return (int)UPLOAD_FAIL;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_fp);

    /* Verbose (optional) */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* Perform request */
    ret_code = curl_easy_perform(curl);
    if (ret_code != CURLE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: curl_easy_perform failed: %s\n",
                __FUNCTION__, curl_easy_strerror(ret_code));
    } else {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: curl_easy_perform success\n", __FUNCTION__);
    }

    /* Extract HTTP code */
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_httpCode);
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: HTTP response code=%ld\n",
            __FUNCTION__, *out_httpCode);

    /* Cleanup */
    fclose(resp_fp);
    if (headers) {
        curl_slist_free_all(headers);
    }

    return (int)ret_code;
}
