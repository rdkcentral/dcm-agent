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

#include "context.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "rdk_logger.h"
#include "rdk_logger_types.h"

/* RBUS includes for TR-181 parameter access */
#include <rbus/rbus.h>

#define RBUS_TR181_FETCH_OK 0
#define RRD_ISSUE_TYPE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RDKRemoteDebugger.IssueType"
#define CLOUD_URL "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.LogUploadEndpoint.URL"
#define REBOOT_DISABLE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UploadLogsOnUnscheduledReboot.Disable"
#define ENCRYPTCLOUDUPLOAD "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.EncryptCloudUpload.Enable"

/* Helper to fetch a TR-181 string value using RBUS */
static int rbus_get_tr181_param(const char* param, char* buf, size_t buflen, bool *field) {
    rbusHandle_t handle;
    rbusValue_t value = NULL;
    int ret = RBUS_TR181_FETCH_OK; // success unless error occurs

    if(rbus_open(&handle, "logupload") != RBUS_ERROR_SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "RBUS: Failed to open connection for param %s\n", param);
        strncpy(buf, "", buflen);
        printf("Rbus failed");
        return 1;
    }

    if(rbus_get(handle, param, &value) == RBUS_ERROR_SUCCESS && value) {
        rbusValueType_t type = rbusValue_GetType(value);
        if(type == RBUS_BOOLEAN) {
          bool data = rbusValue_GetBoolean(value);
          printf("Called get handler for [%s] & value is %s\n", param, data ? "true" : "false");
          if( data == true) {*field = true;}
          else {*field = false;}
        }
        else if(type == RBUS_STRING)
        {
        const char *valstr = rbusValue_GetString(value, NULL);
        if(valstr) {
            strncpy(buf, valstr, buflen-1);
            buf[buflen-1] = 0;
        } else {
            buf[0] = 0;
            ret = 2;
        }
        }
        rbusValue_Release(value);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "RBUS: Failed to get TR-181 param %s\n", param);
        buf[0] = 0;
        ret = 3;
    }
    rbus_close(handle);
    return ret;
}

/* Minimal helper to eliminate -Wformat-truncation warnings for path building */
static void build_path(char *dst, size_t dstsz, const char *base, const char *suffix)
{
    if (!dst || dstsz == 0) return;
    if (!base) base = "";
    if (!suffix) suffix = "";

    size_t base_len = strnlen(base, dstsz - 1);
    size_t suffix_len = strnlen(suffix, dstsz - 1);

    if (base_len + suffix_len >= dstsz) {
        if (suffix_len + 1 >= dstsz) {
            suffix_len = dstsz - 1;
            base_len = 0;
        } else {
            base_len = dstsz - suffix_len - 1;
        }
    }

    if (base_len)
        memcpy(dst, base, base_len);
    if (suffix_len)
        memcpy(dst + base_len, suffix, suffix_len);
    dst[base_len + suffix_len] = '\0';
}

static void zero_context(Context *ctx) {
    memset(ctx, 0, sizeof(*ctx));
}

static void sanitize_mac(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j < out_sz - 1; ++i) {
        if (in[i] != ':')
            out[j++] = in[i];
    }
    out[j] = '\0';
}

static void parse_key_value_file(const char *filepath,
                                 void (*handler)(const char*, const char*, void*),
                                 void *user_data) {
    FILE *f = fopen(filepath, "r");
    if (!f) return;
    char buf[512], kbuf[256], vbuf[256];
    while (fgets(buf, sizeof(buf), f)) {
        if (sscanf(buf, "%255[^=]=%255[^\n]", kbuf, vbuf) == 2) {
            char *v = vbuf;
            while (*v == ' ' || *v == '\t') v++;
            handler(kbuf, v, user_data);
        }
    }
    fclose(f);
}

static void property_handler(const char* key, const char* value, void* user_data) {
    Context *ctx = (Context *)user_data;
    if (strcmp(key, "RDK_PATH") == 0) {
        strncpy(ctx->rdk_path, value, sizeof(ctx->rdk_path)-1);
    } else if (strcmp(key, "LOG_PATH") == 0) {
        strncpy(ctx->log_path, value, sizeof(ctx->log_path)-1);
    } else if (strcmp(key, "DEVICE_TYPE") == 0) {
        strncpy(ctx->device_type, value, sizeof(ctx->device_type)-1);
    } else if (strcmp(key, "BUILD_TYPE") == 0) {
        strncpy(ctx->build_type, value, sizeof(ctx->build_type)-1);
    } else if (strcmp(key, "DIRECT_BLOCK_TIME") == 0) {
        ctx->directblocktime = atoi(value);
    } else if (strcmp(key, "CB_BLOCK_TIME") == 0) {
        ctx->cb_block_time = atoi(value);
    }
}

static void load_properties(Context *ctx) {
    parse_key_value_file("/etc/include.properties", property_handler, ctx);
    parse_key_value_file("/etc/device.properties", property_handler, ctx);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "Loaded properties: LOG_PATH=%s DEVICE_TYPE=%s BUILD_TYPE=%s \n",
                  ctx->log_path, ctx->device_type, ctx->build_type);
}

static void load_mac(Context *ctx) {
    FILE *fm = fopen("/sys/class/net/eth0/address", "r");
    if (fm) {
        fgets(ctx->mac_raw, sizeof(ctx->mac_raw), fm);
        ctx->mac_raw[strcspn(ctx->mac_raw, "\n")] = 0;
        fclose(fm);
    } else {
        strcpy(ctx->mac_raw, "00:00:00:00:00:00");
    }
    sanitize_mac(ctx->mac_raw, ctx->mac_compact, sizeof(ctx->mac_compact));
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "MAC raw=%s compact=%s \n", ctx->mac_raw, ctx->mac_compact);
}

static void generate_timestamps(Context *ctx) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(ctx->dt_stamp, sizeof(ctx->dt_stamp), "%m-%d-%y-%I-%M%p", tm_info);
    strftime(ctx->timestamp_long, sizeof(ctx->timestamp_long), "%Y-%m-%d-%H-%M-%S%p", tm_info);
    strncpy(ctx->time_value_prefix, ctx->dt_stamp, sizeof(ctx->time_value_prefix)-1);
}

static void load_upload_flag(Context *ctx) {
    FILE *f = fopen("/tmp/DCMSettings.conf", "r");
    if (!f) {
        ctx->upload_flag = true;
        RDK_LOG(RDK_LOG_NOTICE, LOG_UPLOAD, "DCMSettings.conf missing, default upload_flag=true \n");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "urn:settings:LogUploadSettings:upload=", 37) == 0) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            eq++;
            while (*eq == '"' || *eq == ' ' || *eq == '\t') eq++;
            char *end = eq + strcspn(eq, "\r\n");
            *end = 0;
            if (*eq == '"') {
                eq++;
                char *q = strchr(eq, '"');
                if (q) *q = 0;
            }
            ctx->upload_flag = (strcmp(eq, "false") != 0);
            break;
        }
    }
    fclose(f);
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "upload_flag=%d \n", ctx->upload_flag);
}


static void load_tr181_parameters(Context *ctx)
{

    /* Fetch TR-181 parameters using RBUS */

    if (rbus_get_tr181_param(RRD_ISSUE_TYPE, ctx->rrd_tr181_name, sizeof(ctx->rrd_tr181_name), false) == 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "TR181 [%s] = [%s]\n", RRD_ISSUE_TYPE, ctx->rrd_tr181_name);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "Failed to fetch TR-181 value for %s\n", CLOUD_URL);
    }
    if (rbus_get_tr181_param(CLOUD_URL, ctx->cloud_url, sizeof(ctx->cloud_url), false) == 0) {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "TR181 [%s] = [%s]\n", CLOUD_URL, ctx->rrd_tr181_name);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "Failed to fetch TR-181 value for %s\n", CLOUD_URL);
    }
    if (rbus_get_tr181_param(REBOOT_DISABLE, NULL, 0, &ctx->tr181_unsched_reboot_disable) == 0)
    {
        RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "TR181 [%s] = [%d]\n", REBOOT_DISABLE, ctx->tr181_unsched_reboot_disable);
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "Failed to fetch TR-181 value for %s\n", REBOOT_DISABLE);
    }
    /*
    if (access("/etc/os-release", F_OK) == 0) {
        if (rbus_get_tr181_param(ENCRYPTCLOUDUPLOAD,NULL, 0, &ctx->encryption_enabled_rfc) == 0)
        {
            RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOAD, "TR181 [%s] = [%d]\n", ENCRYPTCLOUDUPLOAD, ctx->encryption_enabled_rfc);
        } else {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "Failed to fetch TR-181 value for %s\n", ENCRYPTCLOUDUPLOAD);
        }
    }
    */
}

bool context_init(Context *ctx) {
    if (rdk_logger_init("/etc/debug.ini") != RDK_SUCCESS) {
        fprintf(stderr, "RDK Logger init failed\n");
        return false;
    }
    RDK_LOG(RDK_LOG_NOTICE, LOG_UPLOAD, "RDK Logger initialized for module %s \n", LOG_UPLOAD);

    zero_context(ctx);
    load_properties(ctx);

    if (ctx->log_path[0] == '\0')
        strncpy(ctx->log_path, "/opt/logs", sizeof(ctx->log_path)-1);

    build_path(ctx->dcm_log_path,         sizeof(ctx->dcm_log_path),         ctx->log_path, "/dcmlogs");
    build_path(ctx->prev_log_path,        sizeof(ctx->prev_log_path),        ctx->log_path, "/PreviousLogs");
    build_path(ctx->prev_log_backup_path, sizeof(ctx->prev_log_backup_path), ctx->log_path, "/PreviousLogs_backup");
    build_path(ctx->dcm_upload_list_path, sizeof(ctx->dcm_upload_list_path), ctx->log_path, "/dcm_upload");
    build_path(ctx->dcm_script_log_path,  sizeof(ctx->dcm_script_log_path),  ctx->log_path, "/dcmscript.log");
    build_path(ctx->tls_error_log_path,   sizeof(ctx->tls_error_log_path),   ctx->log_path, "/tlsError.log");
    build_path(ctx->rrd_log_file,         sizeof(ctx->rrd_log_file),         ctx->log_path, "/remote-debugger.log");

    snprintf(ctx->curl_info_path, sizeof(ctx->curl_info_path), "/tmp/logupload_curl_info");
    snprintf(ctx->http_code_path, sizeof(ctx->http_code_path), "/tmp/logupload_http_code");
    snprintf(ctx->previous_reboot_info_path, sizeof(ctx->previous_reboot_info_path), "/opt/secure/reboot/previousreboot.info");
    snprintf(ctx->direct_block_file, sizeof(ctx->direct_block_file), "/tmp/.lastdirectfail_upl");
    snprintf(ctx->codebig_block_file, sizeof(ctx->codebig_block_file), "/tmp/.lastcodebigfail_upl");
    snprintf(ctx->telemetry_path, sizeof(ctx->telemetry_path), "/opt/.telemetry");
    snprintf(ctx->rrd_log_dir, sizeof(ctx->rrd_log_dir), "/tmp/rrd/");
    snprintf(ctx->iarm_event_bin_dir, sizeof(ctx->iarm_event_bin_dir),
             access("/etc/os-release", F_OK) == 0 ? "/usr/bin" : "/usr/local/bin");

    load_mac(ctx);
    generate_timestamps(ctx);

    snprintf(ctx->log_file, sizeof(ctx->log_file),
             "%s_Logs_%s.tgz", ctx->mac_compact, ctx->dt_stamp);
    snprintf(ctx->dri_log_file, sizeof(ctx->dri_log_file),
             "%s_DRI_Logs_%s.tgz", ctx->mac_compact, ctx->dt_stamp);

    if (access("/etc/os-release", F_OK) == 0) {
        strcpy(ctx->tls_option, "--tlsv1.2");
    } else {
        ctx->tls_option[0] = '\0';
    }

    ctx->curl_tls_timeout = 30;
    ctx->curl_timeout     = 10;
    ctx->num_upload_attempts    = 3;
    ctx->cb_num_upload_attempts = 1;

    ctx->enable_ocsp_stapling = (access("/tmp/.EnableOCSPStapling", F_OK) == 0);
    ctx->enable_ocsp = (access("/tmp/.EnableOCSPCA", F_OK) == 0);
    ctx->encryption_enabled_rfc = false;

    load_upload_flag(ctx);

    ctx->log_upload_success_code = 0;
    ctx->log_upload_failed_code  = 1;
    ctx->log_upload_aborted_code = 2;

    ctx->use_codebig = (access(ctx->direct_block_file, F_OK) == 0) ? true : false;

    /* Fetch TR-181 parameters using rbus */
    load_tr181_parameters(ctx);

    RDK_LOG(RDK_LOG_NOTICE, LOG_UPLOAD, "Context initialized log_path=%s, device_type=%s, mac=%s \n",
                   ctx->log_path, ctx->device_type, ctx->mac_raw);

    return true;
}

void context_deinit(Context *ctx) {
    (void)ctx;
    /* Let application call rdk_logger_deinit() at shutdown */
}
