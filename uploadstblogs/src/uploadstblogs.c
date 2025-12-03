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
 * @brief Main entry point for uploadSTBLogs application
 *
 * This is the main entry point that orchestrates the entire log upload flow
 * according to the HLD design.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>

#include "uploadstblogs.h"
#include "context_manager.h"
#include "validation.h"
#include "strategy_selector.h"
#include "archive_manager.h"
#include "upload_engine.h"
#include "cleanup_handler.h"
#include "event_manager.h"
#include "system_utils.h"
#include "telemetry.h"

static int lock_fd = -1;

bool parse_args(int argc, char** argv, RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    
    // Initialize context with defaults
    memset(ctx, 0, sizeof(RuntimeContext));
    
    // Set default paths
    strncpy(ctx->paths.log_path, "/opt/logs", sizeof(ctx->paths.log_path) - 1);
    strncpy(ctx->paths.temp_dir, "/tmp", sizeof(ctx->paths.temp_dir) - 1);
    
    // Set default retry configuration
    ctx->retry.direct_max_attempts = 3;
    ctx->retry.codebig_max_attempts = 1;
    ctx->retry.curl_timeout = 30;
    
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
        ctx->flags.flag = atoi(argv[2]);
    }
    
    if (argc >= 4 && argv[3]) {
        // Parse DCM_FLAG
        ctx->flags.dcm_flag = atoi(argv[3]);
    }
    
    if (argc >= 5 && argv[4]) {
        // Parse UploadOnReboot
        ctx->flags.upload_on_reboot = (strcmp(argv[4], "true") == 0) ? 1 : 0;
    }
    
    if (argc >= 6 && argv[5]) {
        // Parse UploadProtocol - stored in settings
        if (strcmp(argv[5], "HTTPS") == 0) {
            ctx->settings.tls_enabled = true;
        }
    }
    
    if (argc >= 7 && argv[6]) {
        // Parse UploadHttpLink
        strncpy(ctx->endpoints.upload_http_link, argv[6], sizeof(ctx->endpoints.upload_http_link) - 1);
    }
    
    if (argc >= 8 && argv[7]) {
        // Parse TriggerType
        if (strcmp(argv[7], "cron") == 0) {
            ctx->flags.trigger_type = TRIGGER_SCHEDULED;
        } else if (strcmp(argv[7], "ondemand") == 0) {
            ctx->flags.trigger_type = TRIGGER_ONDEMAND;
        } else if (strcmp(argv[7], "manual") == 0) {
            ctx->flags.trigger_type = TRIGGER_MANUAL;
        } else if (strcmp(argv[7], "reboot") == 0) {
            ctx->flags.trigger_type = TRIGGER_REBOOT;
        }
    }
    
    if (argc >= 9 && argv[8]) {
        // Parse RRD_FLAG
        ctx->flags.rrd_flag = (strcmp(argv[8], "true") == 0) ? 1 : 0;
    }
    
    if (argc >= 10 && argv[9]) {
        // Parse RRD_UPLOADLOG_FILE
        strncpy(ctx->paths.rrd_file, argv[9], sizeof(ctx->paths.rrd_file) - 1);
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

int main(int argc, char** argv)
{
    RuntimeContext ctx = {0};
    SessionState session = {0};
    int ret = 1;

    /* Parse command-line arguments */
    if (!parse_args(argc, argv, &ctx)) {
        fprintf(stderr, "Failed to parse arguments\n");
        return 1;
    }

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
    telemetry_init();

    /* Initialize runtime context */
    if (!init_context(&ctx)) {
        fprintf(stderr, "Failed to initialize context\n");
        release_lock();
        return 1;
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
        enforce_privacy(ctx.paths.log_path);
        emit_privacy_abort();
        release_lock();
        return 0;
    }

    /* Note: STRAT_NO_LOGS removed - each strategy now checks for logs internally */

    /* Emit upload start event (matches script MAINT_LOGUPLOAD_INPROGRESS) */
    emit_upload_start();

    /* Prepare archive based on strategy */
    if (strategy == STRAT_RRD) {
        if (!prepare_rrd_archive(&ctx, &session)) {
            fprintf(stderr, "Failed to prepare RRD archive\n");
            release_lock();
            return 1;
        }
    } else {
        if (!prepare_archive(&ctx, &session)) {
            fprintf(stderr, "Failed to prepare archive\n");
            release_lock();
            return 1;
        }
    }

    /* Decide upload paths (primary and fallback) */
    decide_paths(&ctx, &session);

    /* Execute upload cycle with retry and fallback logic */
    if (!execute_upload_cycle(&ctx, &session)) {
        fprintf(stderr, "Upload failed\n");
        ret = 1;
    } else {
        ret = 0;
    }

    /* Finalize: cleanup, update markers, emit events */
    finalize(&ctx, &session);

    /* Uninitialize telemetry system */
    telemetry_uninit();

    /* Cleanup IARM connection */
    cleanup_iarm_connection();

    /* Release lock and exit */
    release_lock();
    return ret;
}
