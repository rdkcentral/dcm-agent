#ifndef LOGUPLOAD_H
#define LOGUPLOAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Logger module tag */
#define LOGUPLOAD_MODULE "LOG.RDK.LOGUPLOAD"

/* Sizes */
#define MAX_PATH_LENGTH   256
#define MAX_STR_FIELD     128
#define MAX_STAMP_LENGTH  64
#define MAC_ADDR_LENGTH   32

/* TR-181 parameter names */
#define TR181_RRD_ISSUE_TYPE      "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RDKRemoteDebugger.IssueType"
#define TR181_CLOUD_URL           "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL"
#define TR181_REBOOT_DISABLE      "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UploadLogsOnUnscheduledReboot.Disable"
#define TR181_ENCRYPT_CLOUDUPLOAD "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.EncryptCloudUpload.Enable"

typedef enum
{
    LOGUPLOAD_STATUS_OK = 0,
    LOGUPLOAD_STATUS_LOGGER_FAIL,
    LOGUPLOAD_STATUS_PROPERTIES_FAIL,
    LOGUPLOAD_STATUS_DEVICE_INFO_FAIL,
    LOGUPLOAD_STATUS_TR181_FAIL,
    LOGUPLOAD_STATUS_SETTINGS_FAIL
} logupload_status_t;

/* Sub-structures logically segregated */
typedef struct
{
    char mac_raw[MAC_ADDR_LENGTH];
    char mac_compact[MAC_ADDR_LENGTH];
    char device_type[MAX_STR_FIELD];
    char build_type[MAX_STR_FIELD];
    char version_line[MAX_STR_FIELD];
} logupload_identity_t;

typedef struct
{
    char rdk_path[MAX_PATH_LENGTH];
    char log_path[MAX_PATH_LENGTH];
    char dcm_log_dir[MAX_PATH_LENGTH];
    char previous_logs_dir[MAX_PATH_LENGTH];
    char previous_logs_backup_dir[MAX_PATH_LENGTH];
    char dcm_upload_list_dir[MAX_PATH_LENGTH];
    char telemetry_dir[MAX_PATH_LENGTH];
    char rrd_log_dir[MAX_PATH_LENGTH];
    char iarm_event_bin_dir[MAX_PATH_LENGTH];
} logupload_dirs_t;

typedef struct
{
    char dcm_script_log_file[MAX_PATH_LENGTH];
    char tls_error_log_file[MAX_PATH_LENGTH];
    char curl_info_file[MAX_PATH_LENGTH];
    char http_code_file[MAX_PATH_LENGTH];
    char previous_reboot_info_file[MAX_PATH_LENGTH];
    char direct_fail_block_file[MAX_PATH_LENGTH];
    char codebig_fail_block_file[MAX_PATH_LENGTH];
    char rrd_log_file[MAX_PATH_LENGTH];
    char packaged_logs_file[MAX_PATH_LENGTH];
    char packaged_dri_logs_file[MAX_PATH_LENGTH];
} logupload_files_t;

typedef struct
{
    logupload_dirs_t  dirs;
    logupload_files_t files;
} logupload_paths_t;

typedef struct
{
    char dt_stamp[MAX_STAMP_LENGTH];
    char timestamp_long[MAX_STAMP_LENGTH];
    char time_value_prefix[MAX_STAMP_LENGTH];
} logupload_timestamps_t;

typedef struct
{
    bool enable_ocsp_stapling;
    bool enable_ocsp;
    bool encryption_enabled_rfc;
    bool upload_enabled;
    bool use_codebig;
    bool privacy_block;
    bool tr181_unsched_reboot_disable;
} logupload_flags_t;

typedef struct
{
    int  num_upload_attempts;
    int  cb_num_upload_attempts;
    int  curl_tls_timeout;
    int  curl_timeout;
    char tls_option[32];
} logupload_config_t;

typedef struct
{
    char rrd_issue_type[MAX_STR_FIELD];
    char cloud_url[MAX_PATH_LENGTH];
} logupload_tr181_values_t;

typedef struct
{
    int success_code;
    int failed_code;
    int aborted_code;
} logupload_results_t;

typedef struct
{
    logupload_identity_t      identity;
    logupload_paths_t         paths;
    logupload_timestamps_t    stamps;
    logupload_flags_t         flags;
    logupload_config_t        config;
    logupload_tr181_values_t  tr181;
    logupload_results_t       results;
} logupload_context_t;

/* Lifecycle */
logupload_status_t logupload_context_init(logupload_context_t* ctx);
void               logupload_context_deinit(logupload_context_t* ctx);

/* Queries */
bool        logupload_is_codebig_required(const logupload_context_t* ctx);
const char* logupload_get_cloud_url(const logupload_context_t* ctx);

/* TR-181 dedicated API (implemented in logupload_tr181.c) */
bool tr181_get_string(const char* param, char* out, size_t out_sz);
bool tr181_get_bool  (const char* param, bool* out);

#endif /* LOGUPLOAD_H */
