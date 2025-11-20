#include "logupload.h"
#include "rdk_logger.h"
#include "rdk_fwdl_utils.h"  // from @rdkcentral/common_utilities
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Helper: flatten MAC to "ccffee112233"
static void read_and_flatten_mac(logupload_mac_t* mac) {
    FILE* fm = fopen("/sys/class/net/eth0/address", "r");
    if (fm) {
        if (fgets(mac->mac_raw, sizeof(mac->mac_raw), fm))
            mac->mac_raw[strcspn(mac->mac_raw, "\n")] = '\0';
        fclose(fm);
    } else {
        strncpy(mac->mac_raw, "00:00:00:00:00:00", sizeof(mac->mac_raw)-1);
    }
    size_t j=0;
    for(size_t i=0; mac->mac_raw[i] && j < sizeof(mac->mac_compact)-1; ++i)
        if(mac->mac_raw[i] != ':')
            mac->mac_compact[j++] = mac->mac_raw[i];
    mac->mac_compact[j]='\0';
}

// Helper: timestamps
static void generate_timestamps(logupload_timestamps_t* ts) {
    time_t t = time(NULL); struct tm tm_info;
    localtime_r(&t, &tm_info);
    strftime(ts->dt_stamp, sizeof(ts->dt_stamp), "%m-%d-%y-%I-%M%p", &tm_info);
    strftime(ts->timestamp_long, sizeof(ts->timestamp_long), "%Y-%m-%d-%H-%M-%S%p", &tm_info);
    strncpy(ts->time_value_prefix, ts->dt_stamp, sizeof(ts->time_value_prefix)-1);
}

// Helper: build path
/*
static void join_path(char* dst, size_t sz, const char* base, const char* suffix) {
    snprintf(dst, sz, "%s%s", base, suffix); // Same as rfcMgr utility style
} */

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
) {
    if(rdk_logger_init(LOGUPLOAD_DEBUG_INI) != RDK_SUCCESS)
        return LOGUPLOAD_STATUS_LOGGER_FAIL;
    memset(paths, 0, sizeof(*paths));
    memset(mac, 0, sizeof(*mac));
    memset(stamps, 0, sizeof(*stamps));
    if(device_type) device_type[0]=0;
    if(build_type) build_type[0]=0;
    if(rrd_issue_type) rrd_issue_type[0]=0;
    if(cloud_url) cloud_url[0]=0;

    // --- Use common_utilities for device properties ---
    getDevicePropertyData("DEVICE_TYPE", device_type, device_type_sz);
    getDevicePropertyData("BUILD_TYPE", build_type, build_type_sz);
    getIncludePropertyData("LOG_PATH", paths->log_path, sizeof(paths->log_path));
    getDevicePropertyData("DCM_LOG_PATH", paths->dcm_log_path, sizeof(paths->log_path));
    
    GetEstbMac(mac, sizeof(mac));
    
    
    if(paths->log_path[0]=='\0')
        strncpy(paths->log_path, LOGUPLOAD_DEFAULT_LOG_PATH, sizeof(paths->log_path)-1);

    //read_and_flatten_mac(mac);
    generate_timestamps(stamps);
    snprintf(paths->packaged_logs_file, sizeof(paths->packaged_logs_file), LOGUPLOAD_LOGS_ARCHIVE_FMT, mac->mac_compact, stamps->dt_stamp);

    // Use standard rfcMgr-style logic for upload flags
    if (upload_enabled) *upload_enabled = true; // Your real logic may use deviceprops, config, etc.
    if (use_codebig) *use_codebig = (access(LOGUPLOAD_DIRECT_BLOCK_FILE, F_OK) == 0);
    if (enable_ocsp_stapling) *enable_ocsp_stapling = (access(LOGUPLOAD_ENABLE_OCSP_STAPLING_FILE, F_OK) == 0);
    if (enable_ocsp) *enable_ocsp = (access(LOGUPLOAD_ENABLE_OCSPCA_FILE, F_OK) == 0);
    if (encryption_enabled_rfc) *encryption_enabled_rfc = false;
    if (rrd_issue_type) tr181_get_string(LOGUPLOAD_TR181_RRD_ISSUE_TYPE, rrd_issue_type, rrd_issue_type_sz);
    if (cloud_url) tr181_get_string(LOGUPLOAD_TR181_CLOUD_URL, cloud_url, cloud_url_sz);

    return LOGUPLOAD_STATUS_OK;
}

void logupload_context_deinit(void) {}

bool logupload_is_codebig_required(bool use_codebig) { return use_codebig; }
const char* logupload_get_cloud_url(const char* cloud_url) { return cloud_url && cloud_url[0]?cloud_url:NULL; }
