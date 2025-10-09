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

extern "C" {
#include "dcm_cronparse.h"
#include "../dcm_types.h"

}
//#include "../dcm_cronparse.c"

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
class dcmCronParseToUpperTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST(dcmCronParseToUpperTest, NullPointerReturnsError) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8* str = NULL;

    INT_32 result = myFunctionPtr(str); // Indirectly calls performRequest
    EXPECT_EQ(result, 1);
}
/*
TEST(dcmCronParseToUpperTest, EmptyStringNoChange) {
    INT8 input[] = "";
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    EXPECT_STREQ(input, "");
}

TEST(dcmCronParseToUpperTest, AllLowercase) {
    INT8 input[] = "abcdef";
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    EXPECT_STREQ(input, "ABCDEF");
}

TEST(dcmCronParseToUpperTest, MixedCase) {
    INT8 input[] = "aBcDeF";
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    EXPECT_STREQ(input, "ABCDEF");
}

TEST(dcmCronParseToUpperTest, AlreadyUppercase) {
    INT8 input[] = "ABCDEF";
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    EXPECT_STREQ(input, "ABCDEF");
}

TEST(dcmCronParseToUpperTest, StringWithDigitsAndSymbols) {
    INT8 input[] = "abc123!@#XYZ";
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    EXPECT_STREQ(input, "ABC123!@#XYZ");
}

TEST(dcmCronParseToUpperTest, StringWithSpaces) {
    INT8 input[] = "a b c D E F";
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    EXPECT_STREQ(input, "A B C D E F");
}

TEST(dcmCronParseToUpperTest, UnicodeCharactersUnaffected) {
    // Depending on locale, toupper may not handle Unicode. Here, just check ASCII is uppercased and others remain.
    INT8 input[] = "abc\xC3\xA9\xC3\xB1"; // "abcéñ" in UTF-8 (may not be handled well, but for demonstration)
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    // Only 'a', 'b', 'c' should be uppercased; rest should remain.
    EXPECT_TRUE(strncmp(input, "ABC", 3) == 0);
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
