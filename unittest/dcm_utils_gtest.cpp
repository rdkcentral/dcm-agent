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
#include <fstream>
/*
extern "C" {

#include "dcm_cronparse.h"
#include "dcm_cronparse.c"
#include "../dcm_types.h"
#include "../dcm_utils.c"
} */ 

#include "dcm_cronparse.h"
//#include "dcm_cronparse.c"
#include "../dcm_types.h"
//#include "../dcm_utils.c"

/*#include "rdm_types.h"
#include "rdm.h"
#include "rdm_utils.h"
*/
/*
  #include "mocks/mock_curl.h"
extern "C" {
    #include "rdm_curldownload.h"
}
*/

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

//extern static INT32 dcmCronParseToUpper(INT8* str);
class DCMUtilsTest: public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

#include "dcm_utils.h"

// Helper: create a file with given content
void CreateFile(const char* filename, const char* content) {
    std::ofstream ofs(filename);
    ofs << content;
}

void RemoveFile(const char* filename) {
    std::remove(filename);
}

// Test dcmUtilsFilePresentCheck
TEST(DCMUtilsTest, FilePresentCheck_ValidFile) {
    const char* fname = "/tmp/dcm_testfile_present";
    CreateFile(fname, "test");
    EXPECT_EQ(dcmUtilsFilePresentCheck(fname), DCM_SUCCESS);
    RemoveFile(fname);
}

TEST(DCMUtilsTest, FilePresentCheck_NullPtr) {
    EXPECT_EQ(dcmUtilsFilePresentCheck(NULL), DCM_FAILURE);
}

TEST(DCMUtilsTest, FilePresentCheck_FileNotExist) {
    const char* fname = "/tmp/dcm_testfile_not_exist";
    RemoveFile(fname);
    EXPECT_EQ(dcmUtilsFilePresentCheck(fname), DCM_FAILURE);
}

// Test dcmUtilsCopyCommandOutput
TEST(DCMUtilsTest, CopyCommandOutput_Echo) {
    char output[128];
    dcmUtilsCopyCommandOutput((INT8*)"echo hello", (INT8*)output, sizeof(output));
    EXPECT_STREQ(output, "hello");
}

TEST(DCMUtilsTest, CopyCommandOutput_NullOut) {
    // Should not crash or segfault
    dcmUtilsCopyCommandOutput((INT8*)"echo test", NULL, 0);
}


// Test dcmUtilsSysCmdExec
TEST(DCMUtilsTest, SysCmdExec_Valid) {
    EXPECT_EQ(dcmUtilsSysCmdExec((INT8*)"echo test"), DCM_SUCCESS);
}

TEST(DCMUtilsTest, SysCmdExec_Null) {
    EXPECT_EQ(dcmUtilsSysCmdExec(NULL), DCM_FAILURE);
}

// Test dcmUtilsGetFileEntry
TEST(DCMUtilsTest, GetFileEntry_ValidKey) {
    const char* fname = "/tmp/dcm_test_kv";
    CreateFile(fname, "key1=value1\nkey2=value2\n");
    INT8* result = dcmUtilsGetFileEntry(fname, "key2");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "value2");
    free(result);
    RemoveFile(fname);
}

TEST(DCMUtilsTest, GetFileEntry_KeyNotFound) {
    const char* fname = "/tmp/dcm_test_kv";
    CreateFile(fname, "key1=value1\n");
    INT8* result = dcmUtilsGetFileEntry(fname, "key2");
    EXPECT_EQ(result, nullptr);
    RemoveFile(fname);
}

TEST(DCMUtilsTest, GetFileEntry_NullArgs) {
    EXPECT_EQ(dcmUtilsGetFileEntry(NULL, "key"), nullptr);
    EXPECT_EQ(dcmUtilsGetFileEntry("file", NULL), nullptr);
}

// Test dcmUtilsRemovePIDfile
TEST(DCMUtilsTest, RemovePIDfile_Existing) {
    // Create a fake PID file
    CreateFile(DCM_PID_FILE, "1234\n");
    dcmUtilsRemovePIDfile();
    // Now file should not exist
    std::ifstream ifs(DCM_PID_FILE);
    EXPECT_FALSE(ifs.good());
}

// Test dcmUtilsCheckDaemonStatus (basic functionality)
TEST(DCMUtilsTest, CheckDaemonStatus_NewFile) {
    RemoveFile(DCM_PID_FILE);
    EXPECT_EQ(dcmUtilsCheckDaemonStatus(), DCM_SUCCESS);
    RemoveFile(DCM_PID_FILE);
}

TEST(DCMUtilsTest, CheckDaemonStatus_PidFileExists_ProcessNotRunning) {
    // Create PID file with a PID that doesn't exist (99999)
    CreateFile(DCM_PID_FILE, "99999");
    
    EXPECT_EQ(dcmUtilsCheckDaemonStatus(), DCM_SUCCESS);
    
    // Verify new PID file was created with current process ID
    EXPECT_EQ(dcmUtilsFilePresentCheck(DCM_PID_FILE), DCM_SUCCESS);
}

// Test when PID file exists and process appears to be running
TEST(DCMUtilsTest, PidFileExists_ProcessRunning_ReturnsFailure) {
    // Use PID 1 (init process) which should always exist on Linux systems
    CreateFile(DCM_PID_FILE, "1");
    
    EXPECT_EQ(dcmUtilsCheckDaemonStatus(), DCM_FAILURE);
    
    // Original PID file should still exist
    EXPECT_EQ(dcmUtilsFilePresentCheck(DCM_PID_FILE), DCM_SUCCESS);
}
/*
// Test when PID file exists with current process PID (self-detection)
TEST(DCMUtilsTest, PidFileExists_CurrentProcess_ReturnsFailure) {
    // Create PID file with current process PID
    pid_t currentPid = getpid();
    char pidStr[32];
    snprintf(pidStr, sizeof(pidStr), "%d", currentPid);
    CreateFile(DCM_PID_FILE, pidStr);
    
    EXPECT_EQ(dcmUtilsCheckDaemonStatus(), DCM_FAILURE);
}
*/
/*
// Alternative test: Create read-only file that cannot be overwritten
TEST(DCMUtilsTest, CannotOverwriteReadOnlyPidFile_ReturnsFailure) {
    // First create a PID file with invalid PID so it passes the first check
    std::ofstream ofs(DCM_PID_FILE);
    ofs << "99999";  // Non-existent PID
    ofs.close();
    
    // Make the file read-only (no write permissions)
    ASSERT_EQ(chmod(DCM_PID_FILE, 0444), 0) << "Failed to make file read-only";
    
    // This should fail when trying to open for writing
    EXPECT_EQ(dcmUtilsCheckDaemonStatus(), DCM_FAILURE);
    
    // Restore write permissions for cleanup
    chmod(DCM_PID_FILE, 0644);
}
*/
// Test dcmIARMEvntSend (does nothing)
TEST(DCMUtilsTest, IARMEvntSend) {
    EXPECT_EQ(dcmIARMEvntSend(0), DCM_SUCCESS);
}

TEST(DCMUtilsTest, LogInit_Success) {    
    DCMLOGInit();
}

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


