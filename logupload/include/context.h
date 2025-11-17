/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

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
    char device_type[MAX_STR_FIELD];
    char build_type[MAX_STR_FIELD];
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

    bool tr181_unsched_reboot_disable;
    char rrd_tr181_name[MAX_STR_FIELD];
    char rrd_issue_type[MAX_STR_FIELD];

    char cloud_url[MAX_PATH_LENGTH];
    char tls_option[32];

    int log_upload_success_code;
    int log_upload_failed_code;
    int log_upload_aborted_code;
} Context;

bool context_init(Context *ctx);
void context_deinit(Context *ctx);

#endif
