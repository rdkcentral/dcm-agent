#include "logupload.h"
#include "rdk_logger.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---------- Internal helpers (kept local to maintain 3-file constraint) ---------- */

static void context_zero(logupload_context_t* ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

static void join_path(char* dst, size_t dstsz, const char* base, const char* suffix)
{
    if(!dst || dstsz == 0) return;
    if(!base) base = "";
    if(!suffix) suffix = "";
    size_t bl = strnlen(base, dstsz - 1);
    size_t sl = strnlen(suffix, dstsz - 1);
    if(bl + sl >= dstsz)
    {
        if(sl + 1 >= dstsz) { sl = dstsz - 1; bl = 0; }
        else { bl = dstsz - sl - 1; }
    }
    if(bl) memcpy(dst, base, bl);
    if(sl) memcpy(dst + bl, suffix, sl);
    dst[bl + sl] = '\0';
}

static void load_properties(logupload_context_t* ctx)
{
    /* TODO: Replace with common_utilities APIs if available */
    FILE* f;
    char line[512], k[256], v[256];

    const char* files[] = { "/etc/include.properties", "/etc/device.properties" };
    for(size_t fi = 0; fi < sizeof(files)/sizeof(files[0]); ++fi)
    {
        f = fopen(files[fi], "r");
        if(!f) continue;
        while(fgets(line, sizeof(line), f))
        {
            if(sscanf(line, "%255[^=]=%255[^\n]", k, v) == 2)
            {
                char* val = v;
                while(*val == ' ' || *val == '\t') val++;
                if(strcmp(k, "RDK_PATH") == 0)
                    strncpy(ctx->paths.dirs.rdk_path, val, sizeof(ctx->paths.dirs.rdk_path)-1);
                else if(strcmp(k, "LOG_PATH") == 0)
                    strncpy(ctx->paths.dirs.log_path, val, sizeof(ctx->paths.dirs.log_path)-1);
                else if(strcmp(k, "DEVICE_TYPE") == 0)
                    strncpy(ctx->identity.device_type, val, sizeof(ctx->identity.device_type)-1);
                else if(strcmp(k, "BUILD_TYPE") == 0)
                    strncpy(ctx->identity.build_type, val, sizeof(ctx->identity.build_type)-1);
            }
        }
        fclose(f);
    }

    if(ctx->paths.dirs.log_path[0] == '\0')
        strncpy(ctx->paths.dirs.log_path, "/opt/logs", sizeof(ctx->paths.dirs.log_path)-1);

    RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
            "Properties: LOG_PATH=%s DEVICE_TYPE=%s BUILD_TYPE=%s\n",
            ctx->paths.dirs.log_path,
            ctx->identity.device_type,
            ctx->identity.build_type);
}

static bool load_mac(logupload_context_t* ctx)
{
    FILE* fm = fopen("/sys/class/net/eth0/address", "r");
    if(fm)
    {
        if(fgets(ctx->identity.mac_raw, sizeof(ctx->identity.mac_raw), fm) == NULL)
            ctx->identity.mac_raw[0] = '\0';
        fclose(fm);
        ctx->identity.mac_raw[strcspn(ctx->identity.mac_raw, "\n")] = '\0';
    }
    else
    {
        strncpy(ctx->identity.mac_raw, "00:00:00:00:00:00", sizeof(ctx->identity.mac_raw)-1);
    }

    size_t j=0;
    for(size_t i=0; ctx->identity.mac_raw[i] && j < sizeof(ctx->identity.mac_compact)-1; ++i)
    {
        if(ctx->identity.mac_raw[i] != ':')
            ctx->identity.mac_compact[j++] = ctx->identity.mac_raw[i];
    }
    ctx->identity.mac_compact[j] = '\0';

    RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE,
            "MAC raw=%s compact=%s\n",
            ctx->identity.mac_raw,
            ctx->identity.mac_compact);
    return true;
}

static void generate_timestamps(logupload_context_t* ctx)
{
    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    strftime(ctx->stamps.dt_stamp,       sizeof(ctx->stamps.dt_stamp),       "%m-%d-%y-%I-%M%p", &tm_info);
    strftime(ctx->stamps.timestamp_long, sizeof(ctx->stamps.timestamp_long), "%Y-%m-%d-%H-%M-%S%p", &tm_info);
    strncpy(ctx->stamps.time_value_prefix, ctx->stamps.dt_stamp, sizeof(ctx->stamps.time_value_prefix)-1);
    ctx->stamps.time_value_prefix[sizeof(ctx->stamps.time_value_prefix)-1] = '\0';
}

static void build_paths(logupload_context_t* ctx)
{
    const char* base = ctx->paths.dirs.log_path;
    join_path(ctx->paths.dirs.dcm_log_dir,              sizeof(ctx->paths.dirs.dcm_log_dir),              base, "/dcmlogs");
    join_path(ctx->paths.dirs.previous_logs_dir,        sizeof(ctx->paths.dirs.previous_logs_dir),        base, "/PreviousLogs");
    join_path(ctx->paths.dirs.previous_logs_backup_dir, sizeof(ctx->paths.dirs.previous_logs_backup_dir), base, "/PreviousLogs_backup");
    join_path(ctx->paths.dirs.dcm_upload_list_dir,      sizeof(ctx->paths.dirs.dcm_upload_list_dir),      base, "/dcm_upload");
    join_path(ctx->paths.files.dcm_script_log_file,     sizeof(ctx->paths.files.dcm_script_log_file),     base, "/dcmscript.log");
    join_path(ctx->paths.files.tls_error_log_file,      sizeof(ctx->paths.files.tls_error_log_file),      base, "/tlsError.log");
    join_path(ctx->paths.files.rrd_log_file,            sizeof(ctx->paths.files.rrd_log_file),            base, "/remote-debugger.log");

    snprintf(ctx->paths.files.curl_info_file,            sizeof(ctx->paths.files.curl_info_file),            "/tmp/logupload_curl_info");
    snprintf(ctx->paths.files.http_code_file,            sizeof(ctx->paths.files.http_code_file),            "/tmp/logupload_http_code");
    snprintf(ctx->paths.files.previous_reboot_info_file, sizeof(ctx->paths.files.previous_reboot_info_file), "/opt/secure/reboot/previousreboot.info");
    snprintf(ctx->paths.files.direct_fail_block_file,    sizeof(ctx->paths.files.direct_fail_block_file),    "/tmp/.lastdirectfail_upl");
    snprintf(ctx->paths.files.codebig_fail_block_file,   sizeof(ctx->paths.files.codebig_fail_block_file),   "/tmp/.lastcodebigfail_upl");
    snprintf(ctx->paths.dirs.telemetry_dir,              sizeof(ctx->paths.dirs.telemetry_dir),              "/opt/.telemetry");
    snprintf(ctx->paths.dirs.rrd_log_dir,                sizeof(ctx->paths.dirs.rrd_log_dir),                "/tmp/rrd/");
    snprintf(ctx->paths.dirs.iarm_event_bin_dir,         sizeof(ctx->paths.dirs.iarm_event_bin_dir),
             access("/etc/os-release", F_OK) == 0 ? "/usr/bin" : "/usr/local/bin");

    snprintf(ctx->paths.files.packaged_logs_file,     sizeof(ctx->paths.files.packaged_logs_file),
             "%s_Logs_%s.tgz", ctx->identity.mac_compact, ctx->stamps.dt_stamp);
    snprintf(ctx->paths.files.packaged_dri_logs_file, sizeof(ctx->paths.files.packaged_dri_logs_file),
             "%s_DRI_Logs_%s.tgz", ctx->identity.mac_compact, ctx->stamps.dt_stamp);
}

static void load_upload_setting(logupload_context_t* ctx)
{
    FILE* f = fopen("/tmp/DCMSettings.conf", "r");
    if(!f)
    {
        ctx->flags.upload_enabled = true;
        RDK_LOG(RDK_LOG_NOTICE, LOGUPLOAD_MODULE,
                "DCMSettings.conf missing; default upload_enabled=true\n");
        return;
    }
    char line[512];
    bool found = false;
    while(fgets(line, sizeof(line), f))
    {
        if(strncmp(line, "urn:settings:LogUploadSettings:upload=", 38) == 0)
        {
            char* eq = strchr(line, '=');
            if(!eq) continue;
            eq++;
            while(*eq == '"' || *eq == ' ' || *eq == '\t') eq++;
            char* end = eq + strcspn(eq, "\r\n");
            *end = '\0';
            if(*eq == '"')
            {
                eq++;
                char* q = strchr(eq, '"');
                if(q) *q = '\0';
            }
            if(strcmp(eq, "true") == 0) ctx->flags.upload_enabled = true;
            else if(strcmp(eq, "false") == 0) ctx->flags.upload_enabled = false;
            else ctx->flags.upload_enabled = true;
            found = true;
            break;
        }
    }
    fclose(f);
    if(!found) ctx->flags.upload_enabled = true;
    RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "upload_enabled=%d\n", ctx->flags.upload_enabled);
}

static void eval_dynamic_flags(logupload_context_t* ctx)
{
    ctx->flags.enable_ocsp_stapling = (access("/tmp/.EnableOCSPStapling", F_OK) == 0);
    ctx->flags.enable_ocsp         = (access("/tmp/.EnableOCSPCA", F_OK) == 0);
    ctx->flags.use_codebig         = (access(ctx->paths.files.direct_fail_block_file, F_OK) == 0);

    if(access("/etc/os-release", F_OK) == 0)
        strncpy(ctx->config.tls_option, "--tlsv1.2", sizeof(ctx->config.tls_option)-1);
    else
        ctx->config.tls_option[0] = '\0';

    RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE,
            "Flags: ocsp_stapling=%d ocsp=%d use_codebig=%d tls_option=%s\n",
            ctx->flags.enable_ocsp_stapling,
            ctx->flags.enable_ocsp,
            ctx->flags.use_codebig,
            ctx->config.tls_option);
}

/* ---------- Public API Implementation ---------- */

logupload_status_t logupload_context_init(logupload_context_t* ctx)
{
    if(!ctx) return LOGUPLOAD_STATUS_LOGGER_FAIL;

    if(rdk_logger_init("/etc/debug.ini") != RDK_SUCCESS)
    {
        fprintf(stderr, "RDK Logger init failed\n");
        return LOGUPLOAD_STATUS_LOGGER_FAIL;
    }
    RDK_LOG(RDK_LOG_NOTICE, LOGUPLOAD_MODULE, "Logger initialized\n");

    context_zero(ctx);

    load_properties(ctx);
    if(!load_mac(ctx))
        return LOGUPLOAD_STATUS_DEVICE_INFO_FAIL;

    generate_timestamps(ctx);
    build_paths(ctx);

    /* Config defaults */
    ctx->config.curl_tls_timeout       = 30;
    ctx->config.curl_timeout           = 10;
    ctx->config.num_upload_attempts    = 3;
    ctx->config.cb_num_upload_attempts = 1;
    ctx->flags.encryption_enabled_rfc  = false;
    ctx->flags.privacy_block           = false;

    load_upload_setting(ctx);

    /* TR-181 values via dedicated file */
    if(tr181_get_string(TR181_RRD_ISSUE_TYPE, ctx->tr181.rrd_issue_type, sizeof(ctx->tr181.rrd_issue_type)))
        RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "TR181 IssueType=%s\n", ctx->tr181.rrd_issue_type);
    if(tr181_get_string(TR181_CLOUD_URL, ctx->tr181.cloud_url, sizeof(ctx->tr181.cloud_url)))
        RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "TR181 CloudURL=%s\n", ctx->tr181.cloud_url);
    if(tr181_get_bool(TR181_REBOOT_DISABLE, &ctx->flags.tr181_unsched_reboot_disable))
        RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "TR181 RebootDisable=%d\n", ctx->flags.tr181_unsched_reboot_disable);

    if(access("/etc/os-release", F_OK) == 0)
    {
        bool enc = false;
        if(tr181_get_bool(TR181_ENCRYPT_CLOUDUPLOAD, &enc))
        {
            ctx->flags.encryption_enabled_rfc = enc;
            RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE,
                    "TR181 EncryptCloudUpload=%d\n", ctx->flags.encryption_enabled_rfc);
        }
    }

    eval_dynamic_flags(ctx);

    /* Result codes */
    ctx->results.success_code = 0;
    ctx->results.failed_code  = 1;
    ctx->results.aborted_code = 2;

    RDK_LOG(RDK_LOG_NOTICE, LOGUPLOAD_MODULE,
            "Context ready: log_path=%s device_type=%s mac=%s\n",
            ctx->paths.dirs.log_path,
            ctx->identity.device_type,
            ctx->identity.mac_raw);

    return LOGUPLOAD_STATUS_OK;
}

void logupload_context_deinit(logupload_context_t* ctx)
{
    (void)ctx;
    /* Optional: rdk_logger_deinit(); */
}

bool logupload_is_codebig_required(const logupload_context_t* ctx)
{
    return ctx && ctx->flags.use_codebig;
}

const char* logupload_get_cloud_url(const logupload_context_t* ctx)
{
    if(!ctx) return NULL;
    return ctx->tr181.cloud_url[0] ? ctx->tr181.cloud_url : NULL;
}
