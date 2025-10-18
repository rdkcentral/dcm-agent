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
#include "dcm_cronparse.h"
#include "dcm_types.h"
#include "dcm_cronparse.c"

// Function pointers to access static functions
INT32 (*getdcmCronParseToUpper(void)) (INT8*);
UINT32 (*getdcmCronParseParseUint(void)) (const INT8*, INT32*);
UINT32 (*getdcmCronParseNextSetBit(void)) (UINT8*, UINT32, UINT32, INT32*);
INT32 (*getdcmCronParseResetMin(void)) (struct tm*, INT32);
INT32 (*getdcmCronParseResetAllMin(void)) (struct tm*, INT32*);
INT32 (*getdcmCronParseSetField(void))(struct tm*, INT32, INT32);

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

class dcmCronParseTest : public ::testing::Test {
protected:
    void SetUp(){
    }

    void TearDown(){
    }
};

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

TEST(dcmCronParseTest, dcmCronParseExpInvalidTarget) {
    dcmCronExpr expr = {};
    const INT8* cron = "* * * * * *";
    INT32 result = dcmCronParseExp(cron, NULL);
    EXPECT_EQ(result, -1); // Should fail
}

TEST(dcmCronParseTest, dcmCronParseExpInvalidExpression) {
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

TEST(dcmCronParseTest, GetNext_NullExpression_ReturnsInvalidInstant) {
    time_t date = time(NULL);
    
    time_t result = dcmCronParseGetNext(NULL, date);
    
    EXPECT_EQ(result, -1);
}

class DcmCronParseResetMinTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test calendar with specific values
        memset(&testCalendar, 0, sizeof(struct tm));
        
        // Set to a specific date: 2024-03-15 14:30:45 (Friday)
        testCalendar.tm_year = 2024 - 1900; // tm_year is years since 1900
        testCalendar.tm_mon = 2;            // March (0-based)
        testCalendar.tm_mday = 15;          // 15th day
        testCalendar.tm_hour = 14;          // 2 PM
        testCalendar.tm_min = 30;           // 30 minutes
        testCalendar.tm_sec = 45;           // 45 seconds
        testCalendar.tm_wday = 5;           // Friday (0=Sunday)
        testCalendar.tm_yday = 74;          // Day of year
        testCalendar.tm_isdst = 0;          // No DST
        
        // Store original values for comparison
        originalCalendar = testCalendar;
    }
    
    void TearDown() override {
    }
    
    struct tm testCalendar;
    struct tm originalCalendar;
};


TEST_F(DcmCronParseResetMinTest, ResetMin_SecondField_ResetsToZero) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_sec, 45);
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset seconds field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_SECOND);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify seconds field is reset to 0
    EXPECT_EQ(testCalendar.tm_sec, 0);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_MinuteField_ResetsToZero) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_min, 30);
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset minutes field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_MINUTE);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify minutes field is reset to 0
    EXPECT_EQ(testCalendar.tm_min, 0);
    
    // Verify other fields remain unchanged (except those normalized by mktime)
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_HourField_ResetsToZero) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_hour, 14);
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset hour field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_HOUR_OF_DAY);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify hour field is reset to 0
    EXPECT_EQ(testCalendar.tm_hour, 0);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_DayOfWeekField_ResetsToZero) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_wday, 5); // Friday
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset day of week field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_DAY_OF_WEEK);
    
    // Verify success
    EXPECT_EQ(result, 0);
} 

TEST_F(DcmCronParseResetMinTest, ResetMin_DayOfMonthField_ResetsToOne) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_mday, 15);
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset day of month field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_DAY_OF_MONTH);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify day of month field is reset to 1 (first day)
    EXPECT_EQ(testCalendar.tm_mday, 1);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_MonthField_ResetsToZero) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_mon, 2); // March
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset month field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_MONTH);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify month field is reset to 0 (January)
    EXPECT_EQ(testCalendar.tm_mon, 0);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_YearField_ResetsToZero) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_year, 124); // 2024 - 1900
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_YEAR);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify year field is reset to 0 (1900)
    EXPECT_EQ(testCalendar.tm_year, 0);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_default_field) {
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, 8);
    // Verify success
    EXPECT_EQ(result, 1);
}
TEST_F(DcmCronParseResetMinTest, ResetMin_invalid_field) {
    auto myFunctionPtr = getdcmCronParseResetMin();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, -1);
    // Verify success
    EXPECT_EQ(result, 1);
}

TEST_F(DcmCronParseResetMinTest, ResetMin_NullCalendar_ReturnsFailure) {
    // Test with NULL calendar
    auto myFunctionPtr = getdcmCronParseResetMin();
    INT32 result = myFunctionPtr(nullptr, CRON_CF_SECOND);
    // Verify failure
    EXPECT_EQ(result, 1);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseResetAllMin_success) {
    INT32 testFields[CRON_CF_ARR_LEN];
    memset(testFields, -1, sizeof(testFields));
    testFields[0] = CRON_CF_SECOND;
    auto myFunctionPtr = getdcmCronParseResetAllMin();
    INT32 result = myFunctionPtr(&testCalendar, testFields);
    EXPECT_EQ(result, 0);
    // Verify seconds field is reset to 0
    EXPECT_EQ(testCalendar.tm_sec, 0);

    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
} 
TEST_F(DcmCronParseResetMinTest, dcmCronParseResetAllMin_fail) {
    INT32 testFields[CRON_CF_ARR_LEN];
    memset(testFields, -1, sizeof(testFields));
    testFields[0] = CRON_CF_SECOND;
    auto myFunctionPtr = getdcmCronParseResetAllMin();
    INT32 result = myFunctionPtr(nullptr, testFields);
    EXPECT_EQ(result, 1);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_SecondField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_sec, 45);
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset seconds field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_SECOND, 35);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify seconds field is reset to 0
    EXPECT_EQ(testCalendar.tm_sec, 35);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_MinuteField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_min, 30);
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset minutes field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_MINUTE, 50);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify minutes field is set to 50
    EXPECT_EQ(testCalendar.tm_min, 50);
    
    // Verify other fields remain unchanged (except those normalized by mktime)
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_HourField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_hour, 14);
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset hour field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_HOUR_OF_DAY, 5);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify hour field is reset to 5
    EXPECT_EQ(testCalendar.tm_hour, 5);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_DayOfWeekField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_wday, 5); // Friday
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset day of week field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_DAY_OF_WEEK, 3);
    
    // Verify success
    EXPECT_EQ(result, 0);
  
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_DayOfMonthField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_mday, 15);
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset day of month field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_DAY_OF_MONTH, 25);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify day of month field is set to 25 
    EXPECT_EQ(testCalendar.tm_mday, 25);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_MonthField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_mon, 2); // March
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset month field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_MONTH, 6);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify month field is reset to 6 (January)
    EXPECT_EQ(testCalendar.tm_mon, 6);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_YearField_setvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_year, 124); 
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_YEAR, 1);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify year field is set to 1
    EXPECT_EQ(testCalendar.tm_year, 1);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
}
TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_NullCalendar_ReturnsFailure) {
    // Test with NULL calendar
    auto myFunctionPtr = getdcmCronParseSetField();
    INT32 result = myFunctionPtr(nullptr, CRON_CF_SECOND, 20);
    // Verify failure
    EXPECT_EQ(result, 1);
}
TEST_F(DcmCronParseResetMinTest, dcmCronParseSetField_default_field) {
    auto myFunctionPtr = getdcmCronParseSetField();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, 8, 25);
    // Verify success
    EXPECT_EQ(result, 1);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_MinuteField_addvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_min, 30);
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset minutes field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_MINUTE, 5);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify minutes field is set to 35
    EXPECT_EQ(testCalendar.tm_min, 35);
    
    // Verify other fields remain unchanged (except those normalized by mktime)
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_HourField_addvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_hour, 14);
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset hour field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_HOUR_OF_DAY, 5);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify hour field is set to 19
    EXPECT_EQ(testCalendar.tm_hour, 19);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_DayOfWeekField_addvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_wday, 5); // Friday
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset day of week field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_DAY_OF_WEEK, 3);
    
    // Verify success
    EXPECT_EQ(result, 0);
  
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_DayOfMonthField_addvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_mday, 15);
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset day of month field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_DAY_OF_MONTH, 5);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify day of month field is set to 20
    EXPECT_EQ(testCalendar.tm_mday, 20);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, getdcmCronParseAddToField_MonthField_addvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_mon, 2); // March
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset month field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_MONTH, 6);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify month field is reset to 6 (January)
    EXPECT_EQ(testCalendar.tm_mon, 8);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_year, originalCalendar.tm_year);
}

TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_YearField_addvalue) {
    // Verify initial state
    EXPECT_EQ(testCalendar.tm_year, 124); 
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, CRON_CF_YEAR, 1);
    
    // Verify success
    EXPECT_EQ(result, 0);
    
    // Verify year field is set to 1
    EXPECT_EQ(testCalendar.tm_year, 125);
    
    // Verify other fields remain unchanged
    EXPECT_EQ(testCalendar.tm_sec, originalCalendar.tm_sec);
    EXPECT_EQ(testCalendar.tm_min, originalCalendar.tm_min);
    EXPECT_EQ(testCalendar.tm_hour, originalCalendar.tm_hour);
    EXPECT_EQ(testCalendar.tm_mday, originalCalendar.tm_mday);
    EXPECT_EQ(testCalendar.tm_mon, originalCalendar.tm_mon);
}
TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_NullCalendar_ReturnsFailure) {
    // Test with NULL calendar
    auto myFunctionPtr = getdcmCronParseAddToField();
    INT32 result = myFunctionPtr(nullptr, CRON_CF_SECOND, 20);
    // Verify failure
    EXPECT_EQ(result, 1);
}
TEST_F(DcmCronParseResetMinTest, dcmCronParseAddToField_default) {
    auto myFunctionPtr = getdcmCronParseAddToField();
    // Reset year field
    INT32 result = myFunctionPtr(&testCalendar, 8, 25);
    // Verify success
    EXPECT_EQ(result, 1);
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
