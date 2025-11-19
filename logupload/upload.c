#include "http_upload.h"
#include "context.h"
#include "downloadUtil.h"
#include "mtls_cert_selector.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define URL_MAX        512
#define PATHNAME_MAX   256
#define S3_URL_BUF     1024
#define META_RESP_FILE "/tmp/httpresult.txt"

/* ---------------------------------------------------------------------------
 * Internal helper status
 * ---------------------------------------------------------------------------*/
typedef enum {
    UP_OK            = 0,
    UP_ERR_PARAM     = -10,
    UP_ERR_CURL      = -11,
    UP_ERR_FILE      = -12,
    UP_ERR_HTTP      = -13,
    UP_ERR_MTLS      = -14,
    UP_ERR_PARSE     = -15
} UploadStatus;

/* ---------------------------------------------------------------------------
 * Utility: read first line of a file into buffer (existing behavior preserved)
 * ---------------------------------------------------------------------------*/
static int extract_s3_url_file(const char *result_file, char *out_url, size_t out_url_sz)
{
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

/* ---------------------------------------------------------------------------
 * Optional in-memory response capture (future extensibility)
 * ---------------------------------------------------------------------------*/
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} MemBuf;

static size_t write_mem_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t total = size * nmemb;
    MemBuf *mb = (MemBuf *)userdata;
    if (!mb || total == 0) return 0;

    if (mb->len + total + 1 > mb->cap) {
        size_t new_cap = (mb->cap == 0) ? (total + 128) : (mb->cap * 2 + total);
        char *new_buf = (char *)realloc(mb->buf, new_cap);
        if (!new_buf) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: realloc failed\n", __FUNCTION__);
            return 0;
        }
        mb->buf = new_buf;
        mb->cap = new_cap;
    }
    memcpy(mb->buf + mb->len, ptr, total);
    mb->len += total;
    mb->buf[mb->len] = '\0';
    return total;
}

static int extract_s3_url_memory(const char *mem, char *out_url, size_t out_sz)
{
    if (!mem || !out_url || out_sz == 0) return -1;
    /* First line only */
    const char *newline = strchr(mem, '\n');
    size_t cplen = newline ? (size_t)(newline - mem) : strlen(mem);
    if (cplen >= out_sz) cplen = out_sz - 1;
    memcpy(out_url, mem, cplen);
    out_url[cplen] = '\0';
    return (cplen > 0) ? 0 : -1;
}

/* ---------------------------------------------------------------------------
 * S3 PUT upload (extracted & generalized)
 * ---------------------------------------------------------------------------*/
static UploadStatus perform_s3_put(const char *s3url, const char *localfile, const MtlsAuth_t *auth)
{
    if (!s3url || !localfile) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Invalid parameters\n", __FUNCTION__);
        return UP_ERR_PARAM;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: CURL init failed\n", __FUNCTION__);
        return UP_ERR_CURL;
    }

    FILE *fp = fopen(localfile, "rb");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to open %s\n", __FUNCTION__, localfile);
        curl_easy_cleanup(curl);
        return UP_ERR_FILE;
    }

    fseek(fp, 0, SEEK_END);
    curl_off_t filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    curl_easy_setopt(curl, CURLOPT_URL, s3url);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, filesize);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

    if (auth) {
        CURLcode e1 = curl_easy_setopt(curl, CURLOPT_SSLCERT, auth->cert_name);
        CURLcode e2 = curl_easy_setopt(curl, CURLOPT_SSLKEY,  auth->cert_name);
        CURLcode e3 = curl_easy_setopt(curl, CURLOPT_KEYPASSWD, auth->key_pas);
        CURLcode e4 = curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, auth->cert_type);
        if (e1 || e2 || e3 || e4) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Curl setopt error for cert\n", __FUNCTION__);
            fclose(fp); curl_easy_cleanup(curl);
            return UP_ERR_MTLS;
        }
        if (auth->engine[0] != '\0') {
            curl_easy_setopt(curl, CURLOPT_SSLENGINE, auth->engine);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    long http_code = 0;
    CURLcode cc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    fclose(fp);
    curl_easy_cleanup(curl);

    if (cc == CURLE_OK && http_code >= 200 && http_code < 300) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: S3 PUT success (HTTP %ld)\n", __FUNCTION__, http_code);
        return UP_OK;
    }
    RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: S3 PUT failed: curl=%d, HTTP=%ld\n", __FUNCTION__, cc, http_code);
    return UP_ERR_HTTP;
}

/* ---------------------------------------------------------------------------
 * Build POST fields for metadata request
 * ---------------------------------------------------------------------------*/
static void build_postfields(const FileUpload_t *fu, char *outbuf, size_t outbuf_sz)
{
    if (!fu || !outbuf || outbuf_sz == 0) return;
    if (fu->pPostFields && fu->pPostFields[0] != '\0') {
        snprintf(outbuf, outbuf_sz, "filename=%s&%s", fu->pathname, fu->pPostFields);
    } else {
        snprintf(outbuf, outbuf_sz, "filename=%s", fu->pathname);
    }
}

/* ---------------------------------------------------------------------------
 * Apply hash headers if present
 * ---------------------------------------------------------------------------*/
static CURLcode apply_hash_headers(CURL *curl, const FileUpload_t *fu, struct curl_slist **out_list)
{
    if (!curl || !fu) return CURLE_BAD_FUNCTION_ARGUMENT;
    if (!fu->hashData) return CURLE_OK;

    struct curl_slist *headers = NULL;
    if (fu->hashData->hashvalue) {
        headers = curl_slist_append(headers, fu->hashData->hashvalue);
    }
    if (fu->hashData->hashtime) {
        headers = curl_slist_append(headers, fu->hashData->hashtime);
    }

    if (headers) {
        CURLcode rc = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        if (rc != CURLE_OK) {
            curl_slist_free_all(headers);
            return rc;
        }
        *out_list = headers;
    }
    return CURLE_OK;
}

/* ---------------------------------------------------------------------------
 * Metadata upload (POST) – writes response either to file or memory
 * ---------------------------------------------------------------------------*/
static UploadStatus perform_metadata_upload(CURL *curl,
                                            FileUpload_t *fu,
                                            const MtlsAuth_t *auth,
                                            long *http_code_out,
                                            int use_memory_capture,
                                            MemBuf *opt_mem)
{
    if (!curl || !fu || !http_code_out) return UP_ERR_PARAM;

    CURLcode rc = setCommonCurlOpt(curl,
                                   fu->url,
                                   fu->pPostFields,
                                   fu->sslverify);
    if (rc != CURLE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: setCommonCurlOpt failed: %s\n",
                __FUNCTION__, curl_easy_strerror(rc));
        return UP_ERR_CURL;
    }

    if (auth) {
        rc = setMtlsHeaders(curl, auth);
        if (rc != CURLE_OK) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: setMtlsHeaders failed: %s\n",
                    __FUNCTION__, curl_easy_strerror(rc));
            return UP_ERR_MTLS;
        }
    }

    char postfields[512];
    build_postfields(fu, postfields, sizeof(postfields));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);

    struct curl_slist *headers = NULL;
    rc = apply_hash_headers(curl, fu, &headers);
    if (rc != CURLE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: apply_hash_headers failed: %s\n",
                __FUNCTION__, curl_easy_strerror(rc));
        return UP_ERR_CURL;
    }

    FILE *resp_fp = NULL;
    if (use_memory_capture) {
        if (opt_mem) {
            memset(opt_mem, 0, sizeof(*opt_mem));
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, opt_mem);
        }
    } else {
        resp_fp = fopen(META_RESP_FILE, "wb");
        if (!resp_fp) {
            if (headers) curl_slist_free_all(headers);
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to open response file\n", __FUNCTION__);
            return UP_ERR_FILE;
        }
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp_fp);
    }

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

    CURLcode perf = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code_out);

    if (resp_fp) fclose(resp_fp);
    if (headers) curl_slist_free_all(headers);

    if (perf != CURLE_OK) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: curl_easy_perform failed: %s\n",
                __FUNCTION__, curl_easy_strerror(perf));
        return UP_ERR_CURL;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Metadata HTTP code=%ld\n",
            __FUNCTION__, *http_code_out);

    if (!(*http_code_out >= 200 && *http_code_out < 300)) {
        return UP_ERR_HTTP;
    }

    return UP_OK;
}

/* ---------------------------------------------------------------------------
 * High-level step: parse S3 URL from either memory or file
 * ---------------------------------------------------------------------------*/
static UploadStatus obtain_s3_url(int used_memory, const MemBuf *mb, char *out_url, size_t out_url_sz)
{
    if (used_memory) {
        if (!mb || mb->len == 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Empty memory buffer\n", __FUNCTION__);
            return UP_ERR_PARSE;
        }
        if (extract_s3_url_memory(mb->buf, out_url, out_url_sz) != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed parsing S3 URL from memory\n", __FUNCTION__);
            return UP_ERR_PARSE;
        }
    } else {
        if (extract_s3_url_file(META_RESP_FILE, out_url, out_url_sz) != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed parsing S3 URL from file\n", __FUNCTION__);
            return UP_ERR_PARSE;
        }
    }
    return UP_OK;
}

/* ---------------------------------------------------------------------------
 * Public (legacy) function retained: doHttpFileUpload
 * (Delegates to perform_metadata_upload with file capture)
 * ---------------------------------------------------------------------------*/
int doHttpFileUpload(void *in_curl,
                     FileUpload_t *pfile_upload,
                     MtlsAuth_t *auth,
                     long *out_httpCode)
{
    if (out_httpCode) *out_httpCode = 0;
    if (!in_curl || !pfile_upload || !out_httpCode ||
        !pfile_upload->pathname || !pfile_upload->url) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Parameter validation failed\n", __FUNCTION__);
        return (int)UPLOAD_FAIL;
    }

    CURL *curl = (CURL *)in_curl;
    UploadStatus st = perform_metadata_upload(curl,
                                              pfile_upload,
                                              auth,
                                              out_httpCode,
                                              0,      /* use_memory_capture */
                                              NULL);  /* mem buffer */

    return (st == UP_OK) ? 0 : (int)st;
}

/* ---------------------------------------------------------------------------
 * Main orchestrator: runFileUpload
 * ---------------------------------------------------------------------------*/
int runFileUpload(const char *upload_url, const char *src_file)
{
    if (!upload_url || !src_file) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Invalid arguments\n", __FUNCTION__);
        return -1;
    }

    FileUpload_t fu;
    memset(&fu, 0, sizeof(fu));

    char urlbuf[URL_MAX];
    char pathbuf[PATHNAME_MAX];
    strncpy(urlbuf, upload_url, URL_MAX - 1);
    urlbuf[URL_MAX - 1] = '\0';
    strncpy(pathbuf, src_file, PATHNAME_MAX - 1);
    pathbuf[PATHNAME_MAX - 1] = '\0';

    fu.url         = urlbuf;
    fu.pathname    = pathbuf;
    fu.pPostFields = NULL;
    fu.sslverify   = 1;
    fu.hashData    = NULL;

    long http_code = 0;
    int curl_ret_code = -1;
    int overall_status = -1;

#ifdef LIBRDKCERTSELECTOR
    MtlsAuth_t sec;
    MtlsAuthStatus mtls_status = MTLS_CERT_FETCH_SUCCESS;
    static rdkcertselector_h thisCertSel = NULL;
    memset(&sec, 0, sizeof(sec));

    void *curl = doCurlInit();
    if (!curl) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: CURL init failed\n", __FUNCTION__);
        return -1;
    }

    if (thisCertSel == NULL) {
        const char *certGroup = "MTLS";
        thisCertSel = rdkcertselector_new(NULL, NULL, certGroup);
        if (!thisCertSel) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Cert selector init failed\n", __FUNCTION__);
            doStopDownload(curl);
            return -1;
        }
    }

    /* Retry loop based on selector feedback */
    do {
        mtls_status = loguploadGetCert(&sec, &thisCertSel);
        if (mtls_status == MTLS_CERT_FETCH_FAILURE) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: mTLS cert fetch failure\n", __FUNCTION__);
            curl_ret_code = (int)UP_ERR_MTLS;
            break;
        }

        curl_ret_code = doHttpFileUpload(curl, &fu, &sec, &http_code);
        if (curl_ret_code == 0 && http_code >= 200 && http_code < 300) {
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Metadata upload success\n", __FUNCTION__);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Metadata upload failed curl=%d http=%ld\n",
                    __FUNCTION__, curl_ret_code, http_code);
        }

    } while (rdkcertselector_setCurlStatus(thisCertSel, curl_ret_code, fu.url) == TRY_ANOTHER);

    if (!(curl_ret_code == 0 && http_code >= 200 && http_code < 300)) {
        overall_status = -1;
    } else {
        overall_status = 0;
    }

    doStopDownload(curl);
    curl = NULL;
    rdkcertselector_free(&thisCertSel);

#else
    void *curl = doCurlInit();
    if (!curl) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: CURL init failed\n", __FUNCTION__);
        return -1;
    }

    curl_ret_code = doHttpFileUpload(curl, &fu, NULL, &http_code);
    if (curl_ret_code == 0 && http_code >= 200 && http_code < 300) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Metadata upload success\n", __FUNCTION__);
        overall_status = 0;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Metadata upload failed curl=%d http=%ld\n",
                __FUNCTION__, curl_ret_code, http_code);
        overall_status = -1;
    }

    doStopDownload(curl);
    curl = NULL;
#endif

    /* Second stage: S3 PUT only if metadata succeeded */
    if (curl_ret_code == 0 && http_code >= 200 && http_code < 300) {
        char s3_url[S3_URL_BUF];

        if (extract_s3_url_file(META_RESP_FILE, s3_url, sizeof(s3_url)) == 0) {
#ifdef LIBRDKCERTSELECTOR
            if (perform_s3_put(s3_url, src_file, &sec) == UP_OK) {
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Full upload complete\n", __FUNCTION__);
                return 0;
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: S3 PUT failed\n", __FUNCTION__);
                return -1;
            }
#else
            if (perform_s3_put(s3_url, src_file, NULL) == UP_OK) {
                RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Full upload complete\n", __FUNCTION__);
                return 0;
            } else {
                RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: S3 PUT failed\n", __FUNCTION__);
                return -1;
            }
#endif
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to extract S3 URL\n", __FUNCTION__);
            return -1;
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Skipping S3 PUT due to metadata failure\n", __FUNCTION__);
        return -1;
    }

    return overall_status;
}
