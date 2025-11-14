#include "context.h"
#include "rdk_logger.h"
#include <stdio.h>

int main(void) {
    Context ctx;
    char errmsg[256];

    if (!context_init(&ctx)) {
        fprintf(stderr, "Context init failed\n");
        return 1;
    }
    int v = context_validate(&ctx, errmsg, sizeof(errmsg));
    printf("Validation=%d (%s)\n", v, errmsg);
    printf("MAC=%s compact=%s\n", ctx.mac_raw, ctx.mac_compact);
    printf("LogFile=%s\n", ctx.log_file);
    printf("DRIFile=%s\n", ctx.dri_log_file);
    printf("DeviceType=%s Firmware=%s\n", ctx.device_type, ctx.firmware_version);
    printf("TLS Option=%s UploadFlag=%d UseCodebig=%d\n",
           ctx.tls_option, ctx.upload_flag, ctx.use_codebig);

    /* Example log usage */
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD , "Test info after initialization\n");

    rdk_logger_deinit();
    return 0;
}
