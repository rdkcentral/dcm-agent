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
#include "./mocks/mockRbus.h"

/*extern "C" {
//#include "dcm_types.h"
//#include "dcm_parseconf.h"

}*/
#include "../dcm_utils.c"
#include "dcm_types.h"
#include "dcm_parseconf.c"
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

class dcmParseConfTest : public ::testing::Test {
protected:
    void SetUp(){
    }

    void TearDown(){

    }
};

// Test when /etc/include.properties doesn't exist - uses default path
TEST(dcmParseConfTest, DefaultBoot_IncludeFileNotExists_UsesDefaultPath) {    
    INT32 result = dcmSettingDefaultBoot();
    
    EXPECT_EQ(result, -1);
} 

TEST(dcmParseConfTest, DefaultBoot_IncludeFileNotExists_UsesDefaultPath_success) {
    // Ensure /etc/include.properties doesn't exist
    RemoveFile("/etc/include.properties");
    
    // Create default persistent directory and DCM response file
    CreateDirectory("/opt/.t2persistentfolder");
    CreateFile("/opt/.t2persistentfolder/DCMresponse.txt", 
               "{\"logUploadSettings\":{\"uploadRepository:URL\":\"https://test.com\"}}"); 
    
    INT32 result = dcmSettingDefaultBoot();
    
    EXPECT_EQ(result, 0);
} 

class DcmSettingsInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        handle = nullptr;
        // Create test property files
        CreateTestPropertyFiles();
    }

    void TearDown() override {
        if (handle) {
            dcmSettingsUnInit(handle);
            handle = nullptr;
        }
        // Clean up test files
        CleanupTestFiles();
    }

    void CreateTestPropertyFiles() {
        // Create include.properties with RDK_PATH
        CreateFile(INCLUDE_PROP_FILE, 
            "RDK_PATH=/usr/bin\n"
            "PERSISTENT_ENTRY=/opt/persistent\n"
            "OTHER_PROP=value\n");

        // Create device.properties with ENABLE_MAINTENANCE
        CreateFile(DEVICE_PROP_FILE,
            "ENABLE_MAINTENANCE=true\n"
            "DEVICE_TYPE=STB\n"
            "MODEL=TestModel\n");
    }

    void CreateTestPropertyFilesWithoutMaintenance() {
        // Create include.properties with RDK_PATH
        CreateFile(INCLUDE_PROP_FILE, 
            "RDK_PATH=/usr/bin\n"
            "PERSISTENT_ENTRY=/opt/persistent\n");

        // Create device.properties without ENABLE_MAINTENANCE
        CreateFile(DEVICE_PROP_FILE,
            "DEVICE_TYPE=STB\n"
            "MODEL=TestModel\n");
    }

    void CreateTestPropertyFilesWithoutRDKPath() {
        // Create include.properties without RDK_PATH
        CreateFile(INCLUDE_PROP_FILE, 
            "PERSISTENT_ENTRY=/opt/persistent\n"
            "OTHER_PROP=value\n");

        // Create device.properties with ENABLE_MAINTENANCE
        CreateFile(DEVICE_PROP_FILE,
            "ENABLE_MAINTENANCE=true\n"
            "DEVICE_TYPE=STB\n");
    }

    void CreateFile(const char* filename, const char* content) {
        std::ofstream ofs(filename);
        if (ofs.is_open()) {
            ofs << content;
            ofs.close();
        }
    }

    void CleanupTestFiles() {
        std::remove(INCLUDE_PROP_FILE);
        std::remove(DEVICE_PROP_FILE);
    }

    VOID* handle;
};

// Test successful initialization with all properties present
TEST_F(DcmSettingsInitTest, SuccessfulInitialization) {
    INT32 result = dcmSettingsInit(&handle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(handle, nullptr);
    
    // Verify handle contains expected values
    DCMSettingsHandle* dcmHandle = (DCMSettingsHandle*)handle;
    EXPECT_STREQ(dcmHandle->cRdkPath, "/usr/bin");
    
    // Check if maintenance manager flag is set
    EXPECT_EQ(dcmSettingsGetMMFlag(), 1);
}
// Test initialization with null handle pointer
/*
TEST_F(DcmSettingsInitTest, NullHandlePointer) {
    INT32 result = dcmSettingsInit(nullptr);
    
    // Should handle gracefully or return failure
    // This depends on your implementation - adjust based on expected behavior
    EXPECT_EQ(result, DCM_FAILURE);
}
*/

// Test initialization without RDK_PATH in properties
TEST_F(DcmSettingsInitTest, MissingRDKPath) {
    CleanupTestFiles();
    CreateTestPropertyFilesWithoutRDKPath();
    
    INT32 result = dcmSettingsInit(&handle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(handle, nullptr);
    
    // Should use default DCM_LIB_PATH
    DCMSettingsHandle* dcmHandle = (DCMSettingsHandle*)handle;
    EXPECT_STREQ(dcmHandle->cRdkPath, DCM_LIB_PATH);
}

// Test initialization without ENABLE_MAINTENANCE in properties
TEST_F(DcmSettingsInitTest, MissingMaintenanceFlag) {
    CleanupTestFiles();
    CreateTestPropertyFilesWithoutMaintenance();
    
    INT32 result = dcmSettingsInit(&handle);
    
    EXPECT_EQ(result, DCM_SUCCESS);
    EXPECT_NE(handle, nullptr);
    
    // Maintenance manager flag should be 0 when property is missing
    EXPECT_EQ(dcmSettingsGetMMFlag(), 0);
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
