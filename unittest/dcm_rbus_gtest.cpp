/**
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <stdio.h>
#include <climits>
#include <cerrno>
#include <fstream>
#include "./mocks/mockrbus.h"
//#include "./mocks/mockrbus.cpp"

extern "C" {
VOID get_rbusProcConf(rbusHandle_t handle, rbusEvent_t const* event, rbusEventSubscription_t* subscription);
void get_rbusAsyncSubCB(rbusHandle_t handle, rbusEventSubscription_t* subscription, rbusError_t error);
VOID get_rbusSetConf(rbusHandle_t handle, rbusEvent_t const* event, rbusEventSubscription_t* subscription);
}

//#include "../dcm_utils.c"
#include "dcm_types.h"
#include "dcm_rbus.c"
#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "dcm_cronparse_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256


using namespace testing;
using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::StrEq;


void CreateFile(const char* filename, const char* content) {
    std::ofstream ofs(filename);
    ofs << content;
}

void RemoveFile(const char* filename) {
    std::remove(filename);
}

void CreateDirectory(const char* dirname) {
    mkdir(dirname, 0755);
}

void RemoveDirectory(const char* dirname) {
    rmdir(dirname);
}

class DcmRbusTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
    }
    
    void TearDown() override {
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
    MockRBus* mockRBus;
};

TEST_F(DcmRbusTest, dcmRbusInit_Success) {
    void* handle = nullptr;
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
       .WillOnce(Return(RBUS_ENABLED));
    
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_SUCCESS)));
    
    int result = dcmRbusInit(&handle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(handle, nullptr);
    
    // Cleanup
    if (handle) {
        EXPECT_CALL(*mockRBus, rbusEvent_Unsubscribe(_, _))
            .Times(2)
            .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
        EXPECT_CALL(*mockRBus, rbus_unregDataElements(_, _, _))
            .WillOnce(Return(RBUS_ERROR_SUCCESS));
        EXPECT_CALL(*mockRBus, rbus_close(_))
            .WillOnce(Return(RBUS_ERROR_SUCCESS));
        
        dcmRbusUnInit(handle);
    }
}

TEST_F(DcmRbusTest, dcmRbusInit_rbuscheckstatus_failure) {
    void* handle = nullptr;
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
       .WillOnce(Return(RBUS_DISABLED));
    
    int result = dcmRbusInit(&handle);
    
    EXPECT_EQ(result, DCM_FAILURE);
    
}

TEST_F(DcmRbusTest, dcmRbusInit_rbusopen_failure) {
    void* handle = nullptr;
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
       .WillOnce(Return(RBUS_ENABLED));
    
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_BUS_ERROR)));
    int result = dcmRbusInit(&handle);
    
    EXPECT_EQ(result, DCM_FAILURE);
    
}

TEST_F(DcmRbusTest, dcmRbusSendEvent_Success) {
    // Setup mock DCM handle
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbusValue_Init(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusValue_SetString(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusObject_Init(_, _))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_SetValue(_, _, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusEvent_Publish(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_Release(_))
        .Times(1);
    
    // Set global event subscription flag
    extern int g_eventsub;
    g_eventsub = 1;
    
    int result = dcmRbusSendEvent(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
}

TEST_F(DcmRbusTest, SendEvent_NullHandle_Failure) {
    INT32 result = dcmRbusSendEvent(NULL);
    
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmRbusTest, SendEvent_NullRbusHandle_Failure) {
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = NULL;
    
    INT32 result = dcmRbusSendEvent(&dcmHandle);
    
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmRbusTest, SendEvent_rbusValueInit_Called_rbusEventPublishFails_Failure) {
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    EXPECT_CALL(*mockRBus, rbusValue_Init(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusValue_SetString(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusObject_Init(_, _))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_SetValue(_, _, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusEvent_Publish(_, _))
        .WillOnce(Return(RBUS_ERROR_BUS_ERROR)); // Publish fails
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_Release(_))
        .Times(1);
    
    INT32 result = dcmRbusSendEvent(&dcmHandle);
    
    EXPECT_EQ(result, DCM_FAILURE);
}


TEST_F(DcmRbusTest, SubscribeEvents_AllSubscriptionsSucceed_Success) {
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    strcpy(dcmHandle.confPath, "/tmp/test.conf");
    dcmHandle.eventSub = 0;
    dcmHandle.schedJob = 0;
    // First subscription: DCM_RBUS_SETCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  
        _,  
        _,  
        &dcmHandle,
        0))
        .Times(2)
        .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
    /*
    // Second subscription: DCM_RBUS_PROCCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  
        _, 
        _,  
        &dcmHandle,
        0))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    */
    // Register data elements for reload event
    EXPECT_CALL(*mockRBus, rbus_regDataElements(
        dcmHandle.pRbusHandle,
        1,
        _))  // &g_dataElements
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    INT32 result = dcmRbusSubscribeEvents(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
}
TEST_F(DcmRbusTest, SubscribeEvents_rbus_regDataElements_failure) {
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    // First subscription: DCM_RBUS_SETCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  
        _,  
        _,  
        &dcmHandle,
        0))
        .Times(2)
        .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
    // Register data elements for reload event
    EXPECT_CALL(*mockRBus, rbus_regDataElements(
        dcmHandle.pRbusHandle,
        1,
        _))  // &g_dataElements
        .WillOnce(Return(RBUS_ERROR_BUS_ERROR));
    EXPECT_CALL(*mockRBus, rbusEvent_Unsubscribe(_, _))
        .Times(2)
        .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
    
    INT32 result = dcmRbusSubscribeEvents(&dcmHandle);
    
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmRbusTest, SubscribeEvents_secondSubscription_failure) {
    InSequence seq;
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    // First subscription: DCM_RBUS_SETCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  
        _, 
        _,  
        &dcmHandle,
        0))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    // Second subscription: DCM_RBUS_PROCCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  
        _, 
        _,  
        &dcmHandle,
        0))
        .WillOnce(Return(RBUS_ERROR_BUS_ERROR));
    EXPECT_CALL(*mockRBus, rbusEvent_Unsubscribe(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    INT32 result = dcmRbusSubscribeEvents(&dcmHandle);

    
    EXPECT_EQ(result, DCM_FAILURE);
}
TEST_F(DcmRbusTest, SubscribeEvents_FirstSubscription_failure) {
    InSequence seq;
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    // First subscription: DCM_RBUS_SETCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  
        _, 
        _,  
        &dcmHandle,
        0))
        .WillOnce(Return(RBUS_ERROR_BUS_ERROR));
    INT32 result = dcmRbusSubscribeEvents(&dcmHandle);

    
    EXPECT_EQ(result, DCM_FAILURE);
}
TEST_F(DcmRbusTest, GetT2Version_ValidInputs_Success) {
    rbusValue_t mockValue = mock_rbus_create_string_value("2.1.5");
    char versionBuffer[256];
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    memset(versionBuffer, 0, sizeof(versionBuffer));
    EXPECT_CALL(*mockRBus, rbus_get(
        dcmHandle.pRbusHandle,
        _,  // DCM_RBUS_T2_VERSION
        _))
        .WillOnce(DoAll(SetArgPointee<2>(mockValue), Return(RBUS_ERROR_SUCCESS)));
    
    EXPECT_CALL(*mockRBus, rbusValue_GetType(mockValue))
        .WillOnce(Return(RBUS_STRING));
    
    EXPECT_CALL(*mockRBus, rbusValue_ToString(mockValue, NULL, 0))
        .WillOnce(Return(strdup("2.1.5")));
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(mockValue))
        .Times(1);
    
    INT32 result = dcmRbusGetT2Version(&dcmHandle, versionBuffer);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_STREQ(versionBuffer, "2.1.5");
}
/*
TEST_F(DcmRbusTest, ProcConf_ValidInputs_SetsScheduleJobFlag) {
    // Verify initial state
   // EXPECT_EQ(dcmRbusHandle->schedJob, 0);
    DCMRBusHandle* dcmRbusHandle;
    rbusEvent_t testEvent;
    rbusEventSubscription_t testSubscription;
    
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    dcmRbusHandle->pRbusHandle = mockHandle;
    dcmRbusHandle->eventSub = 1;
    dcmRbusHandle->schedJob = 0; // Initially not scheduled
    strcpy(dcmRbusHandle->confPath, "/etc/dcm.conf");
    
        // Initialize event structure
    memset(&testEvent, 0, sizeof(rbusEvent_t));
    testEvent.name = "Device.X_RDKCENTRAL-COM_T2.ProcessConfig";
    testEvent.type = RBUS_EVENT_GENERAL;
    testEvent.data = nullptr;
        
        // Initialize subscription structure
    memset(&testSubscription, 0, sizeof(rbusEventSubscription_t));
    testSubscription.eventName = "Device.X_RDKCENTRAL-COM_T2.ProcessConfig";
    testSubscription.userData = nullptr;
    
    // Call the function
    get_rbusProcConf(mockHandle, &testEvent, &testSubscription);
    
    // Verify schedJob flag is set
    EXPECT_EQ(dcmRbusHandle->schedJob, 1);
}
*/

class RbusProcConfTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize mock RBUS handle
        mockHandle = (rbusHandle_t)0x12345678;
        
        // Initialize DCM RBUS handle
        dcmRbusHandle = (DCMRBusHandle*)malloc(sizeof(DCMRBusHandle));
        ASSERT_NE(dcmRbusHandle, nullptr);
        memset(dcmRbusHandle, 0, sizeof(DCMRBusHandle));
        
        dcmRbusHandle->pRbusHandle = mockHandle;
        dcmRbusHandle->eventSub = 0;
        dcmRbusHandle->schedJob = 0; // Initially not scheduled
        strcpy(dcmRbusHandle->confPath, "/etc/dcm.conf");
        
        // Initialize event structure
        memset(&testEvent, 0, sizeof(rbusEvent_t));
        testEvent.name = "Device.X_RDKCENTRAL-COM_T2.ProcessConfig";
        testEvent.type = RBUS_EVENT_GENERAL;
        testEvent.data = nullptr; // ProcConf doesn't use event data
        
        // Initialize subscription structure
        memset(&testSubscription, 0, sizeof(rbusEventSubscription_t));
        testSubscription.eventName = "Device.X_RDKCENTRAL-COM_T2.ProcessConfig";
        testSubscription.userData = dcmRbusHandle;
        //testSubscription.handler = rbusProcConf;

        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
    }
    
    void TearDown() override {
        if (dcmRbusHandle) {
            free(dcmRbusHandle);
            dcmRbusHandle = nullptr;
        }
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
    rbusHandle_t mockHandle;
    DCMRBusHandle* dcmRbusHandle;
    rbusEvent_t testEvent;
    rbusEventSubscription_t testSubscription;
    MockRBus* mockRBus;
};

// ==================== Valid Input Test Cases ====================

TEST_F(RbusProcConfTest, ProcConf_ValidInputs_SetsScheduleJobFlag) {
    // Verify initial state
    EXPECT_EQ(dcmRbusHandle->schedJob, 0);
    
    // Call the function
    get_rbusProcConf(mockHandle, &testEvent, &testSubscription);
    
    // Verify schedJob flag is set
    EXPECT_EQ(dcmRbusHandle->schedJob, 1);
}
TEST_F(RbusProcConfTest, ProcConf_NullEvent_ReturnsEarlyWithoutCrash) {
    // Store original schedJob value
    INT32 originalSchedJob = dcmRbusHandle->schedJob;
    
    // Call with NULL event
    get_rbusProcConf(mockHandle, nullptr, &testSubscription);
    
    // Verify schedJob is not modified
    EXPECT_EQ(dcmRbusHandle->schedJob, originalSchedJob);
}

TEST_F(RbusProcConfTest, ProcConf_NullSubscription_ReturnsEarlyWithoutCrash) {
    // Store original schedJob value
    INT32 originalSchedJob = dcmRbusHandle->schedJob;
    
    // Call with NULL subscription
    get_rbusProcConf(mockHandle, &testEvent, nullptr);
    
    // Verify schedJob is not modified
    EXPECT_EQ(dcmRbusHandle->schedJob, originalSchedJob);
}
TEST_F(RbusProcConfTest, ProcConf_NullUserData_ReturnsEarlyWithoutCrash) {
    // Set userData to NULL
    testSubscription.userData = nullptr;
    
    // Call function - should return early due to NULL userData
    EXPECT_NO_THROW(get_rbusProcConf(mockHandle, &testEvent, &testSubscription));
}



TEST_F(RbusProcConfTest, rbusAsyncSubCB_subscrption_success) {
    rbusError_t error = RBUS_ERROR_SUCCESS;
    EXPECT_EQ(dcmRbusHandle->eventSub, 0);
    // Call the function
    get_rbusAsyncSubCB(mockHandle, &testSubscription, error);
    EXPECT_EQ(dcmRbusHandle->eventSub, 1);
    
}
TEST_F(RbusProcConfTest, rbusAsyncSubCB_subscrption_event_failure)
{
    rbusError_t error = RBUS_ERROR_BUS_ERROR;
    EXPECT_EQ(dcmRbusHandle->eventSub, 0);
    // Call the function
    get_rbusAsyncSubCB(mockHandle, &testSubscription, error);
    EXPECT_EQ(dcmRbusHandle->eventSub, 0);
    
}

TEST_F(RbusProcConfTest, rbusAsyncSubCB_subscrption_null) 
{
    rbusError_t error = RBUS_ERROR_SUCCESS;
    // Call the function
    get_rbusAsyncSubCB(mockHandle, nullptr, error);
    
}

TEST_F( RbusProcConfTest, rbusAsyncSubCB_with_userdata_null) 
{
    rbusError_t error = RBUS_ERROR_SUCCESS; 
    testSubscription.userData = nullptr;
    get_rbusAsyncSubCB(mockHandle, &testSubscription, error);
    
}

TEST_F(RbusProcConfTest, rbusSetConf_success) 
{    
    rbusValue_t mockConfigValue;
    const char* newConfigPath = "/etc/test.conf";
    
    // Setup expectations - rbusObject_GetValue returns rbusValue_t
    EXPECT_CALL(*mockRBus, rbusObject_GetValue(testEvent.data, DCM_SET_CONFIG))
        .WillOnce(Return(mockConfigValue));
    
   // EXPECT_CALL(*mockRBus, rbusValue_GetString(mockConfigValue, nullptr))
    //    .WillOnce(Return(newConfigPath));
    get_rbusSetConf(mockHandle, &testEvent, &testSubscription);
    
}

TEST_F(RbusProcConfTest, rbusSetConf_event_handler_null) 
{
    get_rbusSetConf(mockHandle, nullptr, &testSubscription);    
}
TEST_F(RbusProcConfTest, rbusSetConf_subscription_null) 
{
    get_rbusSetConf(mockHandle, &testEvent, nullptr);    
}
TEST_F(RbusProcConfTest, rbusSetConf_with_userdata_null) 
{
    testSubscription.userData = nullptr;
    get_rbusSetConf(mockHandle, &testEvent, &testSubscription);
}

/*
#include <gtest/gtest.h>
#include <gmock/gmock.h>


extern "C" {
#include "dcm_rbus.h"
#include "dcm_types.h"
//#include "mockrbus.cpp"
}
//#include "dcm_rbus.c"
#include "./mocks/mockrbus.h‎"
#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "dcm_cronparse_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256


using namespace testing;
using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::StrEq;


class DcmRbusTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
    }
    
    void TearDown() override {
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
    MockRBus* mockRBus;
};

TEST_F(DcmRbusTest, dcmRbusInit_Success) {
    void* handle = nullptr;
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_ENABLED));
    
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_SUCCESS)));
    
    int result = dcmRbusInit(&handle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(handle, nullptr);
    
    // Cleanup
    if (handle) {
        EXPECT_CALL(*mockRBus, rbusEvent_Unsubscribe(_, _))
            .Times(2)
            .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
        EXPECT_CALL(*mockRBus, rbus_unregDataElements(_, _, _))
            .WillOnce(Return(RBUS_ERROR_SUCCESS));
        EXPECT_CALL(*mockRBus, rbus_close(_))
            .WillOnce(Return(RBUS_ERROR_SUCCESS));
        
        dcmRbusUnInit(handle);
    }
}

TEST_F(DcmRbusTest, dcmRbusSendEvent_Success) {
    // Setup mock DCM handle
    DCMRBusHandle dcmHandle;
    dcmHandle.pRbusHandle = mock_rbus_get_mock_handle();
    
    EXPECT_CALL(*mockRBus, rbusValue_Init(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusValue_SetString(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusObject_Init(_, _))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_SetValue(_, _, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusEvent_Publish(_, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(_))
        .Times(1);
    
    EXPECT_CALL(*mockRBus, rbusObject_Release(_))
        .Times(1);
    
    // Set global event subscription flag
    extern int g_eventsub;
    g_eventsub = 1;
    
    int result = dcmRbusSendEvent(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
}
*/
GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    char buffer[GTEST_REPORT_FILEPATH_SIZE];

    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    memset( buffer, 0, GTEST_REPORT_FILEPATH_SIZE );

    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    //testing::Mock::AllowLeak(mock);
    cout << "Starting DCM GTEST ===================>" << endl;
    return RUN_ALL_TESTS();
}
