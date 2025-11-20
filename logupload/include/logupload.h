#ifndef LOGUPLOAD_H
#define LOGUPLOAD_H

#include <stdbool.h>
#include <stddef.h>

// Macros and constants (as in rfcMgr and common_utilities style)

#define LOGUPLOAD_MODULE                "LOG.RDK.LOGUPLOAD"
#define LOGUPLOAD_DEBUG_INI             "/etc/debug.ini"
#define LOGUPLOAD_DEFAULT_LOG_PATH      "/opt/logs"
#define LOGUPLOAD_MAX_PATH_LENGTH       256
#define LOGUPLOAD_MAX_STR_FIELD         128
#define LOGUPLOAD_MAX_STAMP_LENGTH      64
#define LOGUPLOAD_MAC_ADDR_LENGTH       32

#define LOGUPLOAD_LOGS_ARCHIVE_FMT      "%s_Logs_%s.tgz"
#define LOGUPLOAD_RRD_LOG_FILE_FMT      "%s/remote-debugger.log"

#define LOGUPLOAD_TR181_CLOUD_URL              "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL"
#define LOGUPLOAD_TR181_RRD_ISSUE_TYPE         "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RDKRemoteDebugger.IssueType"
#define LOGUPLOAD_TR181_REBOOT_DISABLE         "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UploadLogsOnUnscheduledReboot.Disable"
#define LOGUPLOAD_TR181_ENCRYPT_CLOUDUPLOAD    "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.EncryptCloudUpload.Enable"
#define LOGUPLOAD_DIRECT_BLOCK_FILE            "/tmp/.lastdirectfail_upl"
#define LOGUPLOAD_ENABLE_OCSP_STAPLING_FILE    "/tmp/.EnableOCSPStapling"
#define LOGUPLOAD_ENABLE_OCSPCA_FILE           "/tmp/.EnableOCSPCA"
#define LOGUPLOAD_OS_RELEASE                   "/etc/os-release"
#define LOGUPLOAD_TLS_OPTION                   "--tlsv1.2"

typedef enum {
    LOGUPLOAD_STATUS_OK = 0,
    LOGUPLOAD_STATUS_LOGGER_FAIL,
    LOGUPLOAD_STATUS_PROPERTIES_FAIL,
    LOGUPLOAD_STATUS_DEVICE_INFO_FAIL,
    LOGUPLOAD_STATUS_TR181_FAIL,
    LOGUPLOAD_STATUS_SETTINGS_FAIL
} logupload_status_t;

typedef struct {
    char mac_raw[LOGUPLOAD_MAC_ADDR_LENGTH];
    char mac_compact[LOGUPLOAD_MAC_ADDR_LENGTH];
} logupload_mac_t;

typedef struct {
    char dt_stamp[LOGUPLOAD_MAX_STAMP_LENGTH];
    char timestamp_long[LOGUPLOAD_MAX_STAMP_LENGTH];
    char time_value_prefix[LOGUPLOAD_MAX_STAMP_LENGTH];
} logupload_timestamps_t;

typedef struct {
    char log_path[LOGUPLOAD_MAX_PATH_LENGTH];
    char packaged_logs_file[LOGUPLOAD_MAX_PATH_LENGTH];
    char rrd_log_file[LOGUPLOAD_MAX_PATH_LENGTH];
} logupload_paths_t;

// API
logupload_status_t logupload_context_init(
    logupload_mac_t* mac,
    char* device_type, size_t device_type_sz,
    char* build_type,  size_t build_type_sz,
    logupload_paths_t* paths,
    logupload_timestamps_t* stamps,
    bool* upload_enabled,
    bool* use_codebig,
    bool* enable_ocsp_stapling,
    bool* enable_ocsp,
    bool* encryption_enabled_rfc,
    char* rrd_issue_type, size_t rrd_issue_type_sz,
    char* cloud_url, size_t cloud_url_sz
);

void logupload_context_deinit(void);

bool logupload_is_codebig_required(bool use_codebig);
const char* logupload_get_cloud_url(const char* cloud_url);

// TR-181 helpers -- implementation in logupload_tr181.c
bool tr181_get_string(const char* param, char* out, size_t out_sz);
bool tr181_get_bool(const char* param, bool* out);

#endif
