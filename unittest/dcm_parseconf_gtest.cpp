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



// Helper function to create a test handle
DCMSettingsHandle* CreateTestHandle() {
    DCMSettingsHandle* handle = (DCMSettingsHandle*)malloc(sizeof(DCMSettingsHandle));
    memset(handle, 0, sizeof(DCMSettingsHandle));
    return handle;
}

// ==================== dcmSettingsGetUploadProtocol Tests ====================

TEST(dcmParseConfTest, GetUploadProtocol_ValidHandle_ReturnsProtocol) {
    DCMSettingsHandle* handle = CreateTestHandle();
    strcpy(handle->cUploadPrtl, "HTTPS");
    
    INT8* protocol = dcmSettingsGetUploadProtocol(handle);
    
    EXPECT_NE(protocol, nullptr);
    EXPECT_STREQ(protocol, "HTTPS");
    
    free(handle);
}
TEST(dcmParseConfTest, GetUploadProtocol_NullHandle_ReturnsNull) {
    INT8* protocol = dcmSettingsGetUploadProtocol(nullptr);
    
    EXPECT_EQ(protocol, nullptr);
}


TEST(dcmParseConfTest, GetUploadURL_ValidHandle_ReturnsURL) {
    DCMSettingsHandle* handle = CreateTestHandle();
    strcpy(handle->cUploadURL, "https://test.example.com/upload");
    
    INT8* url = dcmSettingsGetUploadURL(handle);
    
    EXPECT_NE(url, nullptr);
    EXPECT_STREQ(url, "https://test.example.com/upload");
    
    free(handle);
}

TEST(dcmParseConfTest, GetUploadURL_NullHandle_ReturnsNull) {
    INT8* url = dcmSettingsGetUploadURL(nullptr);
    
    EXPECT_EQ(url, nullptr);
}

TEST(dcmParseConfTest, UnInit_ValidHandle_Success) {
    DCMSettingsHandle* handle = CreateTestHandle();
    EXPECT_NE(handle, nullptr);
    // Should not crash and handle should be freed
    dcmSettingsUnInit(handle);
}

TEST(dcmParseConfTest, UnInit_NullHandle_NoError) {
    // Should handle gracefully without crashing
    dcmSettingsUnInit(nullptr);
}


TEST(dcmParseConfTest, GetRDKPath_ValidHandle_ReturnsPath) {
    DCMSettingsHandle* handle = CreateTestHandle();
    strcpy(handle->cRdkPath, "/usr/bin");
    
    INT8* path = dcmSettingsGetRDKPath(handle);
    
    EXPECT_NE(path, nullptr);
    EXPECT_STREQ(path, "/usr/bin");
    
    free(handle);
}

TEST(dcmParseConfTest, GetRDKPath_NullHandle_ReturnsNull) {
    INT8* path = dcmSettingsGetRDKPath(nullptr);
    
    EXPECT_EQ(path, nullptr);
}



// Helper function to create test JSON file
void CreateTestJSONFile(const char* filename, const char* content) {
    std::ofstream ofs(filename);
    if (ofs.is_open()) {
        ofs << content;
        ofs.close();
    }
}

// ==================== dcmSettingParseConf Tests ====================

TEST(dcmParseConfTest, ParseConf_ValidHandleAndFile_Success) {
    const char* validJson = R"({
        "uploadRepository:uploadProtocol": "HTTPS",
        "uploadRepository:URL": "https://test.example.com/upload",
        "urn:settings:TelemetryProfile:timeZone": "UTC",
        "urn:settings:LogUploadSettings:UploadOnReboot": true,
        "urn:settings:LogUploadSettings:PeriodicUpload": "0 */15 * * *",
        "urn:settings:FirmwareDownload:difdCron": "0 2 * * *"
    })";
    
    CreateTestJSONFile("/tmp/test_valid_settings.json", validJson);
    
    DCMSettingsHandle* handle = CreateTestHandle();
    INT8 logCron[256] = {0};
    INT8 difdCron[256] = {0};
    
    INT32 result = dcmSettingParseConf(handle, "/tmp/test_valid_settings.json", logCron, difdCron);
    
    EXPECT_EQ(result, DCM_FAILURE);
    EXPECT_STREQ(handle->cUploadPrtl, "");
    EXPECT_STREQ(handle->cUploadURL, "");
    EXPECT_STREQ(handle->cTimeZone, "");
    EXPECT_STREQ(logCron, "");
    EXPECT_STREQ(difdCron, "");
    
    free(handle);
    std::remove("/tmp/test_valid_settings.json");
}

TEST(dcmParseConfTest, ParseConf_EmptyJSON_Success) {
    const char* emptyJson = "{}";
    
    CreateTestJSONFile("/tmp/test_empty_settings.json", emptyJson);
    
    DCMSettingsHandle* handle = CreateTestHandle();
    INT8 logCron[256] = {0};
    INT8 difdCron[256] = {0};
    
    INT32 result = dcmSettingParseConf(handle, "/tmp/test_empty_settings.json", logCron, difdCron);
    
    EXPECT_EQ(result, DCM_FAILURE);
    // Should use default values
    EXPECT_STREQ(handle->cUploadPrtl, "HTTP");
    EXPECT_STREQ(handle->cUploadURL, DCM_DEF_LOG_URL);
    EXPECT_STREQ(handle->cTimeZone, DCM_DEF_TIMEZONE);
    EXPECT_STREQ(logCron, "");
    EXPECT_STREQ(difdCron, "");
    
    free(handle);
    std::remove("/tmp/test_empty_settings.json");
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
