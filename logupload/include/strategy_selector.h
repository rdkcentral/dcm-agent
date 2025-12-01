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
 * @file strategy_selector.h
 * @brief Upload strategy selection logic
 *
 * This module implements the strategy selection decision tree based on
 * runtime conditions as defined in the HLD.
 */

#ifndef STRATEGY_SELECTOR_H
#define STRATEGY_SELECTOR_H

#include "uploadstblogs_types.h"

/**
 * @brief Perform early return checks and determine strategy
 * @param ctx Runtime context
 * @return Selected Strategy
 *
 * Decision tree:
 * - RRD_FLAG == 1 → STRAT_RRD
 * - Privacy mode → STRAT_PRIVACY_ABORT
 * - No previous logs → STRAT_NO_LOGS
 * - TriggerType == 5 → STRAT_ONDEMAND
 * - DCM_FLAG == 0 → STRAT_NON_DCM
 * - UploadOnReboot == 1 && FLAG == 1 → STRAT_REBOOT
 * - Otherwise → STRAT_DCM
 */
Strategy early_checks(const RuntimeContext* ctx);

/**
 * @brief Check if privacy mode is enabled
 * @param ctx Runtime context
 * @return true if privacy mode enabled
 */
bool is_privacy_mode(const RuntimeContext* ctx);

/**
 * @brief Check if previous logs directory is empty
 * @param ctx Runtime context
 * @return true if no logs exist
 */
bool has_no_logs(const RuntimeContext* ctx);

/**
 * @brief Decide upload paths (primary and fallback)
 * @param ctx Runtime context
 * @param session Session state to populate with path decisions
 */
void decide_paths(const RuntimeContext* ctx, SessionState* session);

#endif /* STRATEGY_SELECTOR_H */
