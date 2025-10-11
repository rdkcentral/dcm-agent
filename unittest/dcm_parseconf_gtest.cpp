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
        
        // Clean up test files
        RemoveFile("/etc/include.properties");
        RemoveFile("/opt/persistent/DCMresponse.json");
        RemoveFile("/tmp/persistent/DCMresponse.json");
        RemoveFile("/custom/path/DCMresponse.json");
        RemoveFile("/tmp/dcm_settings.conf");
        RemoveFile("/opt/dcm_settings.conf");
        RemoveDirectory("/tmp/persistent");
        RemoveDirectory("/opt/persistent");
        RemoveDirectory("/custom/path"); 
    }
};

// Test when /etc/include.properties doesn't exist - uses default path
TEST_F(dcmParseConfTest, DefaultBoot_IncludeFileNotExists_UsesDefaultPath) {
    // Ensure /etc/include.properties doesn't exist
    RemoveFile("/etc/include.properties");
    
    // Create default persistent directory and DCM response file
    CreateDirectory("/opt/persistent");
    CreateFile("/opt/persistent/DCMresponse.json", 
               "{\"logUploadSettings\":{\"uploadRepository:URL\":\"https://test.com\"}}");
    
    INT32 result = dcmSettingDefaultBoot();
    
    EXPECT_EQ(result, DCM_SUCCESS);
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
