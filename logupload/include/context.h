#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_PATH_LENGTH 256
#define MAX_STR_FIELD   128
#define LOG_UPLOAD "LOG.RDK.LOGUPLOAD"

typedef struct {
    char mac_raw[MAX_STR_FIELD];
    char mac_compact[MAX_STR_FIELD];
    char host_ip[MAX_STR_FIELD];
    char device_type[MAX_STR_FIELD];
    char build_type[MAX_STR_FIELD];
    char firmware_version[MAX_STR_FIELD];
    char version_line[MAX_STR_FIELD];

    char rdk_path[MAX_PATH_LENGTH];
    char log_path[MAX_PATH_LENGTH];
    char dcm_log_path[MAX_PATH_LENGTH];
    char prev_log_path[MAX_PATH_LENGTH];
    char prev_log_backup_path[MAX_PATH_LENGTH];
    char dcm_upload_list_path[MAX_PATH_LENGTH];
    char telemetry_path[MAX_PATH_LENGTH];
    char dcm_script_log_path[MAX_PATH_LENGTH];
    char tls_error_log_path[MAX_PATH_LENGTH];
    char curl_info_path[MAX_PATH_LENGTH];
    char http_code_path[MAX_PATH_LENGTH];
    char previous_reboot_info_path[MAX_PATH_LENGTH];
    char direct_block_file[MAX_PATH_LENGTH];
    char codebig_block_file[MAX_PATH_LENGTH];
    char rrd_log_file[MAX_PATH_LENGTH];
    char rrd_log_dir[MAX_PATH_LENGTH];
    char iarm_event_bin_dir[MAX_PATH_LENGTH];

    char log_file[MAX_PATH_LENGTH];
    char dri_log_file[MAX_PATH_LENGTH];

    char dt_stamp[64];
    char timestamp_long[64];
    char time_value_prefix[64];

    bool enable_ocsp_stapling;
    bool enable_ocsp;
    bool encryption_enabled_rfc;
    bool upload_flag;
    bool use_codebig;
    bool privacy_block;
    int  num_upload_attempts;
    int  cb_num_upload_attempts;

    int curl_tls_timeout;
    int curl_timeout;

    char tr181_unsched_reboot_disable[MAX_STR_FIELD];
    char rrd_tr181_name[MAX_STR_FIELD];
    char rrd_issue_type[MAX_STR_FIELD];

    char cloud_url[MAX_PATH_LENGTH];
    char tls_option[32];

    int log_upload_success_code;
    int log_upload_failed_code;
    int log_upload_aborted_code;
} Context;

bool context_init(Context *ctx);
int context_validate(Context *ctx, char *errmsg, size_t errmsg_sz);
void context_deinit(Context *ctx);

#endif
