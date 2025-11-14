#include "context.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "rdk_logger.h"     /* from rdk_logger repo */
#include "rdk_logger_types.h"

/* Wrapper logging helpers */
static inline void ctx_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rdk_logger_msg_vsprintf(RDK_LOG_INFO, DCM_LOG_MODULE, fmt, args);
    va_end(args);
}
static inline void ctx_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rdk_logger_msg_vsprintf(RDK_LOG_ERROR, DCM_LOG_MODULE, fmt, args);
    va_end(args);
}
static inline void ctx_log_notice(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rdk_logger_msg_vsprintf(RDK_LOG_NOTICE, DCM_LOG_MODULE, fmt, args);
    va_end(args);
}
static inline void ctx_log_debug(const char *fmt, ...) {
    if (!rdk_logger_is_logLevel_enabled(DCM_LOG_MODULE, RDK_LOG_DEBUG))
        return;
    va_list args;
    va_start(args, fmt);
    rdk_logger_msg_vsprintf(RDK_LOG_DEBUG, DCM_LOG_MODULE, fmt, args);
    va_end(args);
}

/* Existing helpers (trimmed for brevity) ... */

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
    }
}

static void load_properties(Context *ctx) {
    parse_key_value_file("/etc/include.properties", property_handler, ctx);
    parse_key_value_file("/etc/device.properties", property_handler, ctx);
    ctx_log_debug("Loaded properties: LOG_PATH=%s DEVICE_TYPE=%s BUILD_TYPE=%s",
                  ctx->log_path, ctx->device_type, ctx->build_type);
}

static void load_firmware_version(Context *ctx) {
    FILE *f = fopen("/version.txt", "r");
    if (!f) {
        strcpy(ctx->firmware_version, "unknown");
        ctx_log_notice("Firmware version file missing, defaulting to unknown");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "imagename:", 10) == 0) {
            char *val = line + 10;
            while (*val == ' ' || *val == '\t') val++;
            val[strcspn(val, "\r\n")] = 0;
            strncpy(ctx->firmware_version, val, sizeof(ctx->firmware_version)-1);
            break;
        }
    }
    fclose(f);
    if (ctx->firmware_version[0] == '\0') {
        strcpy(ctx->firmware_version, "unknown");
    }
    ctx_log_debug("Firmware version: %s", ctx->firmware_version);
}

static void load_host_ip(Context *ctx) {
    /* Placeholder */
    strcpy(ctx->host_ip, "0.0.0.0");
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
    ctx_log_debug("MAC raw=%s compact=%s", ctx->mac_raw, ctx->mac_compact);
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
        ctx_log_notice("DCMSettings.conf missing, default upload_flag=true");
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
    ctx_log_debug("upload_flag=%d", ctx->upload_flag);
}

static void load_privacy_mode(Context *ctx) {
    ctx->privacy_block = false;
}

bool context_init(Context *ctx) {
    /* Initialize RDK Logger first (only once). */
    if (rdk_logger_init("/etc/debug.ini") != RDK_SUCCESS) {
        fprintf(stderr, "RDK Logger init failed\n");
        return false;
    }
    ctx_log_notice("RDK Logger initialized for module %s", DCM_LOG_MODULE);

    zero_context(ctx);
    load_properties(ctx);

    if (ctx->log_path[0] == '\0')
        strncpy(ctx->log_path, "/opt/logs", sizeof(ctx->log_path)-1);

    snprintf(ctx->dcm_log_path, sizeof(ctx->dcm_log_path), "%s/dcmlogs", ctx->log_path);
    snprintf(ctx->prev_log_path, sizeof(ctx->prev_log_path), "%s/PreviousLogs", ctx->log_path);
    snprintf(ctx->prev_log_backup_path, sizeof(ctx->prev_log_backup_path), "%s/PreviousLogs_backup", ctx->log_path);
    snprintf(ctx->dcm_upload_list_path, sizeof(ctx->dcm_upload_list_path), "%s/dcm_upload", ctx->log_path);
    snprintf(ctx->dcm_script_log_path, sizeof(ctx->dcm_script_log_path), "%s/dcmscript.log", ctx->log_path);
    snprintf(ctx->tls_error_log_path, sizeof(ctx->tls_error_log_path), "%s/tlsError.log", ctx->log_path);
    snprintf(ctx->curl_info_path, sizeof(ctx->curl_info_path), "/tmp/logupload_curl_info");
    snprintf(ctx->http_code_path, sizeof(ctx->http_code_path), "/tmp/logupload_http_code");
    snprintf(ctx->previous_reboot_info_path, sizeof(ctx->previous_reboot_info_path), "/opt/secure/reboot/previousreboot.info");
    snprintf(ctx->direct_block_file, sizeof(ctx->direct_block_file), "/tmp/.lastdirectfail_upl");
    snprintf(ctx->codebig_block_file, sizeof(ctx->codebig_block_file), "/tmp/.lastcodebigfail_upl");
    snprintf(ctx->telemetry_path, sizeof(ctx->telemetry_path), "/opt/.telemetry");
    snprintf(ctx->rrd_log_dir, sizeof(ctx->rrd_log_dir), "/tmp/rrd/");
    snprintf(ctx->rrd_log_file, sizeof(ctx->rrd_log_file), "%s/remote-debugger.log", ctx->log_path);
    snprintf(ctx->iarm_event_bin_dir, sizeof(ctx->iarm_event_bin_dir),
             access("/etc/os-release", F_OK) == 0 ? "/usr/bin" : "/usr/local/bin");

    strncpy(ctx->tr181_unsched_reboot_disable,
            "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.UploadLogsOnUnscheduledReboot.Disable",
            sizeof(ctx->tr181_unsched_reboot_disable)-1);
    strncpy(ctx->rrd_tr181_name,
            "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RDKRemoteDebugger.IssueType",
            sizeof(ctx->rrd_tr181_name)-1);

    load_mac(ctx);
    load_host_ip(ctx);
    load_firmware_version(ctx);
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
    load_privacy_mode(ctx);

    ctx->log_upload_success_code = 0;
    ctx->log_upload_failed_code  = 1;
    ctx->log_upload_aborted_code = 2;

    ctx->use_codebig = (access(ctx->direct_block_file, F_OK) == 0) ? true : false;

    ctx_log_notice("Context initialized (log_path=%s, device_type=%s, mac=%s)",
                   ctx->log_path, ctx->device_type, ctx->mac_raw);

    return true;
}

int context_validate(Context *ctx, char *errmsg, size_t errmsg_sz) {
    if (ctx->mac_raw[0] == '\0' || strcmp(ctx->mac_raw, "00:00:00:00:00:00") == 0) {
        snprintf(errmsg, errmsg_sz, "MAC address default or empty");
        ctx_log_error("%s", errmsg);
        return 2;
    }
    if (ctx->log_path[0] == '\0') {
        snprintf(errmsg, errmsg_sz, "log_path not set");
        ctx_log_error("%s", errmsg);
        return 1;
    }
    if (strstr(ctx->log_file, "_Logs_") == NULL) {
        snprintf(errmsg, errmsg_sz, "log_file pattern invalid");
        ctx_log_error("%s", errmsg);
        return 3;
    }
    if (ctx->curl_tls_timeout <= 0 || ctx->curl_timeout <= 0 ||
        ctx->num_upload_attempts <= 0) {
        snprintf(errmsg, errmsg_sz, "timeouts or attempts invalid");
        ctx_log_error("%s", errmsg);
        return 5;
    }
    /* Ensure directories exist */
    const char *dirs[] = {
        ctx->log_path,
        ctx->dcm_log_path,
        ctx->prev_log_path,
        ctx->prev_log_backup_path,
        ctx->telemetry_path,
        ctx->rrd_log_dir
    };
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); ++i) {
        if (dirs[i][0] == '\0') continue;
        struct stat st;
        if (stat(dirs[i], &st) != 0) {
            if (mkdir(dirs[i], 0755) != 0) {
                snprintf(errmsg, errmsg_sz, "Failed mkdir %s errno=%d", dirs[i], errno);
                ctx_log_error("%s", errmsg);
                return 4;
            } else {
                ctx_log_notice("Created missing directory %s", dirs[i]);
            }
        } else if (!S_ISDIR(st.st_mode)) {
            snprintf(errmsg, errmsg_sz, "Path not a directory: %s", dirs[i]);
            ctx_log_error("%s", errmsg);
            return 4;
        }
    }
    if (strcmp(ctx->firmware_version, "unknown") == 0) {
        ctx_log_notice("Firmware version unknown (non-fatal)");
    }
    if (errmsg && errmsg_sz) {
        snprintf(errmsg, errmsg_sz, "OK");
    }
    ctx_log_info("Validation successful");
    return 0;
}

void context_deinit(Context *ctx) {
    (void)ctx;
    /* Let application call rdk_logger_deinit() at shutdown */
}
