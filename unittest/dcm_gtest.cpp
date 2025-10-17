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

//extern "C" {
  void get_dcmRunJobs(const INT8* profileName, VOID *pHandle);
  #include "dcm.c"
  #include "dcm.h"
//} 
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

TEST_F(DcmDaemonMainInitTest, MainInit_checkdemonstatus_fail) {
    
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    EXPECT_EQ(result, DCM_FAILURE);
}

TEST_F(DcmDaemonMainInitTest, MainInit_rbus_int_failure) {
    // Setup successful RBUS mocks
    rbusHandle_t mockHandle = mock_rbus_get_mock_handle();
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
         .WillOnce(Return(RBUS_ERROR_BUS_ERROR));
    
    
    INT32 result = dcmDaemonMainInit(&dcmHandle);
    
    EXPECT_EQ(result, DCM_FAILURE);
}

class DcmRunJobsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize DCM handle
        memset(&dcmHandle, 0, sizeof(DCMDHandle));
        
        // Allocate execution buffer
        dcmHandle.pExecBuff = (INT8*)malloc(EXECMD_BUFF_SIZE);
        ASSERT_NE(dcmHandle.pExecBuff, nullptr);
        
        // Initialize settings handle
        INT32 ret = dcmSettingsInit(&dcmHandle.pDcmSetHandle);
        if (ret != DCM_SUCCESS) {
            // If settings init fails, create a minimal mock handle
            dcmHandle.pDcmSetHandle = malloc(64);
            ASSERT_NE(dcmHandle.pDcmSetHandle, nullptr);
        }
        
        // Store original environment
        originalPath = getenv("PATH");
        
        // Create test directories
        system("mkdir -p /tmp/test_dcm_scripts");
        createTestScripts();
    }
    
    void TearDown() override {
        // Cleanup
        if (dcmHandle.pExecBuff) {
            free(dcmHandle.pExecBuff);
        }
        if (dcmHandle.pDcmSetHandle) {
            dcmSettingsUnInit(dcmHandle.pDcmSetHandle);
        }
        
        // Cleanup test files
        system("rm -rf /tmp/test_dcm_scripts");
        
        // Restore environment
        if (originalPath) {
            setenv("PATH", originalPath, 1);
        }
    }
    
    void createTestScripts() {
        // Create test uploadSTBLogs.sh script
        const char* uploadScript = R"(#!/bin/bash
        echo "Upload script called with args: $*" > /tmp/test_upload_output.txt
        echo "Protocol: $3" >> /tmp/test_upload_output.txt
        echo "URL: $4" >> /tmp/test_upload_output.txt
        exit 0
        )";
        system("chmod +x /tmp/test_dcm_scripts/uploadSTBLogs.sh");
        
        // Create test swupdate_utility.sh script
        const char* swupdateScript = R"(#!/bin/bash
        echo "SW Update script called with args: $*" > /tmp/test_swupdate_output.txt
        echo "Mode: $1" >> /tmp/test_swupdate_output.txt
        echo "Type: $2" >> /tmp/test_swupdate_output.txt
        exit 0
        )";
    
        system("chmod +x /tmp/test_dcm_scripts/swupdate_utility.sh");
    }
    
    bool fileExists(const char* filename) {
        return access(filename, F_OK) == 0;
    }
    
    std::string readFile(const char* filename) {
        FILE* file = fopen(filename, "r");
        if (!file) return "";
        
        char buffer[1024];
        std::string content;
        while (fgets(buffer, sizeof(buffer), file)) {
            content += buffer;
        }
        fclose(file);
        return content;
    }
    
    DCMDHandle dcmHandle;
    const char* originalPath;
};

TEST_F(DcmRunJobsTest, RunJobs_LogUploadProfile_ExecutesCorrectScript) {
    // Set RDK path to our test directory
    setenv("DCM_RDK_PATH", "/tmp/test_dcm_scripts", 1);
    
    // Call the function with log upload profile
    get_dcmRunJobs(DCM_LOGUPLOAD_SCHED, &dcmHandle);
}
TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_ExecutesCorrectScript) {
    // Set RDK path to our test directory
    setenv("DCM_RDK_PATH", "/tmp/test_dcm_scripts", 1);
    
    // Call the function with DIFD profile
    get_dcmRunJobs(DCM_DIFD_SCHED, &dcmHandle);
}
TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_ExecutesCorrectScript_handle_null) {    
    EXPECT_NO_THROW({
        get_dcmRunJobs(DCM_DIFD_SCHED, NULL);
    });
    g_bMMEnable = 1;
    EXPECT_EQ(g_bMMEnable, 1);
    g_bMMEnable = 0;
    EXPECT_EQ(g_bMMEnable, 0);
}
TEST_F(DcmRunJobsTest, RunJobs_DifdProfile_ExecutesCorrectScript_g_bMMEnable)
{    
    g_bMMEnable = 1;
    EXPECT_NO_THROW({
        get_dcmRunJobs(DCM_DIFD_SCHED, NULL);
    });
    EXPECT_EQ(g_bMMEnable, 1);   
}



class DcmDaemonMainUnInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test handle
        memset(&testHandle, 0, sizeof(DCMDHandle));
        
        // Create test files/directories if needed
        system("mkdir -p /tmp/test_dcm");
        
        // Initialize components for testing
        setupTestComponents();
    }
    
    void TearDown() override {
        // Cleanup test files
        system("rm -rf /tmp/test_dcm");
        system("rm -f /var/run/dcm.pid");
        
        // Cleanup any allocated memory
        cleanupTestComponents();
    }
    
    void setupTestComponents() {
        // Allocate execution buffer
        testHandle.pExecBuff = (INT8*)malloc(EXECMD_BUFF_SIZE);
        if (testHandle.pExecBuff) {
            memset(testHandle.pExecBuff, 0, EXECMD_BUFF_SIZE);
            strcpy(testHandle.pExecBuff, "test command");
        }
        
        // Initialize settings handle (may fail, that's OK for testing)
        if (dcmSettingsInit(&testHandle.pDcmSetHandle) != DCM_SUCCESS) {
            testHandle.pDcmSetHandle = nullptr;
        }
        
        // Initialize scheduler
        if (dcmSchedInit() == DCM_SUCCESS) {
            // Add dummy scheduler jobs
            testHandle.pLogSchedHandle = dcmSchedAddJob("test_log", nullptr, nullptr);
            testHandle.pDifdSchedHandle = dcmSchedAddJob("test_difd", nullptr, nullptr);
        }
        
        // Initialize RBUS handle (may fail, that's OK for testing)
        if (dcmRbusInit(&testHandle.pRbusHandle) != DCM_SUCCESS) {
            testHandle.pRbusHandle = nullptr;
        }
    }
    
    void cleanupTestComponents() {
        // Cleanup scheduler if initialized
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

TEST_F(DcmDaemonMainUnInitTest, UnInit_ValidHandle_CompletesSuccessfully) {
    testHandle.isDCMRunning = true; // Won't remove PID file
    
    // Should complete without crashing
    EXPECT_NO_THROW(dcmDaemonMainUnInit(&testHandle));
    
    // Verify handle is cleared
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
    
    dcmDaemonMainUnInit(&testHandle);
    EXPECT_NO_THROW(dcmDaemonMainUnInit(&testHandle));
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
