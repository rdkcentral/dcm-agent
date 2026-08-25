/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file uploadlogsnow.h
 * @brief Header file for UploadLogsNow functionality in logupload binary
 */

#ifndef UPLOADLOGSNOW_H
#define UPLOADLOGSNOW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uploadstblogs_types.h"

/**
 * @brief Execute UploadLogsNow workflow
 *
 * This function replicates the behavior of the original UploadLogsNow.sh script:
 * 1. Creates DCM_LOG_PATH directory
 * 2. Copies all files from LOG_PATH to DCM_LOG_PATH (excluding certain directories)
 * 3. Adds timestamps to files
 * 4. Creates tar archive
 * 5. Uploads using ONDEMAND strategy
 * 6. Cleans up temporary files
 *
 * @param ctx Runtime context with configuration and paths
 * @return 0 on success, negative value on failure
 */
int execute_uploadlogsnow_workflow(RuntimeContext* ctx);

/**
 * @brief Copy top-level files from a source directory into DCM_LOG_PATH
 *
 * Copies each regular file from @p src_path into @p dest_path, excluding the
 * `dcm/`, `PreviousLogs/`, and `PreviousLogs_backup/` sub-directories (so it never
 * recurses into its own staging/backup dirs). Used by the on-demand UploadLogsNow
 * workflow and by the scheduled DCM strategy.
 *
 * @param src_path Source directory (e.g. LOG_PATH)
 * @param dest_path Destination directory (DCM_LOG_PATH)
 * @return Number of files copied (>= 0), or -1 on source open failure
 */
int copy_files_to_dcm_path(const char* src_path, const char* dest_path);

#ifdef __cplusplus
}
#endif

#endif /* UPLOADLOGSNOW_H */
