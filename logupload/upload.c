#include "downloadUtil.h"
//#include "mtlsUtils.h"
#include "rdkv_cdl_log_wrapper.h"


#ifdef LIBRDKCERTSELECTOR
#include "rdkcertselector.h"
// Below macro is invoked if the MTLS certificates retrieved by getMtlscert have failed
#define CURL_MTLS_LOCAL_CERTPROBLEM 58

typedef enum {
    MTLS_CERT_FETCH_FAILURE = -1,          // Indicates general MTLS failure
    MTLS_CERT_FETCH_SUCCESS = 0            // Indicates success
} MtlsAuthStatus;

MtlsAuthStatus loguploadGetCert(MtlsAuth_t *sec, rdkcertselector_h* pthisCertSel);
#endif

#ifdef LIBRDKCONFIG_BUILD
#include "rdkconfig.h"
#endif


#ifdef LIBRDKCERTSELECTOR
#define FILESCHEME "file://"

/* Description: Use for get all mtls related certificate and key.
 * @param sec: This is a pointer hold the certificate, key and type of certificate.
 * @return : MTLS_CERT_FETCH_SUCCESS on success, MTLS_CERT_FETCH_FAILURE on mtls cert failure , STATE_RED_CERT_FETCH_FAILURE on state red cert failure
 * */
MtlsAuthStatus loguploadGetCert(MtlsAuth_t *sec, rdkcertselector_h* pthisCertSel) {

    char *certUri = NULL;
    char *certPass = NULL;
    char *engine = NULL;
    char *certFile = NULL;

    rdkcertselectorStatus_t certStat = rdkcertselector_getCert(*pthisCertSel, &certUri, &certPass);

    if (certStat != certselectorOk || certUri == NULL || certPass == NULL) {
        //RDMInfo("%s, Failed to retrieve certificate for MTLS\n",  __FUNCTION__);
        rdkcertselector_free(pthisCertSel);
        if(*pthisCertSel == NULL){
            //RDMInfo("%s, Cert selector memory free\n", __FUNCTION__);
        }else{
            //RDMInfo("%s, Cert selector memory free failed\n", __FUNCTION__);
        }
        return MTLS_CERT_FETCH_FAILURE; // Return error
    }

    certFile = certUri;
    if (strncmp(certFile, FILESCHEME, sizeof(FILESCHEME)-1) == 0) {
        certFile += (sizeof(FILESCHEME)-1); // Remove file scheme prefix
    }

    strncpy(sec->cert_name, certFile, sizeof(sec->cert_name) - 1);
    sec->cert_name[sizeof(sec->cert_name) - 1] = '\0';

    strncpy(sec->key_pas, certPass, sizeof(sec->key_pas) - 1);
    sec->key_pas[sizeof(sec->key_pas) - 1] = '\0';

    engine = rdkcertselector_getEngine(*pthisCertSel);
    if (engine == NULL) {
        sec->engine[0] = '\0';
    }else{
        strncpy(sec->engine, engine, sizeof(sec->engine) - 1);
        sec->engine[sizeof(sec->engine) - 1] = '\0';
    }

    strncpy(sec->cert_type, "P12", sizeof(sec->cert_type) - 1);
    sec->cert_type[sizeof(sec->cert_type) - 1] = '\0';

    //RDMInfo("%s, MTLS dynamic/static cert success. cert=%s, type=%s, engine=%s\n", __FUNCTION__, sec->cert_name, sec->cert_type, sec->engine);
    return MTLS_CERT_FETCH_SUCCESS; // Return success
}
#endif



int doHttpFileUpload(void *in_curl, FileDwnl_t *pfile_upload, MtlsAuth_t *auth, unsigned int max_upload_speed, int *out_httpCode)
{
    CURL *curl;
    CURLcode ret_code = CURLE_OK;
    CURLcode curl_status = -1;
    FILE *fp = NULL;
    struct curl_slist *headers = NULL;

    //DbgData_t verbosinfo;
    //memset(&verbosinfo, '\0', sizeof(DbgData_t));


    if (in_curl == NULL || pfile_upload == NULL || out_httpCode == NULL || pfile_upload->pathname == NULL) {
        COMMONUTILITIES_ERROR("%s: Parameter Check Fail\n", __FUNCTION__);
        return DWNL_FAIL;
    }
    curl = (CURL *)in_curl;

    // Open the file to be uploaded
    fp = fopen(pfile_upload->pathname, "rb");
    if (!fp) {
        //COMMONUTILITIES_ERROR("%s: Cannot open file: %s\n", __FUNCTION__, pfile_upload->pathname);
        return DWNL_FAIL;
    }

    // Set common curl options
    ret_code = setCommonCurlOpt(curl, pfile_upload->url, pfile_upload->pPostFields, pfile_upload->sslverify);
    if (ret_code != CURLE_OK) {
        //COMMONUTILITIES_ERROR("%s : CURL: setCommonCurlOpt Failed\n", __FUNCTION__);
        fclose(fp);
        return DWNL_FAIL;
    }

    // Set MTLS headers if required
    if(auth != NULL) {
        ret_code = setMtlsHeaders(curl, auth);
        if (ret_code != CURLE_OK) {
            //COMMONUTILITIES_ERROR("%s : CURL: setMtlsHeaders Failed\n", __FUNCTION__);
            fclose(fp);
            return DWNL_FAIL;
        }
    }

    // Add additional headers if needed
    if (pfile_upload->hashData != NULL) {
        headers = curl_slist_append(headers, pfile_upload->hashData->hashvalue);
        headers = curl_slist_append(headers, pfile_upload->hashData->hashtime);
        ret_code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        if (ret_code != CURLE_OK) {
            //COMMONUTILITIES_ERROR("SetRequestHeaders: CURLOPT_HTTPHEADER failed:%s\n", curl_easy_strerror(ret_code));
            curl_slist_free_all(headers);
            fclose(fp);
            return DWNL_FAIL;
        }
    }

    // Set upload-specific options
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);

    // Set file size if known (optional but recommended)
    fseek(fp, 0L, SEEK_END);
    curl_off_t filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, filesize);

    // Throttle upload speed if requested
    if (max_upload_speed > 0) {
        ret_code = setThrottleMode(curl, (curl_off_t) max_upload_speed);
        if (ret_code != CURLE_OK) {
            //COMMONUTILITIES_ERROR("%s : CURL: setThrottleMode Failed\n", __FUNCTION__);
            fclose(fp);
            if(headers) curl_slist_free_all(headers);
            return DWNL_FAIL;
        }
    }

    //ret_code = setCurlDebugOpt(curl, &verbosinfo);

    // Perform the upload
    ret_code = curl_easy_perform(curl);
    curl_status = ret_code;

    // Get HTTP response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, out_httpCode);

    //COMMONUTILITIES_INFO("%s : After curl upload ret status=%d and http code=%d\n", __FUNCTION__, curl_status, *out_httpCode);

    // Cleanup
    fclose(fp);
    if (headers)
        curl_slist_free_all(headers);
/*
    if (verbosinfo.verboslog) {
        fflush(verbosinfo.verboslog);
        fclose(verbosinfo.verboslog);
    }
*/
    return (int)curl_status;
}
#define URL_MAX 512
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <upload_url>\n", argv[0]);
        return 1;
    }

    void *curl;
    FileDwnl_t file_upload;
    int http_code = 0;
    MtlsAuth_t *auth = NULL;  // Or initialize if needed
    log_init();
    COMMONUTILITIES_ERROR("Parameter Check Fail\n");
    COMMONUTILITIES_ERROR("Parameter Check Fail\n");
    COMMONUTILITIES_ERROR("%s : device.property File not found\n", __FUNCTION__);
    COMMONUTILITIES_INFO("%s : CURL: free resources\n", __FUNCTION__);
    int status = 0;
    MtlsAuth_t sec;

    int curl_ret_code = -1;
#ifdef LIBRDKCERTSELECTOR
    int cert_ret_code = -1;
    MtlsAuthStatus ret = MTLS_CERT_FETCH_SUCCESS;
#else
    int ret = -1;
#endif

    memset(&file_upload, 0, sizeof(FileDwnl_t));
    // Set the upload destination URL
    //file_upload.url = "http://localhost:8080/upload/testfile.bin";
    // Path of the file to upload (must exist!)
    //file_upload.pathname = "testfile.bin";
    // For this example, you probably won't need post fields or special SSL


        // Safer than strcpy: limit copy and ensure null termination
    strncpy(file_upload.url, argv[1], URL_MAX - 1);
    file_upload.url[URL_MAX - 1] = '\0';

    printf("Using upload URL: %s\n", file_upload.url);

    //strcpy(file_upload.url, "http://localhost:8080/upload/file_to_upload.txt");
    strcpy(file_upload.pathname, "file_to_upload.txt");


    file_upload.pPostFields = NULL;
    file_upload.sslverify = true;
    file_upload.hashData = NULL;

    printf(" Reaching before certselector ");

#ifdef LIBRDKCERTSELECTOR
        printf(" Reaching certselector ");
        curl = doCurlInit();
        if(curl == NULL) {
            //RDMError("Failed init curl\n");
            return status;
        }
        static rdkcertselector_h thisCertSel = NULL;
        //RDMInfo("Initializing CertSelector\n");
        if (thisCertSel == NULL)
        {
            const char* certGroup = "MTLS";
            thisCertSel = rdkcertselector_new(NULL, NULL, certGroup);
            if (thisCertSel == NULL) {
                 //RDMInfo("Cert Selector Initialisation Failed\n");
                 return cert_ret_code;
            } else {
                //RDMInfo("Cert Selector Initialisation is Successful\n");
            }
        } else {
             //RDMInfo("Cert selector already initialized, reusing existing instance\n");
        }
        memset(&sec, '\0', sizeof(MtlsAuth_t));
        do {
            //RDMInfo("Fetching MTLS credential for SSR/XCONF\n");
            ret = loguploadGetCert(&sec, &thisCertSel);
            //RDMInfo("rdmDwnlGetCert function ret value = %d\n", ret);

            if (ret == MTLS_CERT_FETCH_FAILURE) {
                //RDMInfo("MTLS cert Failed ret= %d\n", ret);
                return cert_ret_code;
            } else {
                //RDMInfo("MTLS is enable\nMTLS creds for SSR fetched ret=%d\n", ret);
            }

            //RDMInfo("Downloading The Package %s \n",file_dwnl.pathname);
            int result = doHttpFileUpload(curl, &file_upload, &sec, 0 , &http_code);

            // 3. Check results
            if (result == 0 && http_code >= 200 && http_code < 300) {
                printf("Upload succeeded! HTTP Code: %d\n", http_code);
            } else {
                printf("Upload failed: curl result=%d, HTTP code=%d\n", result, http_code);
            }
            //RDMInfo("curl_ret_code:%d httpCode:%d\n", curl_ret_code, httpCode);
        }while (rdkcertselector_setCurlStatus(thisCertSel, curl_ret_code, file_upload.url) == TRY_ANOTHER);

        if(curl_ret_code && http_code != 200) {
                //RDMError("Download failed\n");
                status = -1;
        }
        if (curl != NULL) {
            //RDMInfo("Stopping Curl Download\n");
            doStopDownload(curl);
            rdkcertselector_free(thisCertSel);
            curl = NULL;
        }
        return status;
#endif

}

// Example: Test doHttpFileUpload
/*
int main() {
    void *curl;
    FileDwnl_t file_upload;
    int http_code = 0;
    MtlsAuth_t *auth = NULL;  // Or initialize if needed
    log_init();
    COMMONUTILITIES_ERROR("Parameter Check Fail\n");
    COMMONUTILITIES_ERROR("Parameter Check Fail\n");
    COMMONUTILITIES_ERROR("%s : device.property File not found\n", __FUNCTION__);
    COMMONUTILITIES_INFO("%s : CURL: free resources\n", __FUNCTION__);
    memset(&file_upload, 0, sizeof(FileDwnl_t));
    // Set the upload destination URL
    //file_upload.url = "http://localhost:8080/upload/testfile.bin";
    // Path of the file to upload (must exist!)
    //file_upload.pathname = "testfile.bin";
    // For this example, you probably won't need post fields or special SSL
    strcpy(file_upload.url, "http://localhost:8080/upload/file_to_upload.txt");
    strcpy(file_upload.pathname, "file_to_upload.txt");


    file_upload.pPostFields = NULL;
    file_upload.sslverify = true;
    file_upload.hashData = NULL;

    // 1. Init CURL handle
    curl = doCurlInit();
    if (!curl) {
        printf("CURL init failed\n");
        return 1;
    }

    // 2. Perform the upload
    int result = doHttpFileUpload(
        curl, &file_upload, auth, 0 , &http_code);

    // 3. Check results
    if (result == 0 && http_code >= 200 && http_code < 300) {
        printf("Upload succeeded! HTTP Code: %d\n", http_code);
    } else {
        printf("Upload failed: curl result=%d, HTTP code=%d\n", result, http_code);
    }

    // 4. Cleanup
    doStopDownload(curl);
    return 0;
}

*/
