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

/**
 * Copyright 2023 Comcast Cable Communications Management, LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fstream>
#include "dcm_types.h"
#include "dcm.h"
#include "dcm.c"
#include "./mocks/mockrbus.h"

using namespace testing;
using namespace std;

// ======================= Test Configuration =======================
#define TEST_REPORT_DIR "/tmp/Gtest_Report/"
#define TEST_REPORT_FILE "dcm_test_report.json"
#define REPORT_PATH_SIZE 256

// ======================= DCM Main Init Tests =======================
class DcmMainInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRBus = new StrictMock<MockRBus>();
        mock_rbus_set_global_mock(mockRBus);
        mock_rbus_reset();
        
        memset(&dcmHandle, 0, sizeof(DCMDHandle));
        dcmHandle.isDCMRunning = false;
    }
    
    void TearDown() override {
        cleanupHandle();
        mock_rbus_clear_global_mock();
        delete mockRBus;
    }
    
private:
    void cleanupHandle() {
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
        dcmSchedUnInit();
    }
    
    MockRBus* mockRBus;
    DCMDHandle dcmHandle;
};

TEST_F(DcmMainInitTest, InitSuccess_AllComponentsWork) {
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
    std::remove(DCM_PID_FILE);
    
    // RBUS setup
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_ENABLED));
    
    EXPECT_CALL(*mockRBus, rbus_open(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(mockHandle), Return(RBUS_ERROR_SUCCESS)));
    
    // T2 version mock
    rbusValue_t mockValue = mock_rbus_create_string_value("2.1.5");
    EXPECT_CALL(*mockRBus, rbus_get(mockHandle, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(mockValue), Return(RBUS_ERROR_SUCCESS)));
    
    EXPECT_CALL(*mockRBus, rbusValue_GetType(mockValue))
        .WillOnce(Return(RBUS_STRING));
    
    EXPECT_CALL(*mockRBus, rbusValue_ToString(mockValue, nullptr, 0))
        .WillOnce(Return(strdup("2.1.5")));
    
    EXPECT_CALL(*mockRBus, rbusValue_Release(mockValue)).Times(1);
    
    // Event subscription
    EXPECT_CALL(*mockRBus, rbusEvent_SubscribeAsync(_, _, _, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(RBUS_ERROR_SUCCESS));
    
    EXPECT_CALL(*mockRBus, rbus_regDataElements(_, _, _))
        .WillOnce(Return(RBUS_ERROR_SUCCESS));
    
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(dcmHandle.pDcmSetHandle, nullptr);
    EXPECT_NE(dcmHandle.pExecBuff, nullptr);
}

TEST_F(DcmMainInitTest, InitFail_DemonStatusCheck) {
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmMainInitTest, InitFail_RbusDisabled) {
    std::remove(DCM_PID_FILE);
    
    EXPECT_CALL(*mockRBus, rbus_checkStatus())
        .WillOnce(Return(RBUS_DISABLED));
    
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    EXPECT_EQ(result, DCM_FAILURE);
}

// ======================= DCM Run Jobs Tests =======================
class RunJobsTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&dcmHandle, 0, sizeof(DCMDHandle));
        dcmHandle.pExecBuff = (INT8*)malloc(EXECMD_BUFF_SIZE);
        ASSERT_NE(dcmHandle.pExecBuff, nullptr);
        
        if (dcmSettingsInit(&dcmHandle.pDcmSetHandle) != DCM_SUCCESS) {
            dcmHandle.pDcmSetHandle = malloc(64);
        }
    }
    
    void TearDown() override {
        if (dcmHandle.pExecBuff) {
            free(dcmHandle.pExecBuff);
        }
        if (dcmHandle.pDcmSetHandle) {
            dcmSettingsUnInit(dcmHandle.pDcmSetHandle);
        }
    }
    
    DCMDHandle dcmHandle;
};

TEST_F(RunJobsTest, LogUpload_ValidHandle) {
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_LOGUPLOAD_SCHED, &dcmHandle));
}

TEST_F(RunJobsTest, FirmwareUpdate_ValidHandle) {
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, &dcmHandle));
}

TEST_F(RunJobsTest, NullHandle_NoThrow) {
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, nullptr));
}

TEST_F(RunJobsTest, MaintenanceMode_Disabled) {
    g_bMMEnable = 0;
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, nullptr));
    EXPECT_EQ(g_bMMEnable, 0);   
}

TEST_F(RunJobsTest, NullSettingsHandle) {
    dcmHandle.pDcmSetHandle = nullptr;
    EXPECT_NO_THROW(get_dcmRunJobs(DCM_DIFD_SCHED, nullptr));   
}

// ======================= DCM Un-Init Tests =======================
class UnInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&testHandle, 0, sizeof(DCMDHandle));
        prepareTestHandle();
    }
    
    void TearDown() override {
        cleanupTest();
    }
    
private:
    void prepareTestHandle() {
        testHandle.pExecBuff = (INT8*)malloc(EXECMD_BUFF_SIZE);
        if (testHandle.pExecBuff) {
            strcpy(testHandle.pExecBuff, "test command");
        }
        
        if (dcmSettingsInit(&testHandle.pDcmSetHandle) != DCM_SUCCESS) {
            testHandle.pDcmSetHandle = nullptr;
        }
        
        if (dcmSchedInit() == DCM_SUCCESS) {
            testHandle.pLogSchedHandle = dcmSchedAddJob("test_log", nullptr, nullptr);
            testHandle.pDifdSchedHandle = dcmSchedAddJob("test_difd", nullptr, nullptr);
        }
    }
    
    void cleanupTest() {
        system("rm -f /var/run/dcm.pid");
        if (testHandle.pLogSchedHandle || testHandle.pDifdSchedHandle) {
            dcmSchedUnInit();
        }
    }
    
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

TEST_F(UnInitTest, ValidHandle_Success) {
    testHandle.isDCMRunning = true;
    
    EXPECT_NO_THROW(dcmDaemonMainUnInit(&testHandle));
    
    EXPECT_EQ(testHandle.pExecBuff, nullptr);
    EXPECT_EQ(testHandle.pDcmSetHandle, nullptr);
    EXPECT_EQ(testHandle.pRbusHandle, nullptr);
}

TEST_F(UnInitTest, NotRunning_RemovesPID) {
    testHandle.isDCMRunning = false;
    createPIDFile();
    
    EXPECT_TRUE(pidFileExists());
    EXPECT_NO_THROW(dcmDaemonMainUnInit(&testHandle));
}

// ======================= Signal Handler Tests =======================
class SignalTest : public ::testing::Test {
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

TEST_F(SignalTest, SIGINT_ExitsCorrectly) {
    EXPECT_EXIT(get_sig_handler(SIGINT), ExitedWithCode(1), ".*");
}

TEST_F(SignalTest, SIGKILL_ExitsCorrectly) {
    EXPECT_EXIT(get_sig_handler(SIGKILL), ExitedWithCode(1), ".*");
}

TEST_F(SignalTest, SIGTERM_ExitsCorrectly) {
    EXPECT_EXIT(get_sig_handler(SIGTERM), ExitedWithCode(1), ".*");
}

TEST_F(SignalTest, UnknownSignal_NoExit) {
    EXPECT_NO_THROW(get_sig_handler(SIGUSR1));
}

// ======================= Main Function =======================
GTEST_API_ int main(int argc, char *argv[]) {
    char reportPath[REPORT_PATH_SIZE];
    memset(reportPath, 0, REPORT_PATH_SIZE);
    
    snprintf(reportPath, REPORT_PATH_SIZE, "json:%s%s", TEST_REPORT_DIR, TEST_REPORT_FILE);
    ::testing::GTEST_FLAG(output) = reportPath;
    ::testing::InitGoogleTest(&argc, argv);
    
    cout << "Starting DCM Unit Tests" << endl;
    return RUN_ALL_TESTS();
}
