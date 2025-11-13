#include "downloadUtil.h"
//#include "mtlsUtils.h"
#include "rdkv_cdl_log_wrapper.h"

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

// Example: Test doHttpFileUpload
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
        curl, &file_upload, auth, 0 /* no throttle */, &http_code);

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
