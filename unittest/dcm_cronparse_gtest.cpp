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


extern "C" {
#include "dcm_cronparse.h"
#include "../dcm_types.h"
INT32 (*getdcmCronParseToUpper(void)) (INT8*);
UINT32 (*getdcmCronParseParseUint(void)) (const INT8*, INT32*);
UINT32 (*getdcmCronParseNextSetBit(void)) (UINT8*, UINT32, UINT32, INT32*);
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
class dcmCronParseTest : public ::testing::Test {
protected:
    void SetUp(){
    }

    void TearDown(){
    }
};
/*
TEST(dcmCronParseTest , NullPointerReturnsError) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8* str = NULL;

    INT32 result = myFunctionPtr(str); 
    EXPECT_EQ(result, 1);
}

TEST(dcmCronParseTest , EmptyStringNoChange) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8 input[] = "";

    INT32 result = myFunctionPtr(input); 
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(input, "");
}

TEST(dcmCronParseTest , AllLowercase) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8 input[] = "abcdef";

    INT32 result = myFunctionPtr(input); // Indirectly calls performRequest
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(input, "ABCDEF");
}

TEST(dcmCronParseTest , MixedCase) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8 input[] = "aBcDeF";

    INT32 result = myFunctionPtr(input); // Indirectly calls performRequest
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(input, "ABCDEF");
}

TEST(dcmCronParseTest , AlreadyUppercase) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8 input[] = "ABCDEF";
    INT32 result = myFunctionPtr(input); // Indirectly calls performRequest
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(input, "ABCDEF");   
}

TEST(dcmCronParseTest , StringWithDigitsAndSymbols) {
    INT8 input[] = "abc123!@#XYZ";
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT32 result = myFunctionPtr(input); 
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(input, "ABC123!@#XYZ");
}

TEST(dcmCronParseTest , StringWithSpaces) {
    auto myFunctionPtr = getdcmCronParseToUpper();
    INT8 input[] = "a b c D E F";
    INT32 result = myFunctionPtr(input); 
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(input, "A B C D E F");
}
*/

/*
TEST(dcmCronParseToUpperTest, UnicodeCharactersUnaffected) {
    // Depending on locale, toupper may not handle Unicode. Here, just check ASCII is uppercased and others remain.
    INT8 input[] = "abc\xC3\xA9\xC3\xB1"; // "abcéñ" in UTF-8 (may not be handled well, but for demonstration)
    EXPECT_EQ(dcmCronParseToUpper(input), 0);
    // Only 'a', 'b', 'c' should be uppercased; rest should remain.
    EXPECT_TRUE(strncmp(input, "ABC", 3) == 0);
}
*/


TEST(dcmCronParseTest , ValidNumber) {
    INT32 errcode;
    auto myFunctionPtr = getdcmCronParseParseUint();
    UINT32 result = myFunctionPtr("12345", &errcode); 
    EXPECT_EQ(result, 12345u);
    EXPECT_EQ(errcode, 0);
}

TEST(dcmCronParseTest , ZeroValue) {
    INT32 errcode;
    auto myFunctionPtr = getdcmCronParseParseUint();
    UINT32 result = myFunctionPtr("0", &errcode);
    EXPECT_EQ(result, 0u);
    EXPECT_EQ(errcode, 0);
}

TEST(dcmCronParseTest , NegativeNumber) {
    INT32 errcode;
    auto myFunctionPtr = getdcmCronParseParseUint();
    UINT32 result = myFunctionPtr("-123", &errcode);
    EXPECT_EQ(result, 0u);
    EXPECT_EQ(errcode, 1);
}

TEST(dcmCronParseTest , NonNumericString) {
    INT32 errcode;
    auto myFunctionPtr = getdcmCronParseParseUint();
    UINT32 result = myFunctionPtr("abc", &errcode);
    EXPECT_EQ(result, 0u);
    EXPECT_EQ(errcode, 1);
}

TEST(dcmCronParseTest , MixedAlphaNumeric) {
    INT32 errcode;
    auto myFunctionPtr = getdcmCronParseParseUint();
    UINT32 result = myFunctionPtr("123abc", &errcode);
    EXPECT_EQ(result, 0u);
    EXPECT_EQ(errcode, 1);
}

TEST(dcmCronParseTest , OverflowValue) {
    INT32 errcode;
    char bigNum[32];
    auto myFunctionPtr = getdcmCronParseParseUint();
    snprintf(bigNum, sizeof(bigNum), "%lld", (long long)INT_MAX + 1);
    UINT32 result = myFunctionPtr(bigNum, &errcode);
    EXPECT_EQ(result, 0u);
    EXPECT_EQ(errcode, 1);
}

TEST(dcmCronParseTest , MaxIntValue) {
    INT32 errcode;
    char maxIntStr[32];
    auto myFunctionPtr = getdcmCronParseParseUint();
    snprintf(maxIntStr, sizeof(maxIntStr), "%d", INT_MAX);
    UINT32 result = myFunctionPtr(maxIntStr, &errcode);
    EXPECT_EQ(result, static_cast<UINT32>(INT_MAX));
    EXPECT_EQ(errcode, 0);
}

TEST(dcmCronParseTest, ParseValidExpression) {
    dcmCronExpr expr = {};
    const INT8* cron = "* * * * * *"; // Example: every second
    INT32 result = dcmCronParseExp(cron, &expr);
    EXPECT_EQ(result, 0); // Adjust expected value based on implementation
}

TEST(dcmCronParseTest, ParseInvalidExpression) {
    dcmCronExpr expr = {};
    const INT8* cron = NULL;
    INT32 result = dcmCronParseExp(cron, &expr);
    EXPECT_EQ(result, -1); // Should fail
}

TEST(dcmCronParseTest, ParseInvalidExpression1) {
    //dcmCronExpr expr = NULL;
    dcmCronExpr* expr = NULL;
    const INT8* cron = NULL;
    INT32 result = dcmCronParseExp(cron, expr);
    EXPECT_EQ(result, -1); // Should fail
}
TEST(dcmCronParseTest, GetNextTime) {
    dcmCronExpr expr = {};
    const INT8* cron = "* * * * * *";
    dcmCronParseExp(cron, &expr);
    time_t now = time(nullptr);
    time_t next = dcmCronParseGetNext(&expr, now);
    EXPECT_GT(next, now); // Next time should be in the future
}


// Test with NULL bits parameter
TEST(dcmCronParseTest, NextSetBit_NullBits_ReturnsNotFound) {
    INT32 notfound = 0;
    auto myFunctionPtr = getdcmCronParseNextSetBit();
    UINT32 result = myFunctionPtr(NULL, 64, 0, &notfound); 

    EXPECT_EQ(result, 0);
    EXPECT_EQ(notfound, 1);
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
