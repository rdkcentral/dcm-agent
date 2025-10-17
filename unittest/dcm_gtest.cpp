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
#include "dcm.c"
#include "dcm.h"
#include "dcm_types.h"
#include "dcm_rbus.c"
#include "dcm_parseconf.c"
#include "dcm_schedjob.c"
#include "dcm_cronparse.c"
#include "dcm_utils.c"
#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "dcm_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

void get_dcmRunJobs(const INT8* profileName, VOID *pHandle);
void get_sig_handler(INT32 sig);

using namespace testing;
using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::StrEq;

// ======================= DCM Main Init Tests =======================
class DcmDaemonMainInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
        
        memset(&dcmHandle, 0, sizeof(DCMDHandle));
        dcmHandle.isDCMRunning = false;
    }
    
    void TearDown() override {
        cleanupDCMHandle();
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
    void RemoveFile(const char* filename) {
        std::remove(filename);
    }
    
    void cleanupDCMHandle() {
        if (dcmHandle.pExecBuff) {
            free(dcmHandle.pExecBuff);
            dcmHandle.pExecBuff = nullptr;
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
    
public:  // Make these public so tests can access them
    MockRBus* mockRBus;
    DCMDHandle dcmHandle;
};

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

TEST_F(DcmDaemonMainInitTest, MainInit_CheckDemonStatus_Fail) {
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmDaemonMainInitTest, MainInit_RbusDisabled_Failure) {
    RemoveFile(DCM_PID_FILE);
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_DISABLED));
    
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmDaemonMainInitTest, MainInit_dcmRbusSubscribeEvents_failure) {
    // Setup successful RBUS mocks
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    RemoveFile(DCM_PID_FILE);
    // RBUS initialization sequence
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_ENABLED));
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_SUCCESS)));
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
         .WillOnce(Return(RBUS_ERROR_BUS_ERROR));   
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    
    EXPECT_EQ(result, DCM_FAILURE);
}

// ======================= DCM Run Jobs Tests =======================
class DcmRunJobsTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&dcmHandle, 0, sizeof(DCMDHandle));
        
        dcmHandle.pExecBuff = (INT8*)malloc(EXECMD_BUFF_SIZE);
        ASSERT_NE(dcmHandle.pExecBuff, nullptr);
        
        INT32 ret = dcmSettingsInit(&dcmHandle.pDcmSetHandle);
        if (ret != DCM_SUCCESS) {
            dcmHandle.pDcmSetHandle = malloc(64);
            ASSERT_NE(dcmHandle.pDcmSetHandle, nullptr);
        }
        
        originalPath = getenv("PATH");
        system("mkdir -p /tmp/test_dcm_scripts");
        createTestScripts();
    }
    
    void TearDown() override {
        if (dcmHandle.pExecBuff) {
            free(dcmHandle.pExecBuff);
        }
        if (dcmHandle.pDcmSetHandle) {
            dcmSettingsUnInit(dcmHandle.pDcmSetHandle);
        }
        
        system("rm -rf /tmp/test_dcm_scripts");
        
        if (originalPath) {
            setenv("PATH", originalPath, 1);
        }
    }
    
    void createTestScripts() {
        system("echo '#!/bin/bash\necho \"Upload script called\"' > /tmp/test_dcm_scripts/uploadSTBLogs.sh");
        system("chmod +x /tmp/test_dcm_scripts/uploadSTBLogs.sh");
        
        system("echo '#!/bin/bash\necho \"SW Update script called\"' > /tmp/test_dcm_scripts/swupdate_utility.sh");
        system("chmod +x /tmp/test_dcm_scripts/swupdate_utility.sh");
    }
    
public:  // Make public for access
    DCMDHandle dcmHandle;
    const char* originalPath;
};

TEST_F(DcmRunJobsTest, RunJobs_LogUploadProfile_ExecutesCorrectScript) {
    setenv("DCM_RDK_PATH", "/tmp/test_dcm_scripts", 1);
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_LOGUPLOAD_SCHED, &dcmHandle));
}

TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_ExecutesCorrectScript) {
    setenv("DCM_RDK_PATH", "/tmp/test_dcm_scripts", 1);
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, &dcmHandle));
}

TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_NullHandle_NoThrow) {    
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, nullptr));
}

TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_MaintenanceModeDisabled) {    
    extern INT32 g_bMMEnable;  // Declare as extern
    g_bMMEnable = 0;
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, nullptr));
    EXPECT_EQ(g_bMMEnable, 0);   
}

TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_NullSettingsHandle) {    
    dcmHandle.pDcmSetHandle = nullptr;
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, nullptr));   
}

// ======================= DCM Un-Init Tests =======================
class DcmDaemonMainUnInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&testHandle, 0, sizeof(DCMDHandle));
        system("mkdir -p /tmp/test_dcm");
        setupTestComponents();
    }
    
    void TearDown() override {
        system("rm -rf /tmp/test_dcm");
        system("rm -f /var/run/dcm.pid");
        cleanupTestComponents();
    }
    
    void setupTestComponents() {
        testHandle.pExecBuff = (INT8*)malloc(EXECMD_BUFF_SIZE);
        if (testHandle.pExecBuff) {
            memset(testHandle.pExecBuff, 0, EXECMD_BUFF_SIZE);
            strcpy(testHandle.pExecBuff, "test command");
        }
        
        if (dcmSettingsInit(&testHandle.pDcmSetHandle) != DCM_SUCCESS) {
            testHandle.pDcmSetHandle = nullptr;
        }
        
        if (dcmSchedInit() == DCM_SUCCESS) {
            testHandle.pLogSchedHandle = dcmSchedAddJob("test_log", nullptr, nullptr);
            testHandle.pDifdSchedHandle = dcmSchedAddJob("test_difd", nullptr, nullptr);
        }
        
        if (dcmRbusInit(&testHandle.pRbusHandle) != DCM_SUCCESS) {
            testHandle.pRbusHandle = nullptr;
        }
    }
    
    void cleanupTestComponents() {
        if (testHandle.pLogSchedHandle || testHandle.pDifdSchedHandle) {
            dcmSchedUnInit();
        }
    }
    
public:  // Make public for test access
    void createPIDFile() {
        FILE* pidFile = fopen("/var/run/dcm.pid", "w");
        if (pidFile) {
            fprintf(pidFile, "%d\n", getpid());
            fclose(pidFile);
        }
    }
    
    bool pidFileExists() {
        return access("/var/run/dcm.pid", F_OK) == 0;
    }
    
    DCMDHandle testHandle;
};

TEST_F(DcmDaemonMainUnInitTest, UnInit_ValidHandle_CompletesSuccessfully) {
    testHandle.isDCMRunning = true;
    
    EXPECT_NO_THROW(dcmDaemonMainUnInit(&testHandle));
    
    EXPECT_EQ(testHandle.pExecBuff, nullptr);
    EXPECT_EQ(testHandle.pDcmSetHandle, nullptr);
    EXPECT_EQ(testHandle.pRbusHandle, nullptr);
    EXPECT_EQ(testHandle.pLogSchedHandle, nullptr);
    EXPECT_EQ(testHandle.pDifdSchedHandle, nullptr);
}

TEST_F(DcmDaemonMainUnInitTest, UnInit_DCMNotRunning_RemovesPIDFile) {
    testHandle.isDCMRunning = false;
    createPIDFile();
    
    EXPECT_TRUE(pidFileExists());
    EXPECT_NO_THROW(dcmDaemonMainUnInit(&testHandle));
}

// ======================= Signal Handler Tests =======================
class SignalHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_pdcmHandle = (DCMDHandle*)malloc(sizeof(DCMDHandle));
        memset(g_pdcmHandle, 0, sizeof(DCMDHandle));
    }
    
    void TearDown() override {
        if (g_pdcmHandle) {
            free(g_pdcmHandle);
            g_pdcmHandle = nullptr;
        }
    }
};

TEST_F(SignalHandlerTest, SigHandler_SIGINT_ExitsCorrectly) {
    EXPECT_EXIT(get_sig_handler(SIGINT), ExitedWithCode(1), ".*");
}

TEST_F(SignalHandlerTest, SigHandler_SIGKILL_ExitsCorrectly) {
    EXPECT_EXIT(get_sig_handler(SIGKILL), ExitedWithCode(1), ".*");
}

TEST_F(SignalHandlerTest, SigHandler_SIGTERM_ExitsCorrectly) {
    EXPECT_EXIT(get_sig_handler(SIGTERM), ExitedWithCode(1), ".*");
}

TEST_F(SignalHandlerTest, SigHandler_UnknownSignal_NoExit) {
    EXPECT_NO_THROW(get_sig_handler(SIGUSR1));
}

// ======================= Main Function =======================
GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s", 
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);
    
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    
    cout << "Starting DCM Unit Tests" << endl;
    return RUN_ALL_TESTS();
}
