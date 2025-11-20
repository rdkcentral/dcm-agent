#include "logupload.h"
#include <stdio.h>
int main(void)
{
    logupload_mac_t mac; char device_type[LOGUPLOAD_MAX_STR_FIELD]="", build_type[LOGUPLOAD_MAX_STR_FIELD]="";
    logupload_paths_t paths; logupload_timestamps_t stamps;
    bool upload_enabled=0, use_codebig=0, enable_ocsp_stapling=0, enable_ocsp=0, encryption_enabled_rfc=0;
    char rrd_issue_type[LOGUPLOAD_MAX_STR_FIELD]="", cloud_url[LOGUPLOAD_MAX_PATH_LENGTH]="";
    logupload_status_t st = logupload_context_init(
        &mac, device_type, sizeof(device_type), build_type, sizeof(build_type),
        &paths, &stamps, &upload_enabled, &use_codebig, &enable_ocsp_stapling, &enable_ocsp, &encryption_enabled_rfc,
        rrd_issue_type, sizeof(rrd_issue_type), cloud_url, sizeof(cloud_url)
    );
    if(st != LOGUPLOAD_STATUS_OK) { printf("Init failed: %d\n", st); return 1; }
    printf("MAC: %s | DeviceType: %s | BuildType: %s\n", mac.mac_raw, device_type, build_type);
    printf("Cloud URL: %s\n", logupload_get_cloud_url(cloud_url) ?: "(none)");
    printf("Packaged logs: %s\n", paths.packaged_logs_file);
    printf("Upload enabled: %d | CodeBig: %d | OCSP stapling: %d | OCSP: %d | Encrypt RFC: %d\n", upload_enabled, use_codebig, enable_ocsp_stapling, enable_ocsp, encryption_enabled_rfc);
    printf("RRD Issue Type: %s\n", rrd_issue_type);
    logupload_context_deinit();
    return 0;
}
