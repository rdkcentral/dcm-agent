/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/**
 * @file uploadstblogs.c
 * @brief Main implementation for uploadSTBLogs library and binary
 *
 * This file contains the core implementation including uploadstblogs_execute() API.
 * When compiled with -DUPLOADSTBLOGS_BUILD_BINARY, it also includes main() for the binary.
 * When compiled as a library, main() is excluded via conditional compilation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>

#include "uploadstblogs.h"
#include "context_manager.h"
#include "validation.h"
#include "strategy_selector.h"
#include "strategy_handler.h"
#include "archive_manager.h"
#include "upload_engine.h"
#include "file_operations.h"
#include "cleanup_handler.h"
#include "event_manager.h"
#include "system_utils.h"
#include "rdk_debug.h"
#include "uploadlogsnow.h"

#ifdef T2_EVENT_ENABLED
#include <telemetry_busmessage_sender.h>
#endif

#define STATUS_FILE "/opt/loguploadstatus.txt"
#define DCM_TEMP_DIR "/tmp/DCM"

/* Forward declarations */
static int lock_fd = -1;

/* Telemetry helper functions */
void t2_count_notify(char *marker)
{
#ifdef T2_EVENT_ENABLED
    t2_event_d(marker, 1);
#else
    (void)marker;
#endif
}

void t2_val_notify(char *marker, char *val)
{
#ifdef T2_EVENT_ENABLED
    t2_event_s(marker, val);
#else
    (void)marker;
    (void)val;
#endif
}

bool parse_args(int argc, char** argv, RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    
    // Check for special "uploadlogsnow" parameter first
    if (argc >= 2 && strcmp(argv[1], "uploadlogsnow") == 0) {
        // Set UploadLogsNow-specific parameters
        ctx->flag = 1;                          // Upload enabled
        ctx->dcm_flag = 1;                      // Use DCM mode
        ctx->upload_on_reboot = 1;              // Upload on reboot enabled
        ctx->trigger_type = TRIGGER_ONDEMAND;   // ONDEMAND trigger (5)
        ctx->rrd_flag = 0;                      // Not RRD upload
        ctx->tls_enabled = false;               // Default to HTTP
        ctx->uploadlogsnow_mode = true;         // Enable UploadLogsNow mode
        
        RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTBLOGS", "UploadLogsNow mode enabled\n");
        return true;
    }
    
    // DO NOT memset - context is already initialized with device info
    // Only parse command line arguments and set those specific fields
    
    // Parse arguments (script passes 9 arguments)
    // argv[1] - TFTP_SERVER (legacy, may be unused)
    // argv[2] - FLAG 
    // argv[3] - DCM_FLAG
    // argv[4] - UploadOnReboot
    // argv[5] - UploadProtocol
    // argv[6] - UploadHttpLink
    // argv[7] - TriggerType
    // argv[8] - RRD_FLAG
    // argv[9] - RRD_UPLOADLOG_FILE
    
    if (argc >= 3 && argv[2]) {
        // Parse FLAG
        ctx->flag = atoi(argv[2]);
        fprintf(stderr, "DEBUG: FLAG (argv[2]) = '%s' -> %d\n", argv[2], ctx->flag);
    }
    
    if (argc >= 4 && argv[3]) {
        // Parse DCM_FLAG
        ctx->dcm_flag = atoi(argv[3]);
        fprintf(stderr, "DEBUG: DCM_FLAG (argv[3]) = '%s' -> %d\n", argv[3], ctx->dcm_flag);
    }
    
    if (argc >= 5 && argv[4]) {
        // Parse UploadOnReboot
        ctx->upload_on_reboot = (strcmp(argv[4], "true") == 0) ? 1 : 0;
        fprintf(stderr, "DEBUG: UploadOnReboot (argv[4]) = '%s' -> %d\n", argv[4], ctx->upload_on_reboot);
    }
    
    if (argc >= 6 && argv[5]) {
        // Parse UploadProtocol - stored in settings
        if (strcmp(argv[5], "HTTPS") == 0) {
            ctx->tls_enabled = true;
        }
    }
    
    if (argc >= 7 && argv[6]) {
        // Parse UploadHttpLink
        strncpy(ctx->upload_http_link, argv[6], sizeof(ctx->upload_http_link) - 1);
        fprintf(stderr, "DEBUG: upload_http_link (argv[6]) = '%s'\n", argv[6]);
    }
    
    if (argc >= 8 && argv[7]) {
        // Parse TriggerType
        fprintf(stderr, "DEBUG: TriggerType (argv[7]) = '%s'\n", argv[7]);
        if (strcmp(argv[7], "cron") == 0) {
            ctx->trigger_type = TRIGGER_SCHEDULED;
        } else if (strcmp(argv[7], "ondemand") == 0) {
            ctx->trigger_type = TRIGGER_ONDEMAND;
        } else if (strcmp(argv[7], "manual") == 0) {
            ctx->trigger_type = TRIGGER_MANUAL;
        } else if (strcmp(argv[7], "reboot") == 0) {
            ctx->trigger_type = TRIGGER_REBOOT;
        }
        fprintf(stderr, "DEBUG: trigger_type = %d\n", ctx->trigger_type);
    }
    
    if (argc >= 9 && argv[8]) {
        // Parse RRD_FLAG
        ctx->rrd_flag = (strcmp(argv[8], "true") == 0) ? 1 : 0;
        fprintf(stderr, "DEBUG: RRD_FLAG (argv[8]) = '%s' -> %d\n", argv[8], ctx->rrd_flag);
    }
    
    if (argc >= 10 && argv[9]) {
        // Parse RRD_UPLOADLOG_FILE
        strncpy(ctx->rrd_file, argv[9], sizeof(ctx->rrd_file) - 1);
    }
    
    return true;
}

bool acquire_lock(const char* lock_path)
{
    if (!lock_path) {
        return false;
    }
    
    // Open lock file for writing (create if doesn't exist)
    lock_fd = open(lock_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (lock_fd == -1) {
        perror("Failed to open lock file");
        return false;
    }
    
    // Try to acquire exclusive non-blocking lock (matches script flock -n)
    if (flock(lock_fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // Another instance is running
            close(lock_fd);
            lock_fd = -1;
            return false;
        } else {
            perror("Failed to acquire lock");
            close(lock_fd);
            lock_fd = -1;
            return false;
        }
    }
    
    return true;
}

void release_lock(void)
{
    if (lock_fd != -1) {
        // Release the lock by closing the file descriptor
        // This automatically releases the flock
        close(lock_fd);
        lock_fd = -1;
    }
}

bool is_maintenance_enabled(void)
{
    // Check if maintenance mode is enabled from /etc/device.properties
    char buffer[256] = {0};
    if (getDevicePropertyData("ENABLE_MAINTENANCE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        return (strcasecmp(buffer, "true") == 0);
    }
    return false;
}

int uploadstblogs_run(const UploadSTBLogsParams* params)
{
    static RuntimeContext ctx;
    SessionState session = {0};
    int ret = 1;

    if (!params) {
        fprintf(stderr, "Invalid parameters\n");
        return 1;
    }

    /* Clear context to ensure clean state */
    memset(&ctx, 0, sizeof(ctx));

    /* Acquire lock to ensure single instance */
    if (!acquire_lock("/tmp/.log-upload.lock")) {
        fprintf(stderr, "Failed to acquire lock - another instance running\n");
        if (is_maintenance_enabled()) {
            send_iarm_event_maintenance(16);
        }
        return 1;
    }

    /* Initialize telemetry system */
#ifdef T2_EVENT_ENABLED
    t2_init("uploadstblogs");
#endif

    /* Initialize runtime context */
    if (!init_context(&ctx)) {
        fprintf(stderr, "Failed to initialize context\n");
        release_lock();
        return 1;
    }

    /* Set parameters from API call */
    ctx.flag = params->flag;
    ctx.dcm_flag = params->dcm_flag;
    ctx.upload_on_reboot = params->upload_on_reboot ? 1 : 0;
    ctx.trigger_type = params->trigger_type;
    ctx.rrd_flag = params->rrd_flag ? 1 : 0;

    if (params->upload_protocol && strcmp(params->upload_protocol, "HTTPS") == 0) {
        ctx.tls_enabled = true;
    }

    if (params->upload_http_link) {
        strncpy(ctx.upload_http_link, params->upload_http_link,
                sizeof(ctx.upload_http_link) - 1);
    }

    if (params->rrd_file) {
        strncpy(ctx.rrd_file, params->rrd_file, sizeof(ctx.rrd_file) - 1);
    }

    /* Validate system prerequisites */
    if (!validate_system(&ctx)) {
        fprintf(stderr, "System validation failed\n");
        release_lock();
        return 1;
    }

    /* Perform early return checks and determine strategy */
    Strategy strategy = early_checks(&ctx);
    session.strategy = strategy;

    /* Handle early abort strategies */
    if (strategy == STRAT_PRIVACY_ABORT) {
        enforce_privacy(ctx.log_path);
        emit_privacy_abort();
        release_lock();
        return 0;
    }

    /* Emit upload start event */
    emit_upload_start();

    /* Prepare archive based on strategy */
    if (strategy == STRAT_RRD) {
        if (!file_exists(ctx.rrd_file)) {
            fprintf(stderr, "RRD archive file does not exist: %s\n", ctx.rrd_file);
            release_lock();
            return 1;
        }

        strncpy(session.archive_file, ctx.rrd_file, sizeof(session.archive_file) - 1);
        session.archive_file[sizeof(session.archive_file) - 1] = '\0';

        decide_paths(&ctx, &session);
        if (!execute_upload_cycle(&ctx, &session)) {
            fprintf(stderr, "RRD upload failed\n");
            ret = 1;
        } else {
            ret = 0;
        }
    } else {
        if (execute_strategy_workflow(&ctx, &session) != 0) {
            fprintf(stderr, "Strategy workflow failed\n");
            release_lock();
            return 1;
        }
        ret = session.success ? 0 : 1;
    }

    /* Finalize: cleanup, update markers, emit events */
    finalize(&ctx, &session);

    /* Uninitialize telemetry system */
#ifdef T2_EVENT_ENABLED
    t2_uninit();
#endif

    /* Cleanup IARM connection */
    cleanup_iarm_connection();

    /* Release lock and exit */
    release_lock();
    return ret;
}

int uploadstblogs_execute(int argc, char** argv)
{
    static RuntimeContext ctx;
    SessionState session = {0};
    int ret = 1;

    /* Clear context to ensure clean state */
    memset(&ctx, 0, sizeof(ctx));

    /* Acquire lock to ensure single instance */
    if (!acquire_lock("/tmp/.log-upload.lock")) {
        fprintf(stderr, "Failed to acquire lock - another instance running\n");
        /* Script sends MAINT_LOGUPLOAD_INPROGRESS when another instance is already running */
        if (is_maintenance_enabled()) {
            send_iarm_event_maintenance(16);  // Matches script: eventSender "MaintenanceMGR" $MAINT_LOGUPLOAD_INPROGRESS
        }
        return 1;
    }

    /* Initialize telemetry system (matches rdm-agent pattern) */
#ifdef T2_EVENT_ENABLED
    t2_init("uploadstblogs");
#endif

    /* Initialize runtime context */
    if (!init_context(&ctx)) {
        fprintf(stderr, "Failed to initialize context\n");
        release_lock();
        return 1;
    }
    
    /* Verify context after initialization */
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[main] Context after init: ctx addr=%p, MAC='%s', device_type='%s'\n",
            (void*)&ctx, ctx.mac_address,
            strlen(ctx.device_type) > 0 ? ctx.device_type : "(empty)");

    /* Parse command-line arguments */
    if (!parse_args(argc, argv, &ctx)) {
        fprintf(stderr, "Failed to parse arguments\n");
        release_lock();
        return 1;
    }
    
    /* Handle UploadLogsNow mode - use custom implementation */
    if (ctx.uploadlogsnow_mode) {
        RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                "[%s:%d] UploadLogsNow mode detected, executing custom workflow\n", 
                __FUNCTION__, __LINE__);
        
        ret = execute_uploadlogsnow_workflow(&ctx);
        
        /* Release lock and exit */
        release_lock();
        return ret;
    }
    
    /* Verify context after parse_args */
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[main] Context after parse_args: MAC='%s', device_type='%s'\n",
            ctx.mac_address,
            strlen(ctx.device_type) > 0 ? ctx.device_type : "(empty)");

    /* Validate system prerequisites */
    if (!validate_system(&ctx)) {
        fprintf(stderr, "System validation failed\n");
        release_lock();
        return 1;
    }

    /* Perform early return checks and determine strategy */
    Strategy strategy = early_checks(&ctx);
    session.strategy = strategy;

    /* Handle early abort strategies */
    if (strategy == STRAT_PRIVACY_ABORT) {
        enforce_privacy(ctx.log_path);
        emit_privacy_abort();
        release_lock();
        return 0;
    }

    /* Note: STRAT_NO_LOGS removed - each strategy now checks for logs internally */

    /* Emit upload start event (matches script MAINT_LOGUPLOAD_INPROGRESS) */
    emit_upload_start();

    /* Prepare archive based on strategy */
    if (strategy == STRAT_RRD) {
        // RRD: Upload pre-existing archive file directly (provided via command line)
        if (!file_exists(ctx.rrd_file)) {
            fprintf(stderr, "RRD archive file does not exist: %s\n", ctx.rrd_file);
            release_lock();
            return 1;
        }
        
        // Store RRD file path in session for upload
        strncpy(session.archive_file, ctx.rrd_file, sizeof(session.archive_file) - 1);
        session.archive_file[sizeof(session.archive_file) - 1] = '\0';
        
        // Decide paths and upload
        decide_paths(&ctx, &session);
        if (!execute_upload_cycle(&ctx, &session)) {
            fprintf(stderr, "RRD upload failed\n");
            ret = 1;
        } else {
            ret = 0;
        }
    } else {
        // Other strategies: execute full workflow (setup, archive, upload, cleanup)
        if (execute_strategy_workflow(&ctx, &session) != 0) {
            fprintf(stderr, "Strategy workflow failed\n");
            release_lock();
            return 1;
        }
        ret = session.success ? 0 : 1;
    }

    /* Finalize: cleanup, update markers, emit events */
    finalize(&ctx, &session);

    /* Uninitialize telemetry system */
#ifdef T2_EVENT_ENABLED
    t2_uninit();
#endif

    /* Cleanup IARM connection */
    cleanup_iarm_connection();

    /* Release lock and exit */
    release_lock();
    return ret;
}

#ifdef UPLOADSTBLOGS_BUILD_BINARY
/**
 * @brief Main entry point for standalone binary
 * 
 * This is only compiled when building the binary, not the library.
 * External components should call uploadstblogs_execute() directly.
 */
int main(int argc, char** argv)
{
    return uploadstblogs_execute(argc, argv);
}
#endif /* UPLOADSTBLOGS_BUILD_BINARY */


