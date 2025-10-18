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

#include "mockrbus.h"
#include <cstring>
#include <cstdlib>
#include <iostream>

// Global mock instance
MockRBus* g_mockRBus = nullptr;

// Mock value and object structures for internal use
struct MockRBusValue {
    rbusValueType_t type;
    char stringValue[256];
    int intValue;
    bool boolValue;
    double doubleValue;
    uint32_t uint32Value;
    int32_t int32Value;
};

struct MockRBusObject {
    char name[256];
    struct {
        char key[64];
        MockRBusValue* value;
    } properties[10];
    int propertyCount;
};

// Static variables to track mock state
static char g_mockStringBuffer[512] = "mock_default_value";
static rbusHandle_t g_mockHandle = (rbusHandle_t)0x12345678;
static int g_mockCallCount = 0;

// Stored callback pointers for test triggering
static rbusEventHandler_t g_storedEventHandler = nullptr;
static rbusEventSubAsyncHandler_t g_storedAsyncHandler = nullptr;
static void* g_storedUserData = nullptr;

// Mock control functions
void mock_rbus_reset() {
    g_mockCallCount = 0;
    strcpy(g_mockStringBuffer, "mock_default_value");
    g_storedEventHandler = nullptr;
    g_storedAsyncHandler = nullptr;
    g_storedUserData = nullptr;
}

void mock_rbus_set_global_mock(MockRBus* mockRBus) {
    g_mockRBus = mockRBus;
}

void mock_rbus_clear_global_mock() {
    g_mockRBus = nullptr;
}

void mock_rbus_set_string_value(const char* value) {
    if (value) {
        strncpy(g_mockStringBuffer, value, sizeof(g_mockStringBuffer) - 1);
        g_mockStringBuffer[sizeof(g_mockStringBuffer) - 1] = '\0';
    }
}

rbusHandle_t mock_rbus_get_mock_handle() {
    return g_mockHandle;
}

rbusValue_t mock_rbus_create_string_value(const char* str) {
    MockRBusValue* mockValue = (MockRBusValue*)malloc(sizeof(MockRBusValue));
    if (mockValue) {
        memset(mockValue, 0, sizeof(MockRBusValue));
        mockValue->type = RBUS_STRING;
        if (str) {
            strncpy(mockValue->stringValue, str, sizeof(mockValue->stringValue) - 1);
            mockValue->stringValue[sizeof(mockValue->stringValue) - 1] = '\0';
        }
    }
    return (rbusValue_t)mockValue;
}

rbusObject_t mock_rbus_create_object(const char* name) {
    MockRBusObject* mockObject = (MockRBusObject*)malloc(sizeof(MockRBusObject));
    if (mockObject) {
        memset(mockObject, 0, sizeof(MockRBusObject));
        if (name) {
            strncpy(mockObject->name, name, sizeof(mockObject->name) - 1);
            mockObject->name[sizeof(mockObject->name) - 1] = '\0';
        }
    }
    return (rbusObject_t)mockObject;
}

// Helper functions for triggering callbacks
void mock_rbus_trigger_event_callback(rbusEventHandler_t handler, rbusHandle_t handle, 
                                      rbusEvent_t* event, rbusEventSubscription_t* subscription) {
    if (handler && event && subscription) {
        handler(handle, event, subscription);
    }
}

void mock_rbus_trigger_async_callback(rbusEventSubAsyncHandler_t handler, rbusHandle_t handle, 
                                      rbusEventSubscription_t* subscription, rbusError_t error) {
    if (handler && subscription) {
        handler(handle, subscription, error);
    }
}

extern "C" {

// RBUS API Mock Implementations
rbusStatus_t rbus_checkStatus(void) {
    if (g_mockRBus) {
        return g_mockRBus->rbus_checkStatus();
    }
    return RBUS_ENABLED; // Default success for basic functionality
}

rbusError_t rbus_open(rbusHandle_t* handle, const char* componentName) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbus_open(handle, componentName);
    }
    // Default implementation
    if (handle) {
        *handle = g_mockHandle;
        return RBUS_ERROR_SUCCESS;
    }
    return RBUS_ERROR_INVALID_INPUT;
}

rbusError_t rbus_close(rbusHandle_t handle) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbus_close(handle);
    }
    return RBUS_ERROR_SUCCESS; // Default success
}

rbusError_t rbus_get(rbusHandle_t handle, const char* paramName, rbusValue_t* paramValue) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbus_get(handle, paramName, paramValue);
    }
    
    // Default implementation - create a mock value
    if (paramValue) {
        MockRBusValue* mockValue = (MockRBusValue*)malloc(sizeof(MockRBusValue));
        if (mockValue) {
            memset(mockValue, 0, sizeof(MockRBusValue));
            mockValue->type = RBUS_STRING;
            strcpy(mockValue->stringValue, g_mockStringBuffer);
            *paramValue = (rbusValue_t)mockValue;
            return RBUS_ERROR_SUCCESS;
        }
        return RBUS_ERROR_OUT_OF_RESOURCES;
    }
    return RBUS_ERROR_INVALID_INPUT;
}

rbusError_t rbus_regDataElements(rbusHandle_t handle, int numElements, rbusDataElement_t* elements) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbus_regDataElements(handle, numElements, elements);
    }
    return RBUS_ERROR_SUCCESS; // Default success
}

rbusError_t rbus_unregDataElements(rbusHandle_t handle, int numElements, rbusDataElement_t* elements) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbus_unregDataElements(handle, numElements, elements);
    }
    return RBUS_ERROR_SUCCESS; // Default success
}

// Event functions
rbusError_t rbusEvent_SubscribeAsync(rbusHandle_t handle, const char* eventName, 
                                     rbusEventHandler_t handler, rbusEventSubAsyncHandler_t asyncHandler, 
                                     void* userData, int timeout) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbusEvent_SubscribeAsync(handle, eventName, handler, asyncHandler, userData, timeout);
    }
    
    // Store callbacks for potential triggering in tests
    g_storedEventHandler = handler;
    g_storedAsyncHandler = asyncHandler;
    g_storedUserData = userData;
    
    // Default implementation - simulate successful subscription
    if (asyncHandler && userData) {
        // Create a mock subscription for the callback
        rbusEventSubscription_t subscription = {0};
        subscription.eventName = eventName;
        subscription.userData = userData;
        
        // Trigger the async callback with success
        asyncHandler(handle, &subscription, RBUS_ERROR_SUCCESS);
    }
    
    return RBUS_ERROR_SUCCESS;
}

rbusError_t rbusEvent_Unsubscribe(rbusHandle_t handle, const char* eventName) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbusEvent_Unsubscribe(handle, eventName);
    }
    return RBUS_ERROR_SUCCESS; // Default success
}

rbusError_t rbusEvent_Publish(rbusHandle_t handle, rbusEvent_t* event) {
    g_mockCallCount++;
    if (g_mockRBus) {
        return g_mockRBus->rbusEvent_Publish(handle, event);
    }
    return RBUS_ERROR_SUCCESS; // Default success
}

// Value functions
void rbusValue_Init(rbusValue_t* value) {
    if (g_mockRBus) {
        g_mockRBus->rbusValue_Init(value);
        return;
    }
    
    // Default implementation
    if (value) {
        MockRBusValue* mockValue = (MockRBusValue*)malloc(sizeof(MockRBusValue));
        if (mockValue) {
            memset(mockValue, 0, sizeof(MockRBusValue));
            mockValue->type = RBUS_STRING;
            *value = (rbusValue_t)mockValue;
        }
    }
}

void rbusValue_Release(rbusValue_t value) {
    if (g_mockRBus) {
        g_mockRBus->rbusValue_Release(value);
        return;
    }
    
    // Default implementation
    if (value) {
        free(value);
    }
}

rbusError_t rbusValue_SetString(rbusValue_t value, const char* str) {
    if (g_mockRBus) {
        return g_mockRBus->rbusValue_SetString(value, str);
    }
    
    // Default implementation
    if (value && str) {
        MockRBusValue* mockValue = (MockRBusValue*)value;
        mockValue->type = RBUS_STRING;
        strncpy(mockValue->stringValue, str, sizeof(mockValue->stringValue) - 1);
        mockValue->stringValue[sizeof(mockValue->stringValue) - 1] = '\0';
        return RBUS_ERROR_SUCCESS;
    }
    return RBUS_ERROR_INVALID_INPUT;
}

const char* rbusValue_GetString(rbusValue_t value, int* len) {
    /*if (g_mockRBus) {
        return g_mockRBus->rbusValue_GetString(value, len);
    }
    
    // Default implementation
    if (value) {
        MockRBusValue* mockValue = (MockRBusValue*)value;
        if (len) {
            *len = strlen(mockValue->stringValue);
        }
        return mockValue->stringValue;
    }*/
    return "Mockconfig";
}

char* rbusValue_ToString(rbusValue_t value, int* len, int radix) {
    (void)radix; // Unused parameter
    
    if (g_mockRBus) {
        return g_mockRBus->rbusValue_ToString(value, len, radix);
    }
    
    // Default implementation
    if (value) {
        MockRBusValue* mockValue = (MockRBusValue*)value;
        int strLen = strlen(mockValue->stringValue);
        char* result = (char*)malloc(strLen + 1);
        if (result) {
            strcpy(result, mockValue->stringValue);
            if (len) {
                *len = strLen;
            }
        }
        return result;
    }
    return nullptr;
}

rbusValueType_t rbusValue_GetType(rbusValue_t value) {
    if (g_mockRBus) {
        return g_mockRBus->rbusValue_GetType(value);
    }
    
    // Default implementation
    if (value) {
        MockRBusValue* mockValue = (MockRBusValue*)value;
        return mockValue->type;
    }
    return RBUS_STRING;
}

// Object functions
void rbusObject_Init(rbusObject_t* object, const char* name) {
    if (g_mockRBus) {
        g_mockRBus->rbusObject_Init(object, name);
        return;
    }
    
    // Default implementation
    if (object) {
        MockRBusObject* mockObject = (MockRBusObject*)malloc(sizeof(MockRBusObject));
        if (mockObject) {
            memset(mockObject, 0, sizeof(MockRBusObject));
            if (name) {
                strncpy(mockObject->name, name, sizeof(mockObject->name) - 1);
                mockObject->name[sizeof(mockObject->name) - 1] = '\0';
            }
            *object = (rbusObject_t)mockObject;
        }
    }
}

void rbusObject_Release(rbusObject_t object) {
    if (g_mockRBus) {
        g_mockRBus->rbusObject_Release(object);
        return;
    }
    
    // Default implementation
    if (object) {
        MockRBusObject* mockObject = (MockRBusObject*)object;
        // Release any stored values
        for (int i = 0; i < mockObject->propertyCount; i++) {
            if (mockObject->properties[i].value) {
                free(mockObject->properties[i].value);
            }
        }
        free(object);
    }
}

rbusError_t rbusObject_SetValue(rbusObject_t object, const char* name, rbusValue_t value) {
    if (g_mockRBus) {
        return g_mockRBus->rbusObject_SetValue(object, name, value);
    }
    
    // Default implementation
    if (object && name && value) {
        MockRBusObject* mockObject = (MockRBusObject*)object;
        MockRBusValue* mockValue = (MockRBusValue*)value;
        
        if (mockObject->propertyCount < 10) {
            strncpy(mockObject->properties[mockObject->propertyCount].key, name, 
                   sizeof(mockObject->properties[mockObject->propertyCount].key) - 1);
            mockObject->properties[mockObject->propertyCount].key[sizeof(mockObject->properties[mockObject->propertyCount].key) - 1] = '\0';
            
            // Create a copy of the value
            MockRBusValue* valueCopy = (MockRBusValue*)malloc(sizeof(MockRBusValue));
            if (valueCopy) {
                *valueCopy = *mockValue;
                mockObject->properties[mockObject->propertyCount].value = valueCopy;
                mockObject->propertyCount++;
                return RBUS_ERROR_SUCCESS;
            }
        }
    }
    return RBUS_ERROR_INVALID_INPUT;
}

rbusValue_t rbusObject_GetValue(rbusObject_t object, const char* name) {
    if (g_mockRBus) {
        return g_mockRBus->rbusObject_GetValue(object, name);
    }
    
    // Default implementation
    if (object && name) {
        MockRBusObject* mockObject = (MockRBusObject*)object;
        
        for (int i = 0; i < mockObject->propertyCount; i++) {
            if (strcmp(mockObject->properties[i].key, name) == 0) {
                // Return a copy of the stored value
                MockRBusValue* result = (MockRBusValue*)malloc(sizeof(MockRBusValue));
                if (result) {
                    *result = *(mockObject->properties[i].value);
                    return (rbusValue_t)result;
                }
            }
        }
    }
    
    // Return a default mock value if not found
    MockRBusValue* defaultValue = (MockRBusValue*)malloc(sizeof(MockRBusValue));
    if (defaultValue) {
        memset(defaultValue, 0, sizeof(MockRBusValue));
        defaultValue->type = RBUS_STRING;
        strcpy(defaultValue->stringValue, g_mockStringBuffer);
        return (rbusValue_t)defaultValue;
    }
    
    return nullptr;
}

} // extern "C"
