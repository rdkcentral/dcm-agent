#include "http_upload.h"
#include "context.h"
#include <stdio.h>
#include <string.h>
#include "downloadUtil.h"
#include "mtls_cert_selector.h"

#define URL_MAX 512
#define PATHNAME_MAX 256
#define S3_URL_BUF   1024

int extract_s3_url(const char *result_file, char *out_url, size_t out_url_sz) {
    FILE *fp = fopen(result_file, "rb");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Unable to open result file %s\n", __FUNCTION__, result_file);
        return -1;
    }
    if (!fgets(out_url, (int)out_url_sz, fp)) {
        fclose(fp);
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to read S3 URL\n", __FUNCTION__);
        return -1;
    }
    size_t len = strlen(out_url);
    if (len > 0 && out_url[len - 1] == '\n')
        out_url[len - 1] = '\0';
    fclose(fp);
    return 0;
}

/* Same as doHttpFileUpload for S3: can add custom headers if needed */
int doS3PutUpload(const char *s3url, const char *localfile, const MtlsAuth_t *auth)
{
    CURL *curl = curl_easy_init();
    FILE *fp = NULL;
    if (!curl) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: CURL init failed\n", __FUNCTION__);
        return -1;
    }
    fp = fopen(localfile, "rb");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to open %s\n", __FUNCTION__, localfile);
        curl_easy_cleanup(curl);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    curl_off_t filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    curl_easy_setopt(curl, CURLOPT_URL, s3url);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, filesize);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

#ifdef LIBRDKCERTSELECTOR
    if (auth) {
        CURLcode e1, e2, e3, e4;
        e1 = curl_easy_setopt(curl, CURLOPT_SSLCERT, auth->cert_name);
        e2 = curl_easy_setopt(curl, CURLOPT_SSLKEY,  auth->cert_name); // For P12, same file
        e3 = curl_easy_setopt(curl, CURLOPT_KEYPASSWD, auth->key_pas);
        e4 = curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, auth->cert_type);
        if ((e1 != CURLE_OK) || (e2 != CURLE_OK) || (e3 != CURLE_OK) || (e4 != CURLE_OK)) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Curl setopt error for cert\n", __FUNCTION__);
            fclose(fp); curl_easy_cleanup(curl); return -1;
        }
        if (auth->engine[0] != '\0') {
            curl_easy_setopt(curl, CURLOPT_SSLENGINE, auth->engine);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }
#endif

    long http_code = 0;
    CURLcode cc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (cc == CURLE_OK && http_code >= 200 && http_code < 300) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: S3 PUT success (HTTP %ld)\n", __FUNCTION__, http_code);
        return 0;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: S3 PUT failed: curl=%d, HTTP=%ld\n", __FUNCTION__, cc, http_code);
        return -1;
    }
}















int doHttpFileUpload(void *in_curl,
                     FileUpload_t *pfile_upload,
                     MtlsAuth_t *auth,
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


  
        // Second stage: Only proceed if metadata POST succeeded.
    if (curl_ret_code == 0 && http_code >= 200 && http_code < 300) {
        char s3_url[S3_URL_BUF];
        if (extract_s3_url("/tmp/httpresult.txt", s3_url, sizeof(s3_url)) == 0) {
#ifdef LIBRDKCERTSELECTOR
            /* Use same mTLS as initial POST */
            if (doS3PutUpload(s3_url, src_file, &sec) == 0) {
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Full upload complete (metadata and S3 PUT)\n", __FUNCTION__);
                return 0;
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: S3 PUT failed\n", __FUNCTION__);
                return -1;
            }
#else
            if (doS3PutUpload(s3_url, src_file, NULL) == 0) {
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Full upload complete (metadata and S3 PUT)\n", __FUNCTION__);
                return 0;
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: S3 PUT failed\n", __FUNCTION__);
                return -1;
            }
#endif
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to extract S3 URL from metadata response\n", __FUNCTION__);
            return -1;
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Not attempting S3 PUT after metadata upload failure\n", __FUNCTION__);
        return -1;
    }

  
    return status;
}
