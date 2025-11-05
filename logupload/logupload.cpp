#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <sys/file.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <curl/curl.h>
#include <cstring>
#include <errno.h>
#include "rdk_debug.h"
#define DEBUG_INI_FILE "/etc/debug.ini"
#define LOG_LOGUPLOAD "LOG.RDK.LOGUPLOAD"

#ifdef LIBRDKCONFIG_BUILD
#include "rdkconfig.h"
#endif

#ifndef MTLS_FAILURE
#define MTLS_FAILURE -1
#endif

#ifdef LIBRDKCERTSELECTOR
#include "rdkcertselector.h"
// Below macro is invoked if the MTLS certificates retrieved by getMtlscert have failed
#define CURL_MTLS_LOCAL_CERTPROBLEM 58

typedef enum {
    MTLS_CERT_FETCH_FAILURE = -1,          // Indicates general MTLS failure
    MTLS_CERT_FETCH_SUCCESS = 0            // Indicates success
} MtlsAuthStatus;

MtlsAuthStatus rdmDwnlGetCert(MtlsAuth_t *sec, rdkcertselector_h* pthisCertSel);
#endif


// Helper: timestamp string
std::string currentTimestamp(const char* format = "%m-%d-%y-%I-%M%p") {
    char buf[64];
    time_t now = time(nullptr);
    strftime(buf, sizeof(buf), format, localtime(&now));
    return std::string(buf);
}

// Helper: run shell command and return output
std::string runCommand(const std::string& cmd) {
    std::string data;
    FILE* stream = popen(cmd.c_str(), "r");
    if (stream) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), stream) != nullptr)
            data.append(buffer);
        pclose(stream);
    }
    return data;
}

// Example: Get MAC address (replace with device-specific method)
std::string getMacAddressOnly() {
    // Replace with actual device call
    return runCommand("cat /sys/class/net/eth0/address | tr -d '\n'");
}

// Example: Get IP address (replace with device-specific method)
std::string getIPAddress() {
    // Replace with actual device call
    return runCommand("hostname -I | awk '{print $1}'");
}

// Logging functions
void uploadLog(const std::string& msg, const std::string& logFile) {
    std::ofstream ofs(logFile, std::ios_base::app);
    ofs << currentTimestamp("[%Y-%m-%d %H:%M:%S]") << " : " << msg << std::endl;
}

// Flock locking
bool acquireLock(const std::string& lockfile) {
    int fd = open(lockfile.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) return false;
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        close(fd);
        return false;
    }
    // Keep fd open and process running for the duration
    return true;
}


bool httpUpload(const std::string& url, const std::string& filename, std::string& errorMsg) {
    std::string FUNCTION = "httpUpload";
    //CURL* curl = curl_easy_init();
#ifdef LIBRDKCERTSELECTOR
    int cert_ret_code = -1;
    MtlsAuthStatus ret = MTLS_CERT_FETCH_SUCCESS;
#else
    int ret = MTLS_FAILURE;
#endif
    //if (!curl) {
    //    errorMsg = "Failed to init curl";
    //    return false;
    //}
    //curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        errorMsg = "Cannot open file for upload: " + filename;
      //  curl_easy_cleanup(curl);
        return false;
    }
    //curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    //curl_easy_setopt(curl, CURLOPT_READDATA, file);
    //CURLcode res = curl_easy_perform(curl);



    #ifdef LIBRDKCERTSELECTOR
        curl = doCurlInit();
        if(curl == NULL) {
            //RDMError("Failed init curl\n");
            return status;
        }
        static rdkcertselector_h thisCertSel = NULL;
        uploadLog(" cerselector starting ", FUNCTION);
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
            curl_ret_code = doHttpFileUpload(curl, &filename, &sec, &httpCode);
            //RDMInfo("curl_ret_code:%d httpCode:%d\n", curl_ret_code, httpCode);
            uploadLog(" cerselector running ", FUNCTION);
        }while (rdkcertselector_setCurlStatus(thisCertSel, curl_ret_code, file_dwnl.url) == TRY_ANOTHER);

        if(curl_ret_code && httpCode != 200) {
                //RDMError("Download failed\n");
                status = 1;
        }
        return status;
#endif
    fclose(file);
    /*curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        errorMsg = curl_easy_strerror(res);
        return false;
    }*/
    //return true;
}
/*
// Example: HTTP Upload using libcurl
bool httpUpload(const std::string& url, const std::string& filename, std::string& errorMsg) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        errorMsg = "Failed to init curl";
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        errorMsg = "Cannot open file for upload: " + filename;
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    CURLcode res = curl_easy_perform(curl);
    fclose(file);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        errorMsg = curl_easy_strerror(res);
        return false;
    }
    return true;
}
*/


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



// Main processing
int main(int argc, char* argv[]) {

    rdk_logger_init(DEBUG_INI_FILE);

    if (rdk_logger_init(DEBUG_INI_FILE) != RDK_SUCCESS) {
        fprintf(stderr, "Failed to initialize RDK Logger\n");
        return -1;
    }
        // Assign Input Arguments
    if (argc < 9) {
        RDK_LOG(RDK_LOG_ERROR, LOG_LOGUPLOAD, "[%s:%d]: Memory Allocation Failed.\n", __FUNCTION__, __LINE__);
            std::cerr << "USAGE: " << argv[0] << " <TFTP Server IP> <Flag> <SCP_SERVER> <UploadOnReboot> <UploadProtocol> <UploadHttpLink> <TriggerType> <RRD_FLAG> <RRD_UPLOADLOG_FILE>" << std::endl;
        return 1;
    }
    std::string TFTP_SERVER = argv[1];
    int FLAG = atoi(argv[2]);
    int DCM_FLAG = atoi(argv[3]);
    int UploadOnReboot = atoi(argv[4]);
    std::string UploadProtocol = argv[5];
    std::string UploadHttpLink = argv[6];
    int TriggerType = atoi(argv[7]);
    int RRD_FLAG = atoi(argv[8]);
    std::string RRD_UPLOADLOG_FILE = argc > 9 ? argv[9] : "";

    // Initialize Variables
    std::string MAC = getMacAddressOnly();
    std::string HOST_IP = getIPAddress();
    std::string DT = currentTimestamp();
    std::string LOG_FILE = MAC + "_Logs_" + DT + ".tgz";
    std::string DCM_LOG_FILE = "/opt/logs/dcmscript.log";
    std::string LOCKFILE = "/tmp/.log-upload.lock";
    std::string LOG_PATH = "/opt/logs"; // Set appropriately
    std::string DCM_LOG_PATH = LOG_PATH + "/dcm";

    if (!acquireLock(LOCKFILE)) {
        uploadLog("Another instance is running (flock lock held). Exiting.", DCM_LOG_FILE);
        return 1;
    }

    // Main logic flow (simplified)
    if (RRD_FLAG == 1) {
        uploadLog("Uploading RRD Debug Logs " + RRD_UPLOADLOG_FILE + " to S3 SERVER", DCM_LOG_FILE);
        if (UploadProtocol == "HTTP") {
            std::string errorMsg;
            uploadLog(" calling httpUpload", DCM_LOG_FILE);
            if (httpUpload(UploadHttpLink, RRD_UPLOADLOG_FILE, errorMsg)) {
                uploadLog("Uploading Logs through HTTP Success...", DCM_LOG_FILE);
                return 0;
            } else {
                uploadLog("Uploading Logs through HTTP Failed: " + errorMsg, DCM_LOG_FILE);
                return 127;
            }
        } else {
            uploadLog("UploadProtocol is not HTTP", DCM_LOG_FILE);
            return 127;
        }
    }

    // More logic here: DCM_FLAG, FLAG, UploadOnReboot, etc.
    // For each branch, mimic logic from the shell script:
    // - handle log file management (tar, copying, cleaning)
    // - perform uploads (HTTP, fallback, retries)
    // - handle privacy modes, device checks, etc.

    // Example: Upload logs via HTTP if protocol matches
    if (UploadProtocol == "HTTP") {
        std::string errorMsg;
        if (httpUpload(UploadHttpLink, LOG_FILE, errorMsg)) {
            uploadLog("LogUpload is successful", DCM_LOG_FILE);
        } else {
            uploadLog("Failed Uploading Logs through HTTP: " + errorMsg, DCM_LOG_FILE);
        }
    } else {
        uploadLog("UploadProtocol is not HTTP", DCM_LOG_FILE);
    }

    // Clean up, remove logs, etc. as needed

    // Release lock by exiting
    return 0;
}
