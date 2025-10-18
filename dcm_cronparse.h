/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:

 * Copyright 2024 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DCM_CRONPARSE_H_
#define _DCM_CRONPARSE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef GTEST_ENABLE
#include "dcm_types.h"
#endif
/**
 * Parsed cron expression
 */
typedef struct {
    UINT8 seconds[8];
    UINT8 minutes[8];
    UINT8 hours[3];
    UINT8 days_of_week[1];
    UINT8 days_of_month[4];
    UINT8 months[2];
} dcmCronExpr;

INT32 dcmCronParseExp(const INT8* expression, dcmCronExpr* target);

time_t dcmCronParseGetNext(dcmCronExpr* expr, time_t date);

#ifdef __cplusplus
}
#endif
#endif //_DCM_CRONPARSE_H_


