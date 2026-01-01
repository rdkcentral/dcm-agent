
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
 * @file uploadstblogs.h
 * @brief Main header for uploadSTBLogs application
 *
 * This file contains the main entry point declarations and high-level
 * application interfaces.
 */

#ifndef UPLOADSTBLOGS_H
#define UPLOADSTBLOGS_H

#include "uploadstblogs_types.h"

/**
 * @brief Parse command-line arguments
 * @param argc Argument count
 * @param argv Argument vector
 * @param ctx Runtime context to populate
 * @return true on success, false on failure
 */
bool parse_args(int argc, char** argv, RuntimeContext* ctx);

/**
 * @brief Acquire file lock to ensure single instance
 * @param lock_path Path to lock file
 * @return true if lock acquired, false otherwise
 */
bool acquire_lock(const char* lock_path);

/**
 * @brief Release previously acquired lock
 */
void release_lock(void);

/**
 * @brief Public API for executing STB log upload from external components
 * 
 * This is the recommended API for external components to call.
 * It takes structured parameters instead of argc/argv.
 *
 * @param params Pointer to UploadSTBLogsParams structure with upload parameters
 * @return 0 on success, 1 on failure
 * 
 * @note This function handles its own locking and resource cleanup.
 *       It can be called from any component.
 * 
 * Example usage:
 * @code
 *   UploadSTBLogsParams params = {
 *       .flag = 1,
 *       .dcm_flag = 0,
 *       .upload_on_reboot = false,
 *       .upload_protocol = "HTTPS",
 *       .upload_http_link = "https://example.com/upload",
 *       .trigger_type = TRIGGER_ONDEMAND,
 *       .rrd_flag = false,
 *       .rrd_file = NULL
 *   };
 *   int result = uploadstblogs_run(&params);
 * @endcode
 */
int uploadstblogs_run(const UploadSTBLogsParams* params);

/**
 * @brief Internal API for executing STB log upload with argc/argv (used by main)
 * 
 * This function is used internally by main() and kept for compatibility.
 * External components should use uploadstblogs_run() instead.
 * 
 * This function encapsulates the complete log upload workflow and can be
 * called directly from other components without requiring the main() entry point.
 * It handles initialization, validation, strategy execution, and cleanup.
 *
 * @param argc Argument count (same as main)
 * @param argv Argument vector (same as main):
 *             argv[1]: LOG_PATH (e.g., "/opt/logs")
 *             argv[2]: DCM_LOG_PATH (e.g., "/tmp/DCM")
 *             argv[3]: DCM_FLAG (integer)
 *             argv[4]: UploadOnReboot ("true"/"false")
 *             argv[5]: UploadProtocol ("HTTPS"/"HTTP")
 *             argv[6]: UploadHttpLink (URL)
 *             argv[7]: TriggerType ("cron"/"ondemand"/"manual"/"reboot")
 *             argv[8]: RRD_FLAG ("true"/"false")
 *             argv[9]: RRD_UPLOADLOG_FILE (path to RRD archive)
 * 
 * @return 0 on success, 1 on failure
 * 
 * @note This function handles its own locking and resource cleanup.
 *       It is safe to call from external components.
 */
int uploadstblogs_execute(int argc, char** argv);

/**
 * @brief Main application entry point
 * 
 * This is a thin wrapper around uploadstblogs_execute() that provides
 * the standard main() interface for the standalone binary.
 */
int main(int argc, char** argv);

#endif /* UPLOADSTBLOGS_H */
