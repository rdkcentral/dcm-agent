#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include "privacy_mode.h"

int main(void) {
    Context ctx;

    printf("Initializing context...\n");
    if (!context_init(&ctx)) {
        fprintf(stderr, "context_init failed!\n");
        return 1;
    }
    printf("Validation...\n");

    printf("\nVerifying parsed context fields:\n");
    printf("MAC raw        : %s\n", ctx.mac_raw);
    printf("MAC compact    : %s\n", ctx.mac_compact);
    //printf("Host IP        : %s\n", ctx.host_ip);
    printf("Device Type    : %s\n", ctx.device_type);
    printf("Build Type     : %s\n", ctx.build_type);

    printf("log_path       : %s\n", ctx.log_path);
    printf("dcm_log_path   : %s\n", ctx.dcm_log_path);
    printf("prev_log_path  : %s\n", ctx.prev_log_path);
    printf("prev_log_backup: %s\n", ctx.prev_log_backup_path);
    printf("dcm_upload_list_path : %s\n", ctx.dcm_upload_list_path);
    printf("dcm_script_log_path  : %s\n", ctx.dcm_script_log_path);
    printf("tls_error_log_path   : %s\n", ctx.tls_error_log_path);
    printf("rrd_log_file    : %s\n", ctx.rrd_log_file);
    printf("curl_info_path  : %s\n", ctx.curl_info_path);
    printf("http_code_path  : %s\n", ctx.http_code_path);
    printf("previous_reboot_info_path: %s\n", ctx.previous_reboot_info_path);
    printf("direct_block_file        : %s\n", ctx.direct_block_file);
    printf("codebig_block_file       : %s\n", ctx.codebig_block_file);
    printf("telemetry_path           : %s\n", ctx.telemetry_path);
    printf("rrd_log_dir              : %s\n", ctx.rrd_log_dir);
    printf("iarm_event_bin_dir       : %s\n", ctx.iarm_event_bin_dir);

    printf("log_file        : %s\n", ctx.log_file);
    printf("dri_log_file    : %s\n", ctx.dri_log_file);

    printf("dt_stamp        : %s\n", ctx.dt_stamp);
    printf("timestamp_long  : %s\n", ctx.timestamp_long);

    printf("enable_ocsp_stapling   : %d\n", ctx.enable_ocsp_stapling);
    printf("enable_ocsp           : %d\n", ctx.enable_ocsp);
    printf("encryption_enabled_rfc: %d\n", ctx.encryption_enabled_rfc);
    printf("upload_flag           : %d\n", ctx.upload_flag);
    printf("use_codebig           : %d\n", ctx.use_codebig);
    printf("privacy_block         : %d\n", ctx.privacy_block);

    printf("num_upload_attempts    : %d\n", ctx.num_upload_attempts);
    printf("cb_num_upload_attempts : %d\n", ctx.cb_num_upload_attempts);
    printf("curl_tls_timeout       : %d\n", ctx.curl_tls_timeout);
    printf("curl_timeout           : %d\n", ctx.curl_timeout);

    printf("tr181_unsched_reboot_disable : %d\n", ctx.tr181_unsched_reboot_disable);
    printf("rrd_tr181_name               : %s\n", ctx.rrd_tr181_name);

    printf("tls_option        : %s\n", ctx.tls_option);
    printf("cloud_url         : %s\n", ctx.cloud_url);

    printf("log_upload_success_code : %d\n", ctx.log_upload_success_code);
    printf("log_upload_failed_code  : %d\n", ctx.log_upload_failed_code);
    printf("log_upload_aborted_code : %d\n", ctx.log_upload_aborted_code);

    printf("direct_blocktime : %d\n",ctx.directblocktime);
    printf("cb_block_time   : %d\n",ctx.cb_block_time);

    printf("\nAll done.\n");

    char *pm = get_privacy_mode_string();
    if (pm) {
        printf("PrivacyMode: %s\n", pm);
        free(pm);
    } else {
        printf("Failed to get privacy mode\n");
    }

    privacy_mode_t em;
    if (get_privacy_mode(&em) == OK) {
        printf("Enum: %d\n", em);
    }
    printf("DO_NOT_SHARE? %d\n", is_privacy_mode_do_not_share());


   int main(int argc, char *argv[])
{
    if (argc != 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *upload_url = argv[1];
    const char *file_path  = argv[2];


    int rc = runFileUpload(upload_url, file_path);
    if (rc == 0) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Upload completed successfully\n", __FUNCTION__);
        return EXIT_SUCCESS;
    } else {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Upload failed (rc=%d)\n", __FUNCTION__, rc);
        return EXIT_FAILURE;
    }
}
    

    context_deinit(&ctx);
    return 0;
}
