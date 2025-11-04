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
