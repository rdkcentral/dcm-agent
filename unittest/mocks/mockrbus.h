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

//#pragma once
//#ifndef MOCK_RBUS_H
//#define MOCK_RBUS_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdint.h>
#include <stdbool.h>
#include "dcm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// RBUS Types and Constants
typedef enum {
    RBUS_ERROR_SUCCESS = 0,
    RBUS_ERROR_BUS_ERROR,
    RBUS_ERROR_INVALID_INPUT,
    RBUS_ERROR_NOT_INITIALIZED,
    RBUS_ERROR_OUT_OF_RESOURCES,
    RBUS_ERROR_DESTINATION_NOT_FOUND,
    RBUS_ERROR_DESTINATION_NOT_REACHABLE,
    RBUS_ERROR_DESTINATION_RESPONSE_FAILURE,
    RBUS_ERROR_INVALID_RESPONSE_FROM_DESTINATION,
    RBUS_ERROR_INVALID_OPERATION,
    RBUS_ERROR_INVALID_EVENT,
    RBUS_ERROR_INVALID_HANDLE,
    RBUS_ERROR_SESSION_ALREADY_EXIST,
    RBUS_ERROR_COMPONENT_NAME_DUPLICATE,
    RBUS_ERROR_ELEMENT_NAME_DUPLICATE,
    RBUS_ERROR_ELEMENT_NAME_MISSING,
    RBUS_ERROR_COMPONENT_PATH_MISMATCH,
    RBUS_ERROR_ELEMENT_PATH_MISMATCH,
    RBUS_ERROR_ACCESS_NOT_ALLOWED,
    RBUS_ERROR_INVALID_CONTEXT,
    RBUS_ERROR_TIMEOUT,
    RBUS_ERROR_ASYNC_RESPONSE,
    RBUS_ERROR_INVALID_METHOD,
    RBUS_ERROR_NOSUBSCRIBERS
} rbusError_t;

typedef enum {
    RBUS_ENABLED = 1,
    RBUS_DISABLED = 0
} rbusStatus_t;

typedef enum {
    RBUS_EVENT_ACTION_SUBSCRIBE = 1,
    RBUS_EVENT_ACTION_UNSUBSCRIBE = 2
} rbusEventSubAction_t;

typedef enum {
    RBUS_EVENT_GENERAL = 1,
    RBUS_EVENT_VALUE_CHANGED = 2
} rbusEventType_t;

typedef enum {
    RBUS_STRING = 1,
    RBUS_INT32 = 2,
    RBUS_BOOLEAN = 3,
    RBUS_UINT32 = 4,
    RBUS_BYTES = 5,
    RBUS_PROPERTY = 6,
    RBUS_OBJECT = 7,
    RBUS_DATETIME = 8,
    RBUS_SINGLE = 9,
    RBUS_DOUBLE = 10,
    RBUS_INT64 = 11,
    RBUS_UINT64 = 12
} rbusValueType_t;

typedef enum {
    RBUS_ELEMENT_TYPE_PROPERTY = 0,
    RBUS_ELEMENT_TYPE_TABLE = 1,
    RBUS_ELEMENT_TYPE_EVENT = 2,
    RBUS_ELEMENT_TYPE_METHOD = 3
} rbusElementType_t;

// Forward declarations
typedef void* rbusHandle_t;
typedef void* rbusValue_t;
typedef void* rbusObject_t;
typedef void* rbusFilter_t;

// Event structures
typedef struct {
    const char* name;
    rbusEventType_t type;
    rbusObject_t data;
} rbusEvent_t;

typedef struct {
    const char* eventName;
    void* userData;
} rbusEventSubscription_t;

// Callback function types
typedef void (*rbusEventHandler_t)(
    rbusHandle_t handle,
    rbusEvent_t const* event,
    rbusEventSubscription_t* subscription
);

typedef void (*rbusEventSubAsyncHandler_t)(
    rbusHandle_t handle,
    rbusEventSubscription_t* subscription,
    rbusError_t error
);

typedef rbusError_t (*rbusEventSubHandler_t)(
    rbusHandle_t handle,
    rbusEventSubAction_t action,
    const char* eventName,
    rbusFilter_t filter,
    int32_t interval,
    BOOL* autoPublish
);

// Data element structure
typedef struct {
    char* name;
    rbusElementType_t type;
    struct {
        void* getHandler;
        void* setHandler;
        void* tableAddRowHandler;
        void* tableRemoveRowHandler;
        rbusEventSubHandler_t eventSubHandler;
        void* methodHandler;
    } cbTable;
} rbusDataElement_t;

#ifdef __cplusplus
}
#endif

// Mock class for RBUS API functions only
class MockRBus {
public:
    // Core RBUS functions
    MOCK_METHOD(rbusStatus_t, rbus_checkStatus, (), ());
    MOCK_METHOD(rbusError_t, rbus_open, (rbusHandle_t* handle, const char* componentName), ());
    MOCK_METHOD(rbusError_t, rbus_close, (rbusHandle_t handle), ());
    MOCK_METHOD(rbusError_t, rbus_get, (rbusHandle_t handle, const char* paramName, rbusValue_t* paramValue), ());
    MOCK_METHOD(rbusError_t, rbus_regDataElements, (rbusHandle_t handle, int numElements, rbusDataElement_t* elements), ());
    MOCK_METHOD(rbusError_t, rbus_unregDataElements, (rbusHandle_t handle, int numElements, rbusDataElement_t* elements), ());
    
    // Event functions
    MOCK_METHOD(rbusError_t, rbusEvent_SubscribeAsync, 
                (rbusHandle_t handle, const char* eventName, rbusEventHandler_t handler, 
                 rbusEventSubAsyncHandler_t asyncHandler, void* userData, int timeout), ());
    MOCK_METHOD(rbusError_t, rbusEvent_Unsubscribe, (rbusHandle_t handle, const char* eventName), ());
    MOCK_METHOD(rbusError_t, rbusEvent_Publish, (rbusHandle_t handle, rbusEvent_t* event), ());
    
    // Value functions
    MOCK_METHOD(void, rbusValue_Init, (rbusValue_t* value), ());
    MOCK_METHOD(void, rbusValue_Release, (rbusValue_t value), ());
    MOCK_METHOD(rbusError_t, rbusValue_SetString, (rbusValue_t value, const char* str), ());
    MOCK_METHOD(const char*, rbusValue_GetString, (rbusValue_t value, int* len), ());
    MOCK_METHOD(char*, rbusValue_ToString, (rbusValue_t value, int* len, int radix), ());
    MOCK_METHOD(rbusValueType_t, rbusValue_GetType, (rbusValue_t value), ());
    
    // Object functions
    MOCK_METHOD(void, rbusObject_Init, (rbusObject_t* object, const char* name), ());
    MOCK_METHOD(void, rbusObject_Release, (rbusObject_t object), ());
    MOCK_METHOD(rbusError_t, rbusObject_SetValue, (rbusObject_t object, const char* name, rbusValue_t value), ());
    MOCK_METHOD(rbusValue_t, rbusObject_GetValue, (rbusObject_t object, const char* name), ());
};

#ifdef __cplusplus
extern "C" {
#endif

// C wrapper function declarations - RBUS functions only
rbusStatus_t rbus_checkStatus(void);
rbusError_t rbus_open(rbusHandle_t* handle, const char* componentName);
rbusError_t rbus_close(rbusHandle_t handle);
rbusError_t rbus_get(rbusHandle_t handle, const char* paramName, rbusValue_t* paramValue);
rbusError_t rbus_regDataElements(rbusHandle_t handle, int numElements, rbusDataElement_t* elements);
rbusError_t rbus_unregDataElements(rbusHandle_t handle, int numElements, rbusDataElement_t* elements);

rbusError_t rbusEvent_SubscribeAsync(rbusHandle_t handle, const char* eventName, 
                                     rbusEventHandler_t handler, rbusEventSubAsyncHandler_t asyncHandler, 
                                     void* userData, int timeout);
rbusError_t rbusEvent_Unsubscribe(rbusHandle_t handle, const char* eventName);
rbusError_t rbusEvent_Publish(rbusHandle_t handle, rbusEvent_t* event);

void rbusValue_Init(rbusValue_t* value);
void rbusValue_Release(rbusValue_t value);
rbusError_t rbusValue_SetString(rbusValue_t value, const char* str);
const char* rbusValue_GetString(rbusValue_t value, int* len);
char* rbusValue_ToString(rbusValue_t value, int* len, int radix);
rbusValueType_t rbusValue_GetType(rbusValue_t value);

void rbusObject_Init(rbusObject_t* object, const char* name);
void rbusObject_Release(rbusObject_t object);
rbusError_t rbusObject_SetValue(rbusObject_t object, const char* name, rbusValue_t value);
rbusValue_t rbusObject_GetValue(rbusObject_t object, const char* name);

// Global mock instance
extern MockRBus* g_mockRBus;

// Mock control functions
void mock_rbus_reset();
void mock_rbus_set_global_mock(MockRBus* mockRBus);
void mock_rbus_clear_global_mock();

// Helper functions for triggering callbacks in tests
void mock_rbus_trigger_event_callback(rbusEventHandler_t handler, rbusHandle_t handle, 
                                      rbusEvent_t* event, rbusEventSubscription_t* subscription);
void mock_rbus_trigger_async_callback(rbusEventSubAsyncHandler_t handler, rbusHandle_t handle, 
                                      rbusEventSubscription_t* subscription, rbusError_t error);

// Test utility functions
void mock_rbus_set_string_value(const char* value);
rbusHandle_t mock_rbus_get_mock_handle();
rbusValue_t mock_rbus_create_string_value(const char* str);
rbusObject_t mock_rbus_create_object(const char* name);

#ifdef __cplusplus
}
#endif
