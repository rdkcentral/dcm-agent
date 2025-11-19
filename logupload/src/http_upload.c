#include "http_upload.h"
#include "context.h"
#include <stdio.h>
#include <string.h>
#include "downloadUtil.h"

#define URL_MAX 512
#define PATHNAME_MAX 256

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

/* Main upload runner - handles mTLS, curl setup, resource cleanup */

int runFileUpload(const char *upload_url, const char *src_file)
{
    void *curl = NULL;
    FileUpload_t file_upload;
    long http_code = 0;
    int status = 0;
    int curl_ret_code = -1;

#ifdef LIBRDKCERTSELECTOR
    MtlsAuth_t sec;
    MtlsAuthStatus mtls_status = MTLS_CERT_FETCH_SUCCESS;
    static rdkcertselector_h thisCertSel = NULL;
#endif

    if (upload_url == NULL || src_file == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Invalid arguments\n", __FUNCTION__);
        return -1;
    }

    /* Prepare upload descriptor */
    memset(&file_upload, 0, sizeof(FileUpload_t));
    char urlbuf[URL_MAX];
    char pathbuf[PATHNAME_MAX];
    strncpy(urlbuf, upload_url, URL_MAX - 1);
    urlbuf[URL_MAX - 1] = '\0';
    strncpy(pathbuf, src_file, PATHNAME_MAX - 1);
    pathbuf[PATHNAME_MAX - 1] = '\0';
    file_upload.url        = urlbuf;
    file_upload.pathname   = pathbuf;
    file_upload.pPostFields = NULL;
    file_upload.sslverify  = 1;
    file_upload.hashData   = NULL;

#ifdef LIBRDKCERTSELECTOR
    curl = doCurlInit();
    if (curl == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: CURL init failed\n", __FUNCTION__);
        return -1;
    }

    if (thisCertSel == NULL) {
        const char *certGroup = "MTLS";
        thisCertSel = rdkcertselector_new(NULL, NULL, certGroup);
        if (thisCertSel == NULL) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Cert selector init failed\n", __FUNCTION__);
            doStopDownload(curl);
            return -1;
        }
    }

    memset(&sec, 0, sizeof(MtlsAuth_t));

    do {
        /* Fetch (or refresh) mTLS credentials */
        mtls_status = loguploadGetCert(&sec, &thisCertSel);
        if (mtls_status == MTLS_CERT_FETCH_FAILURE) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: mTLS cert fetch failure\n", __FUNCTION__);
            status = -1;
            break;
        }

        curl_ret_code = doHttpFileUpload(curl, &file_upload, &sec, &http_code);

        if (curl_ret_code == 0 && http_code >= 200 && http_code < 300) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Upload success (HTTP %ld)\n", __FUNCTION__, http_code);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Upload attempt failed curl=%d http=%ld\n",
                    __FUNCTION__, curl_ret_code, http_code);
        }

        /* Retry while selector instructs TRY_ANOTHER (e.g., cert rotation or renewal) */
    } while (rdkcertselector_setCurlStatus(thisCertSel, curl_ret_code, file_upload.url) == TRY_ANOTHER);

    if (!(curl_ret_code == 0 && http_code >= 200 && http_code < 300)) {
        status = -1;
    }

    if (curl) {
        doStopDownload(curl);
        curl = NULL;
    }
    rdkcertselector_free(&thisCertSel);

#else  /* No LIBRDKCERTSELECTOR: plain upload without retry loop */
    curl = doCurlInit();
    if (curl == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: CURL init failed\n", __FUNCTION__);
        return -1;
    }

    curl_ret_code = doHttpFileUpload(curl, &file_upload, NULL, &http_code);
    if (curl_ret_code == 0 && http_code >= 200 && http_code < 300) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Upload success (HTTP %ld)\n", __FUNCTION__, http_code);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Upload failed curl=%d http=%ld\n",
                __FUNCTION__, curl_ret_code, http_code);
        status = -1;
    }

    if (curl) {
        doStopDownload(curl);
        curl = NULL;
    }
#endif

    return status;
}
