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
 * @file strategy_handler.c
 * @brief Strategy handler pattern implementation
 */

#include <stdio.h>
#include "strategy_handler.h"
#include "rdk_debug.h"
#include <string.h>

// Forward declarations of strategy handlers
extern const StrategyHandler ondemand_strategy_handler;
extern const StrategyHandler reboot_strategy_handler;
extern const StrategyHandler dcm_strategy_handler;

const StrategyHandler* get_strategy_handler(Strategy strategy)
{
    switch (strategy) {
        case STRAT_ONDEMAND:
            return &ondemand_strategy_handler;
        
        case STRAT_REBOOT:
        case STRAT_NON_DCM:
            return &reboot_strategy_handler;
        
        case STRAT_DCM:
            return &dcm_strategy_handler;
        
        case STRAT_RRD:
        case STRAT_PRIVACY_ABORT:
        case STRAT_NO_LOGS:
            // These strategies don't use the full workflow
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Strategy %d does not use workflow handler\n", 
                    __FUNCTION__, __LINE__, strategy);
            return NULL;
        
        default:
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Invalid strategy: %d\n", 
                    __FUNCTION__, __LINE__, strategy);
            return NULL;
    }
}

int execute_strategy_workflow(RuntimeContext* ctx, SessionState* session)
{
    if (!ctx || !session) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // Verify context has valid data
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Context check: ctx=%p, MAC='%s', device_type='%s'\n",
            __FUNCTION__, __LINE__, (void*)ctx,
            ctx->device.mac_address,
            strlen(ctx->device.device_type) > 0 ? ctx->device.device_type : "(empty)");

    const StrategyHandler* handler = get_strategy_handler(session->strategy);
    if (!handler) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] No handler for strategy: %d\n", 
                __FUNCTION__, __LINE__, session->strategy);
        return -1;
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Starting workflow for strategy: %d\n", 
            __FUNCTION__, __LINE__, session->strategy);

    int ret = 0;
    bool upload_success = false;

    // Phase 1: Setup
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Phase 1: Setup\n", __FUNCTION__, __LINE__);
    
    if (handler->setup_phase) {
        ret = handler->setup_phase(ctx, session);
        if (ret != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Setup phase failed\n", __FUNCTION__, __LINE__);
            goto cleanup;
        }
    }

    // Phase 2: Archive
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Phase 2: Archive\n", __FUNCTION__, __LINE__);
    
    if (handler->archive_phase) {
        ret = handler->archive_phase(ctx, session);
        if (ret != 0) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                    "[%s:%d] Archive phase failed\n", __FUNCTION__, __LINE__);
            goto cleanup;
        }
    }

    // Phase 3: Upload
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Phase 3: Upload\n", __FUNCTION__, __LINE__);
    
    if (handler->upload_phase) {
        ret = handler->upload_phase(ctx, session);
        if (ret == 0) {
            upload_success = true;
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
                    "[%s:%d] Upload phase succeeded\n", __FUNCTION__, __LINE__);
        } else {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Upload phase failed\n", __FUNCTION__, __LINE__);
        }
    }

cleanup:
    // Phase 4: Cleanup (always runs)
    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Phase 4: Cleanup\n", __FUNCTION__, __LINE__);
    
    if (handler->cleanup_phase) {
        int cleanup_ret = handler->cleanup_phase(ctx, session, upload_success);
        if (cleanup_ret != 0) {
            RDK_LOG(RDK_LOG_WARN, LOG_UPLOADSTB, 
                    "[%s:%d] Cleanup phase failed\n", __FUNCTION__, __LINE__);
            // Don't override ret if upload already failed
            if (ret == 0) {
                ret = cleanup_ret;
            }
        }
    }

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOADSTB, 
            "[%s:%d] Workflow complete. Result: %d, Upload success: %d\n", 
            __FUNCTION__, __LINE__, ret, upload_success);

    return ret;
}

