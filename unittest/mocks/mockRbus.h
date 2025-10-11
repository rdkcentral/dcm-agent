/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
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

#pragma once

#include <gtest/gtest.h>
/* --------- RBUS MACROS ------------*/
typedef enum _rbusError
{
    RBUS_ERROR_SUCCESS,
    RBUS_ERROR_NOT_INITIALIZED,
    RBUS_ERROR_BUS_ERROR,
} rbusError_t;

char const * rbusError_ToString(rbusError_t e);

struct _rbusHandle
{
};

typedef struct _rbusHandle *rbusHandle_t;

struct _rbusObject
{
};
typedef struct _rbusObject *rbusObject_t;

struct _rbusValue
{
};
typedef struct _rbusValue *rbusValue_t;

typedef void (*rbusMethodAsyncRespHandler_t)(rbusHandle_t handle, char const *methodName, rbusError_t error, rbusObject_t params);
