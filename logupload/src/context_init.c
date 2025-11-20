#include "logupload.h"
#include "rdk_logger.h"
#include "rdk_fwdl_utils.h"      /* getDevicePropertyData / getIncludePropertyData */
#include "common_device_api.h"   /* GetEstbMac */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Helper: flatten MAC to "ccffee112233" */
static void read_and_flatten_mac(logupload_mac_t* mac)
{
    if(!mac)
        return;

    RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "MAC: Attempting to read MAC via GetEstbMac()\n");
    int len = GetEstbMac(mac->mac_raw, sizeof(mac->mac_raw));
    if(len > 0)
    {
        mac->mac_raw[sizeof(mac->mac_raw)-1] = '\0';
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE, "MAC: Raw='%s' (len=%d)\n", mac->mac_raw, len);

        size_t j=0;
        for(size_t i=0; mac->mac_raw[i] && j < sizeof(mac->mac_compact)-1; ++i)
        {
            if(mac->mac_raw[i] != ':')
                mac->mac_compact[j++] = mac->mac_raw[i];
        }
        mac->mac_compact[j]='\0';
        RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "MAC: Compact='%s'\n", mac->mac_compact);
    }
    else
    {
        /* Logic unchanged: if len <= 0 we leave buffers zeroed */
        RDK_LOG(RDK_LOG_WARN, LOGUPLOAD_MODULE,
                "MAC: GetEstbMac returned len=%d; mac fields remain empty\n", len);
    }
}

/* Helper: timestamps */
static void generate_timestamps(logupload_timestamps_t* ts)
{
    if(!ts)
        return;

    RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE, "Timestamp: Generating timestamps\n");
    time_t t = time(NULL); 
    struct tm tm_info;
    if(!localtime_r(&t, &tm_info))
    {
        RDK_LOG(RDK_LOG_ERROR, LOGUPLOAD_MODULE, "Timestamp: localtime_r failed\n");
        return; /* Logic unchanged: silently returns with zeroed struct */
    }

    strftime(ts->dt_stamp, sizeof(ts->dt_stamp), "%m-%d-%y-%I-%M%p", &tm_info);
    strftime(ts->timestamp_long, sizeof(ts->timestamp_long), "%Y-%m-%d-%H-%M-%S%p", &tm_info);
    strncpy(ts->time_value_prefix, ts->dt_stamp, sizeof(ts->time_value_prefix)-1);
    ts->time_value_prefix[sizeof(ts->time_value_prefix)-1] = '\0';

    RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
            "Timestamp: dt_stamp='%s' timestamp_long='%s'\n",
            ts->dt_stamp, ts->timestamp_long);
}

logupload_status_t logupload_context_init(
    logupload_mac_t* mac,
    char* device_type, size_t device_type_sz,
    char* build_type,  size_t build_type_sz,
    logupload_paths_t* paths,
    logupload_timestamps_t* stamps,
    bool* enable_ocsp_stapling,
    bool* enable_ocsp,
    bool* encryption_enabled_rfc,
    char* rrd_issue_type, size_t rrd_issue_type_sz,
    char* cloud_url, size_t cloud_url_sz
)
{
    RDK_LOG(RDK_LOG_NOTICE, LOGUPLOAD_MODULE, "Init: Starting logupload_context_init\n");

    if(rdk_logger_init(LOGUPLOAD_DEBUG_INI) != RDK_SUCCESS)
    {
        fprintf(stderr, "RDK Logger init failed: %s\n", LOGUPLOAD_DEBUG_INI);
        RDK_LOG(RDK_LOG_ERROR, LOGUPLOAD_MODULE,
                "Init: rdk_logger_init failed for %s\n", LOGUPLOAD_DEBUG_INI);
        return LOGUPLOAD_STATUS_LOGGER_FAIL;
    }
    RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
            "Init: Logger initialized using %s\n", LOGUPLOAD_DEBUG_INI);

    /* Zero initialize structures / buffers (logic unchanged) */
    if(paths) memset(paths, 0, sizeof(*paths));
    if(mac)   memset(mac,   0, sizeof(*mac));
    if(stamps) memset(stamps, 0, sizeof(*stamps));

    if(device_type && device_type_sz) device_type[0]=0;
    if(build_type  && build_type_sz)  build_type[0]=0;
    if(rrd_issue_type && rrd_issue_type_sz) rrd_issue_type[0]=0;
    if(cloud_url && cloud_url_sz) cloud_url[0]=0;

    /* Device properties retrieval (logic unchanged) */
    if(device_type && device_type_sz)
    {
        int rc = getDevicePropertyData("DEVICE_TYPE", device_type, (unsigned int)device_type_sz);
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                "Init: getDevicePropertyData(DEVICE_TYPE) rc=%d value='%s'\n",
                rc, device_type);
    }
    if(build_type && build_type_sz)
    {
        int rc = getDevicePropertyData("BUILD_TYPE", build_type, (unsigned int)build_type_sz);
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                "Init: getDevicePropertyData(BUILD_TYPE) rc=%d value='%s'\n",
                rc, build_type);
    }
    if(paths)
    {
        int rc_inc = getIncludePropertyData("LOG_PATH", paths->log_path, sizeof(paths->log_path));
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                "Init: getIncludePropertyData(LOG_PATH) rc=%d value='%s'\n",
                rc_inc, paths->log_path);

        int rc_dev = getDevicePropertyData("DCM_LOG_PATH", paths->dcm_log_path, sizeof(paths->log_path)); /* Buffer size matches original logic */
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                "Init: getDevicePropertyData(DCM_LOG_PATH) rc=%d value='%s'\n",
                rc_dev, paths->dcm_log_path);

        if(paths->log_path[0]=='\0')
        {
            strncpy(paths->log_path, LOGUPLOAD_DEFAULT_LOG_PATH, sizeof(paths->log_path)-1);
            paths->log_path[sizeof(paths->log_path)-1]='\0';
            RDK_LOG(RDK_LOG_WARN, LOGUPLOAD_MODULE,
                    "Init: LOG_PATH empty; using default '%s'\n", LOGUPLOAD_DEFAULT_LOG_PATH);
        }
    }

    /* MAC + timestamps */
    read_and_flatten_mac(mac);
    generate_timestamps(stamps);

    /* Packaged logs file (logic unchanged) */
    if(paths && mac && stamps)
    {
        int written = snprintf(paths->packaged_logs_file,
                               sizeof(paths->packaged_logs_file),
                               LOGUPLOAD_LOGS_ARCHIVE_FMT,
                               mac->mac_compact,
                               stamps->dt_stamp);
        if(written < 0 || (size_t)written >= sizeof(paths->packaged_logs_file))
        {
            RDK_LOG(RDK_LOG_WARN, LOGUPLOAD_MODULE,
                    "Init: packaged_logs_file truncated (size=%zu)\n",
                    sizeof(paths->packaged_logs_file));
            paths->packaged_logs_file[sizeof(paths->packaged_logs_file)-1]='\0';
        }
        else
        {
            RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                    "Init: Packaged logs file='%s'\n", paths->packaged_logs_file);
        }
    }

    if(enable_ocsp_stapling)
    {
        *enable_ocsp_stapling = (access(LOGUPLOAD_ENABLE_OCSP_STAPLING_FILE, F_OK) == 0);
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                "Init: enable_ocsp_stapling=%d (file %s %s)\n",
                *enable_ocsp_stapling,
                LOGUPLOAD_ENABLE_OCSP_STAPLING_FILE,
                *enable_ocsp_stapling ? "present" : "absent");
    }
    if(enable_ocsp)
    {
        *enable_ocsp = (access(LOGUPLOAD_ENABLE_OCSPCA_FILE, F_OK) == 0);
        RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                "Init: enable_ocsp=%d (file %s %s)\n",
                *enable_ocsp,
                LOGUPLOAD_ENABLE_OCSPCA_FILE,
                *enable_ocsp ? "present" : "absent");
    }
    if(encryption_enabled_rfc)
    {
        *encryption_enabled_rfc = false;
        RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE,
                "Init: encryption_enabled_rfc default=%d\n",
                (int)*encryption_enabled_rfc);
    }

    if(rrd_issue_type && rrd_issue_type_sz)
    {
        if(tr181_get_string(LOGUPLOAD_TR181_RRD_ISSUE_TYPE,
                            rrd_issue_type, rrd_issue_type_sz))
            RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                    "Init: TR181 %s='%s'\n",
                    LOGUPLOAD_TR181_RRD_ISSUE_TYPE, rrd_issue_type);
        else
            RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE,
                    "Init: TR181 %s not available\n", LOGUPLOAD_TR181_RRD_ISSUE_TYPE);
    }
    if(cloud_url && cloud_url_sz)
    {
        if(tr181_get_string(LOGUPLOAD_TR181_CLOUD_URL,
                            cloud_url, cloud_url_sz))
            RDK_LOG(RDK_LOG_INFO, LOGUPLOAD_MODULE,
                    "Init: TR181 %s='%s'\n",
                    LOGUPLOAD_TR181_CLOUD_URL, cloud_url);
        else
            RDK_LOG(RDK_LOG_DEBUG, LOGUPLOAD_MODULE,
                    "Init: TR181 %s not available\n", LOGUPLOAD_TR181_CLOUD_URL);
    }

    RDK_LOG(RDK_LOG_NOTICE, LOGUPLOAD_MODULE,
            "Init: Completed. LOG_PATH='%s' DCM_LOG_PATH='%s' MAC='%s' Packaged='%s'\n",
            paths ? paths->log_path : "(null)",
            paths ? paths->dcm_log_path : "(null)",
            mac ? mac->mac_raw : "(null)",
            paths ? paths->packaged_logs_file : "(null)");

    return LOGUPLOAD_STATUS_OK;
}
