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

extern "C" {
#include "dcm_cronparse.h"
#include "../dcm_types.h"
#include "dcm_cronparse.c"

}
#include "../dcm_schedjob.c"

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
#include "dcm_schedjob.h"


// Mock callback function for scheduler
void MockCallback(INT8 *name, VOID *userData) {
    int *callbackHit = static_cast<int*>(userData);
    (*callbackHit)++;
}

// Fixture for scheduler tests
class DcmSchedJobTest : public ::testing::Test {
protected:
    VOID *schedHandle = nullptr;
    int callbackHit = 0;

    virtual void SetUp() override {
        schedHandle = dcmSchedAddJob((INT8*)"TestJob", MockCallback, &callbackHit);
        ASSERT_NE(schedHandle, nullptr);
    }

    virtual void TearDown() override {
        if (schedHandle) {
            dcmSchedRemoveJob(schedHandle);
            schedHandle = nullptr;
        }
    }
};

TEST_F(DcmSchedJobTest, AddJobAndRemoveJob) {
    // Job was added in SetUp, should be non-null
    ASSERT_NE(schedHandle, nullptr);
    // RemoveJob called in TearDown, should not crash
}
/*
TEST_F(DcmSchedJobTest, StartJobWithInvalidCronPatternFails) {
    // Should fail with invalid cron pattern
    INT32 ret = dcmSchedStartJob(schedHandle, (INT8*)"invalid pattern");
    EXPECT_EQ(ret, DCM_FAILURE);
}
*/
/*
TEST_F(DcmSchedJobTest, StartAndStopJobWithValidCronPattern) {
    // A valid cron pattern (e.g., every minute: "* * * * *")
    INT32 ret = dcmSchedStartJob(schedHandle, (INT8*)"* * * * *");
    EXPECT_EQ(ret, DCM_SUCCESS);

    ret = dcmSchedStopJob(schedHandle);
    EXPECT_EQ(ret, DCM_SUCCESS);
}
*/

/*
TEST_F(DcmSchedJobTest, SchedulerCallbackIsCalledOnTimeout) {
    // Use a cron pattern that triggers almost immediately for the test
    INT32 ret = dcmSchedStartJob(schedHandle, (INT8*)"* * * * *"); // every minute
    EXPECT_EQ(ret, DCM_SUCCESS);

    // Wait a bit longer than a second to allow the callback to be triggered
    // (Depending on cron parser implementation, you may need to adjust this)
    sleep(2);

    EXPECT_GT(callbackHit, 0);

    dcmSchedStopJob(schedHandle);
}
*/
TEST(DcmSchedJobStandaloneTest, AddJobWithNullNameReturnsNull) {
    VOID *handle = dcmSchedAddJob(nullptr, MockCallback, nullptr);
    EXPECT_EQ(handle, nullptr);
}

TEST(DcmSchedJobStandaloneTest, StartJobWithNullHandleFails) {
    INT32 ret = dcmSchedStartJob(nullptr, (INT8*)"* * * * *");
    EXPECT_EQ(ret, DCM_FAILURE);
}

TEST(DcmSchedJobStandaloneTest, StopJobWithNullHandleFails) {
    INT32 ret = dcmSchedStopJob(nullptr);
    EXPECT_EQ(ret, DCM_FAILURE);
}

TEST(DcmSchedJobStandaloneTest, RemoveJobWithNullHandleDoesNothing) {
    // Should not crash
    dcmSchedRemoveJob(nullptr);
}

//
/*
extern "C" {
INT32 dcmCronParseExp(INT8 *pattern, dcmCronParseData *parseData) {
    // Simulate success for a valid pattern, failure for "fail"
    if (strcmp((const char *)pattern, "fail") == 0) {
        return DCM_FAILURE;
    }
    return DCM_SUCCESS;
}
} */

class DcmSchedStartJobTest : public ::testing::Test {
protected:
    DCMScheduler sched;

    void SetUp() override {
        memset(&sched, 0, sizeof(sched));
        pthread_mutex_init(&sched.tMutex, nullptr);
        pthread_cond_init(&sched.tCond, nullptr);
    }
    void TearDown() override {
        pthread_mutex_destroy(&sched.tMutex);
        pthread_cond_destroy(&sched.tCond);
    }
};

TEST_F(DcmSchedStartJobTest, NullHandleReturnsFailure) {
    INT32 ret = dcmSchedStartJob(nullptr, (INT8*)"* * * * *");
    EXPECT_EQ(ret, DCM_FAILURE);
}

TEST_F(DcmSchedStartJobTest, NullPatternReturnsFailure) {
    INT32 ret = dcmSchedStartJob(&sched, nullptr);
    EXPECT_EQ(ret, DCM_FAILURE);
}

TEST_F(DcmSchedStartJobTest, CronParseSuccessSetsStartSchedAndSignals) {
    sched.startSched = 0;
    INT32 ret = dcmSchedStartJob(&sched, (INT8*)"* * * * *");
    EXPECT_EQ(ret, DCM_SUCCESS);
    EXPECT_EQ(sched.startSched, 1);
}

TEST_F(DcmSchedStartJobTest, CronParseFailUnsetsStartSched) {
    sched.startSched = 1;
    INT32 ret = dcmSchedStartJob(&sched, (INT8*)"fail");
    EXPECT_EQ(ret, DCM_FAILURE);
    EXPECT_EQ(sched.startSched, 0);
}

class DcmSchedStopJobTest : public ::testing::Test {
protected:
    DCMScheduler sched;

    void SetUp() override {
        memset(&sched, 0, sizeof(sched));
        pthread_mutex_init(&sched.tMutex, nullptr);
        pthread_cond_init(&sched.tCond, nullptr);
        sched.startSched = 1;
    }
    void TearDown() override {
        pthread_mutex_destroy(&sched.tMutex);
        pthread_cond_destroy(&sched.tCond);
    }
};

TEST_F(DcmSchedStopJobTest, NullHandleReturnsFailure) {
    INT32 ret = dcmSchedStopJob(nullptr);
    EXPECT_EQ(ret, DCM_FAILURE);
}

TEST_F(DcmSchedStopJobTest, StopJobSetsStartSchedToZeroAndReturnsSuccess) {
    sched.startSched = 1;
    INT32 ret = dcmSchedStopJob(&sched);
    EXPECT_EQ(ret, DCM_SUCCESS);
    EXPECT_EQ(sched.startSched, 0);
}



// Mock callback
void* mockCallback(void* arg) { return NULL; }

// 1. Null job name
TEST(DcmSchedAddJobTest, ReturnsNullWhenJobNameIsNull) {
    void* result = dcmSchedAddJob(NULL, mockCallback, NULL);
    EXPECT_EQ(result, nullptr);
}
/*

// 2. Memory allocation failure
TEST(DcmSchedAddJobTest, ReturnsNullWhenMallocFails) {
    // Simulate malloc failure (if you have wrapper/mocking infra)
    void* result = dcmSchedAddJob((INT8*)"Job1", mockCallback, NULL);
    // Expected NULL if malloc fails internally
    EXPECT_EQ(result, nullptr);
}

// 3. Mutex initialization failure
TEST(DcmSchedAddJobTest, ReturnsNullWhenMutexInitFails) {
    // Simulate pthread_mutex_init failure
    void* result = dcmSchedAddJob((INT8*)"Job2", mockCallback, NULL);
    EXPECT_EQ(result, nullptr);
}

// 4. Condition variable initialization failure
TEST(DcmSchedAddJobTest, ReturnsNullWhenCondInitFails) {
    // Simulate pthread_cond_init failure
    void* result = dcmSchedAddJob((INT8*)"Job3", mockCallback, NULL);
    EXPECT_EQ(result, nullptr);
}

// 5. Thread creation failure
TEST(DcmSchedAddJobTest, ReturnsNullWhenThreadCreationFails) {
    // Simulate pthread_create failure
    void* result = dcmSchedAddJob((INT8*)"Job4", mockCallback, NULL);
    EXPECT_EQ(result, nullptr);
}
*/
// 6. Successful creation
TEST(DcmSchedAddJobTest, ReturnsValidHandleOnSuccess) {
    void* handle = dcmSchedAddJob((INT8*)"Job5", mockCallback, (void*)1234);
    ASSERT_NE(handle, nullptr);

    DCMScheduler* sched = (DCMScheduler*)handle;
    EXPECT_STREQ(sched->name, "Job5");
    EXPECT_EQ(sched->pDcmCB, mockCallback);
    EXPECT_EQ(sched->pUserData, (void*)1234);
    EXPECT_FALSE(sched->terminated);
    EXPECT_FALSE(sched->startSched);

    // Cleanup (important to avoid leaks or threads left running)
    pthread_cancel(sched->tId);
    pthread_join(sched->tId, NULL);
    pthread_mutex_destroy(&sched->tMutex);
    pthread_cond_destroy(&sched->tCond);
    free(sched);
}



/*
class DCMSchedRemoveJobTest : public ::testing::Test {

    void SetUp() override {
        
    }
    void TearDown() override {
        
    }
};

TEST(DCMSchedRemoveJobTest, NullHandle_ShouldNotCrash)
{
    // Should simply return when pHandle == NULL
    EXPECT_NO_THROW({
        dcmSchedRemoveJob(NULL);
    });
}

TEST(DCMSchedRemoveJobTest, ValidHandle_ShouldCleanUpResources)
{
    DCMScheduler* sched = (DCMScheduler*)malloc(sizeof(DCMScheduler));
    ASSERT_NE(sched, nullptr);

    pthread_mutex_init(&sched->tMutex, NULL);
    pthread_cond_init(&sched->tCond, NULL);
    sched->startSched = true;
    sched->terminated = false;

    // Create dummy thread
    pthread_create(&sched->tId, NULL, DummyThread, sched);

    // Call function under test
    dcmSchedRemoveJob(sched);

    // Nothing to directly assert since memory is freed,
    // but test passes if no crash / deadlock / UB occurs.
    SUCCEED();
}

TEST(DCMSchedRemoveJobTest, ThreadShouldBeJoinedAndTerminated)
{
    DCMScheduler* sched = (DCMScheduler*)malloc(sizeof(DCMScheduler));
    ASSERT_NE(sched, nullptr);

    pthread_mutex_init(&sched->tMutex, NULL);
    pthread_cond_init(&sched->tCond, NULL);
    sched->startSched = true;
    sched->terminated = false;

    // Create a dummy thread to simulate scheduler
    pthread_create(&sched->tId, NULL, DummyThread, sched);

    // Remove job
    dcmSchedRemoveJob(sched);

    // If we reached here — thread joined and memory freed correctly
    SUCCEED();
}

TEST(DCMSchedRemoveJobTest, MultipleRemoveCalls_ShouldNotCrash)
{
    DCMScheduler* sched = (DCMScheduler*)malloc(sizeof(DCMScheduler));
    ASSERT_NE(sched, nullptr);

    pthread_mutex_init(&sched->tMutex, NULL);
    pthread_cond_init(&sched->tCond, NULL);
    sched->startSched = true;
    sched->terminated = false;
    pthread_create(&sched->tId, NULL, DummyThread, sched);

    // First call should succeed
    dcmSchedRemoveJob(sched);

    // Calling again with freed pointer should be safe if handled
    // (not expected in real use, but for robustness)
    EXPECT_NO_THROW({
        dcmSchedRemoveJob(NULL);
    });
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
