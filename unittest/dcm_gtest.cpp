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
 
  VOID (*getdcmRunJobs(void)) (const INT8*, VOID);
  #include "dcm.c"
  #include "dcm.h"

} 
//#include "../dcm_utils.c"

#include "dcm_types.h"
#include "dcm_rbus.c"
#include "dcm_parseconf.c"
#include "dcm_schedjob.c"
#include "dcm_cronparse.c"
#include "dcm_utils.c"
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


class DcmDaemonMainInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
        
        // Initialize DCM handle
        memset(&dcmHandle, 0, sizeof(DCMDHandle));
        dcmHandle.isDCMRunning = false;
        
    }
    
    void TearDown() override {
        mock_rbus_clear_global_mock();
        delete mockRBus;
        
        // Cleanup
        cleanupDCMHandle();
    }
    
    // Helper: create a file with given content
    void CreateFile(const char* filename, const char* content) {
        std::ofstream ofs(filename);
        ofs << content;
    }

    void RemoveFile(const char* filename) {
        std::remove(filename);
    }
    
    void cleanupDCMHandle() {
        if (dcmHandle.pExecBuff) {
            free(dcmHandle.pExecBuff);
            dcmHandle.pExecBuff = NULL;
        }
        if (dcmHandle.pDcmSetHandle) {
            dcmSettingsUnInit(dcmHandle.pDcmSetHandle);
        }
        if (dcmHandle.pRbusHandle) {
            dcmRbusUnInit(dcmHandle.pRbusHandle);
        }
        if (dcmHandle.pLogSchedHandle) {
            dcmSchedRemoveJob(dcmHandle.pLogSchedHandle);
        }
        if (dcmHandle.pDifdSchedHandle) {
            dcmSchedRemoveJob(dcmHandle.pDifdSchedHandle);
        }
        dcmSchedUnInit();
    }
    
    MockRBus* mockRBus;
    DCMDHandle dcmHandle;
    const char* pidFilePath;
};

// ==================== Positive Test Cases ====================

TEST_F(DcmDaemonMainInitTest, MainInit_AllComponentsInitializeSuccessfully_Success) {
    // Setup successful RBUS mocks
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    RemoveFile(DCM_PID_FILE);
    
    // RBUS initialization sequence
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_ENABLED));
    
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_SUCCESS)));
    
    // T2 version retrieval
    rbusValue_t mockValue = mock_rbus_create_string_value("2.1.5");
    EXPECT_CALL(*mockRBus, rbus_get(mockHandle, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(mockValue), Return(RBUS_ERROR_SUCCESS)));
    
    EXPECT_CALL(*mockRBus, rbusValue_GetType(mockValue))
        .WillOnce(Return(RBUS_STRING));
    
    EXPECT_CALL(*mockRBus, rbusValue_ToString(mockValue, NULL, 0))
        .WillOnce(Return(strdup("2.1.5")));
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(mockValue))
        .Times(1);
    
    // Event subscription
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(_, _, _, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbus_regDataElements(_, _, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_FALSE(dcmHandle.isDCMRunning);
    EXPECT_NE(dcmHandle.pDcmSetHandle, nullptr);
    EXPECT_NE(dcmHandle.pRbusHandle, nullptr);
    EXPECT_NE(dcmHandle.pExecBuff, nullptr);
    EXPECT_NE(dcmHandle.pLogSchedHandle, nullptr);
    EXPECT_NE(dcmHandle.pDifdSchedHandle, nullptr);
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    char buffer[GTEST_REPORT_FILEPATH_SIZE];

    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    memset( buffer, 0, GTEST_REPORT_FILEPATH_SIZE );

    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    cout << "Starting DCM GTEST ===================>" << endl;
    return RUN_ALL_TESTS();
}
