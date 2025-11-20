#include "logupload.h"
#include <stdio.h>

int main(void)
{
    logupload_context_t ctx;
    logupload_status_t st = logupload_context_init(&ctx);
    if(st != LOGUPLOAD_STATUS_OK)
    {
        printf("Init failed status=%d\n", st);
        return 1;
    }

    printf("MAC: %s\n", ctx.identity.mac_raw);
    printf("Cloud URL: %s\n", logupload_get_cloud_url(&ctx) ?: "(none)");
    printf("Packaged logs: %s\n", ctx.paths.files.packaged_logs_file);
    printf("Upload enabled: %d\n", ctx.flags.upload_enabled);
    printf("CodeBig required: %d\n", logupload_is_codebig_required(&ctx));
    printf("RRD Issue Type: %s\n", ctx.tr181.rrd_issue_type);

    logupload_context_deinit(&ctx);
    return 0;
}
