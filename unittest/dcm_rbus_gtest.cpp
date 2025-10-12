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

/*extern "C" {
//#include "dcm_types.h"
//#include "dcm_parseconf.h"

}*/
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

class DcmRbusSubscribeEventsTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
        
        // Setup valid DCM handle
        dcmHandle.pRbusHandle = (rbusHandle_t)0x12345678;
        strcpy(dcmHandle.confPath, "/tmp/test.conf");
        dcmHandle.eventSub = 0;
        dcmHandle.schedJob = 0;
    }
    
    void TearDown() override {
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
    MockRBus* mockRBus;
    DCMRBusHandle dcmHandle;
};

// ==================== Positive Test Cases ====================

TEST_F(DcmRbusSubscribeEventsTest, SubscribeEvents_AllSubscriptionsSucceed_Success) {
    InSequence seq;
    
    // First subscription: DCM_RBUS_SETCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  // DCM_RBUS_SETCONF_EVENT
        _,  // rbusSetConf callback
        _,  // rbusAsyncSubCB callback
        &dcmHandle,
        0))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    // Second subscription: DCM_RBUS_PROCCONF_EVENT
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(
        dcmHandle.pRbusHandle,
        _,  // DCM_RBUS_PROCCONF_EVENT
        _,  // rbusProcConf callback
        _,  // rbusAsyncSubCB callback
        &dcmHandle,
        0))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    // Register data elements for reload event
    EXPECT_CALL(*mockRBus, rbus_regDataElements(
        dcmHandle.pRbusHandle,
        1,
        _))  // &g_dataElements
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    INT32 result = dcmRbusSubscribeEvents(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
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
