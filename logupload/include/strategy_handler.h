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
 * @file strategy_handler.h
 * @brief Strategy-based upload workflow handlers
 *
 * This module implements the strategy handler pattern where each upload strategy
 * (ONDEMAND, REBOOT/NON_DCM, DCM) has its own complete workflow implementation.
 */

#ifndef STRATEGY_HANDLER_H
#define STRATEGY_HANDLER_H

#include "uploadstblogs_types.h"

/**
 * @struct StrategyHandler
 * @brief Function pointers for strategy-specific workflow phases
 */
typedef struct {
    /**
     * @brief Setup phase - prepare working directory and files
     * @param ctx Runtime context
     * @param session Session state
     * @return 0 on success, -1 on failure
     */
    int (*setup_phase)(RuntimeContext* ctx, SessionState* session);

    /**
     * @brief Archive phase - create tar.gz archive
     * @param ctx Runtime context
     * @param session Session state
     * @return 0 on success, -1 on failure
     */
    int (*archive_phase)(RuntimeContext* ctx, SessionState* session);

    /**
     * @brief Upload phase - upload archive to server
     * @param ctx Runtime context
     * @param session Session state
     * @return 0 on success, -1 on failure
     */
    int (*upload_phase)(RuntimeContext* ctx, SessionState* session);

    /**
     * @brief Cleanup phase - post-upload cleanup and backup
     * @param ctx Runtime context
     * @param session Session state
     * @param upload_success Whether upload was successful
     * @return 0 on success, -1 on failure
     */
    int (*cleanup_phase)(RuntimeContext* ctx, SessionState* session, bool upload_success);
} StrategyHandler;

/**
 * @brief Get the appropriate strategy handler for the given strategy
 * @param strategy Upload strategy
 * @return Pointer to strategy handler, or NULL if invalid strategy
 */
const StrategyHandler* get_strategy_handler(Strategy strategy);

/**
 * @brief Execute complete upload workflow for the given strategy
 * @param ctx Runtime context
 * @param session Session state (strategy must be set)
 * @return 0 on success, -1 on failure
 */
int execute_strategy_workflow(RuntimeContext* ctx, SessionState* session);

/* Strategy-specific handler implementations */

/**
 * @brief ONDEMAND strategy handler
 * - Working dir: /tmp/log_on_demand
 * - Source: LOG_PATH (current logs)
 * - No timestamps
 * - No permanent backup
 * - Temp directory deleted after upload
 */
extern const StrategyHandler ondemand_strategy_handler;

/**
 * @brief REBOOT/NON_DCM strategy handler
 * - Working dir: PREV_LOG_PATH
 * - Source: PREV_LOG_PATH (previous boot logs)
 * - Timestamps added before upload, removed after
 * - Permanent backup created (always)
 * - Includes PCAP and DRI logs
 * - Sleep delay if uptime < 15min
 */
extern const StrategyHandler reboot_strategy_handler;

/**
 * @brief DCM strategy handler
 * - Working dir: DCM_LOG_PATH
 * - Source: DCM_LOG_PATH (batched logs + current logs)
 * - Timestamps added before upload
 * - No permanent backup
 * - Entire directory deleted after upload
 * - Includes PCAP, no DRI
 */
extern const StrategyHandler dcm_strategy_handler;

#endif /* STRATEGY_HANDLER_H */
