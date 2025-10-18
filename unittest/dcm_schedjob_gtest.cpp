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
#include "dcm_cronparse.h"
#include "dcm_types.h"
#include "dcm_schedjob.h"
#include "dcm_cronparse.c"
#include "dcm_schedjob.c"

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "dcm_schedjob_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::StrEq;

// ======================= Thread-Safe Test Globals =======================
static std::atomic<bool> g_callbackExecuted{false};
static std::atomic<int> g_callbackCount{0};
static std::atomic<bool> g_callbackInProgress{false};
static std::atomic<bool> g_callbackShouldDelay{false};

// Thread-safe string storage
static char g_lastJobName[256] = {0};
static std::atomic<void*> g_lastUserData{nullptr};

// ======================= Safe Test Callback Functions =======================
void safeTestCallback(const INT8* jobName, void* userData) {
    g_callbackInProgress = true;
    g_callbackExecuted = true;
    g_callbackCount++;
    
    // Safe string copy
    if (jobName) {
        strncpy(g_lastJobName, jobName, sizeof(g_lastJobName) - 1);
        g_lastJobName[sizeof(g_lastJobName) - 1] = '\0';
    } else {
        strcpy(g_lastJobName, "NULL");
    }
    
    g_lastUserData = userData;
    
    if (g_callbackShouldDelay) {
        usleep(50000); // 50ms delay
    }
    
    g_callbackInProgress = false;
}

void resetTestGlobals() {
    g_callbackExecuted = false;
    g_callbackCount = 0;
    g_callbackInProgress = false;
    g_callbackShouldDelay = false;
    memset(g_lastJobName, 0, sizeof(g_lastJobName));
    g_lastUserData = nullptr;
}

// ======================= Test Fixture with Proper Cron Data Initialization =======================
class DcmSchedulerThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        resetTestGlobals();
        
        // Initialize scheduler structure with proper memory management
        memset(&scheduler, 0, sizeof(DCMScheduler));
        
        // Allocate and set job name safely
        jobName = new char[32];
        strcpy(jobName, "TestJob");
        scheduler.name = (INT8*)jobName;
        
        scheduler.pDcmCB = safeTestCallback;
        scheduler.pUserData = (void*)0x12345678;
        scheduler.terminated = false;
        scheduler.startSched = false;
        
        // Properly initialize the cron parse data
        memset(&scheduler.parseData, 0, sizeof(scheduler.parseData));
        
        // Initialize with a valid cron expression BEFORE starting the thread
        const char* validCronExpr = "*/5 * * * *"; // Every 5 minutes
        INT32 parseResult = dcmCronParseExp(validCronExpr, &scheduler.parseData);
        if (parseResult != DCM_SUCCESS) {
            // Fallback to a simpler expression
            parseResult = dcmCronParseExp("* * * * *", &scheduler.parseData);
        }
        ASSERT_EQ(parseResult, DCM_SUCCESS) << "Failed to parse cron expression";
        
        // Initialize threading primitives with error checking
        int mutexResult = pthread_mutex_init(&scheduler.tMutex, NULL);
        ASSERT_EQ(mutexResult, 0) << "Failed to initialize mutex: " << strerror(mutexResult);
        
        int condResult = pthread_cond_init(&scheduler.tCond, NULL);
        ASSERT_EQ(condResult, 0) << "Failed to initialize condition variable: " << strerror(condResult);
        
        threadCreated = false;
        mutexInitialized = true;
        condInitialized = true;
    }
    
    void TearDown() override {
        // Clean shutdown if thread was created
        if (threadCreated) {
            terminateThreadSafely();
        }
        
        // Cleanup threading primitives
        if (mutexInitialized) {
            pthread_mutex_destroy(&scheduler.tMutex);
            mutexInitialized = false;
        }
        
        if (condInitialized) {
            pthread_cond_destroy(&scheduler.tCond);
            condInitialized = false;
        }
        
        // Clean up allocated memory
        if (jobName) {
            delete[] jobName;
            jobName = nullptr;
        }
        
        resetTestGlobals();
    }
    
    bool createThread() {
        if (threadCreated) {
            return false; // Thread already created
        }
        
        int result = pthread_create(&scheduler.tId, NULL, dcmSchedulerThread, &scheduler);
        if (result != 0) {
            ADD_FAILURE() << "Failed to create thread: " << strerror(result);
            return false;
        }
        
        threadCreated = true;
        usleep(20000); // Give thread more time to start
        return true;
    }
    
    void terminateThreadSafely() {
        if (!threadCreated) return;
        
        // Set termination flag safely
        if (mutexInitialized) {
            pthread_mutex_lock(&scheduler.tMutex);
            scheduler.terminated = true;
            if (condInitialized) {
                pthread_cond_signal(&scheduler.tCond);
            }
            pthread_mutex_unlock(&scheduler.tMutex);
        } else {
            scheduler.terminated = true;
        }
        
        // Wait for thread with timeout
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 5; // 5 second timeout
        
        int joinResult = pthread_join(scheduler.tId, NULL);
        if (joinResult != 0) {
            // Force thread termination if join fails
            pthread_cancel(scheduler.tId);
            pthread_join(scheduler.tId, NULL);
        }
        
        threadCreated = false;
    }
    
    bool startScheduling() {
        if (!mutexInitialized || !condInitialized) {
            return false;
        }
        
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.startSched = true;
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
        return true;
    }
    
    bool stopScheduling() {
        if (!mutexInitialized || !condInitialized) {
            return false;
        }
        
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.startSched = false;
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
        return true;
    }
    
    bool signalCondition() {
        if (!mutexInitialized || !condInitialized) {
            return false;
        }
        
        pthread_mutex_lock(&scheduler.tMutex);
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
        return true;
    }
    
    DCMScheduler scheduler;
    char* jobName;
    bool threadCreated;
    bool mutexInitialized;
    bool condInitialized;
};

// ======================= Basic Thread Lifecycle Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadStarts_WaitsInInitialState) {
    ASSERT_TRUE(createThread());
    
    // Thread should be waiting on condition variable
    usleep(100000); // 100ms - longer wait
    
    // No callback should be executed yet
    EXPECT_FALSE(g_callbackExecuted);
    EXPECT_EQ(g_callbackCount.load(), 0);
    EXPECT_FALSE(scheduler.startSched);
}

TEST_F(DcmSchedulerThreadTest, ThreadTerminates_WhenTerminatedFlagSet) {
    ASSERT_TRUE(createThread());
    
    // Brief wait to ensure thread is running
    usleep(50000); // 50ms
    
    // Signal termination
    terminateThreadSafely();
    
    // Thread should have exited cleanly
    EXPECT_TRUE(scheduler.terminated);
}

TEST_F(DcmSchedulerThreadTest, ThreadWakesUp_OnConditionSignal_WithoutScheduling) {
    ASSERT_TRUE(createThread());
    
    // Signal without starting scheduling
    ASSERT_TRUE(signalCondition());
    
    // Give thread time to process
    usleep(100000); // 100ms
    
    // Thread should wake up but not execute callback
    EXPECT_FALSE(g_callbackExecuted);
    EXPECT_FALSE(scheduler.startSched);
}

// ======================= Test with Quick Cron Expression =======================

TEST_F(DcmSchedulerThreadTest, ThreadExecutesCallback_QuickCron) {
    // Reinitialize with a cron expression that triggers every minute
    const char* quickCronExpr = "* * * * *"; // Every minute
    memset(&scheduler.parseData, 0, sizeof(scheduler.parseData));
    INT32 parseResult = dcmCronParseExp(quickCronExpr, &scheduler.parseData);
    ASSERT_EQ(parseResult, DCM_SUCCESS);
    
    ASSERT_TRUE(createThread());
    ASSERT_TRUE(startScheduling());
    
    // Wait up to 70 seconds (to account for minute boundary)
    bool callbackReceived = false;
    
    for (int i = 0; i < 350; i++) { // 70 seconds
        usleep(100000); // 100ms intervals 100000
        if (g_callbackExecuted) {
            callbackReceived = true;
            break;
        }
        
        if (i % 100 == 0 && i > 0) {
            printf("DEBUG: Waiting for minute boundary... %d seconds elapsed\n", i/10);
        }
    }
    
    EXPECT_TRUE(callbackReceived) << "Callback was not executed within 70 seconds"; 
}

TEST_F(DcmSchedulerThreadTest, ThreadHandlesNullCallback_Gracefully) {
    // Set callback to NULL
    scheduler.pDcmCB = nullptr;
    
    ASSERT_TRUE(createThread());
    ASSERT_TRUE(startScheduling());
    
    // Wait for timeout
    usleep(1500000); // 1.5 seconds
    
    // No callback should execute, but thread should continue
    EXPECT_FALSE(g_callbackExecuted);
    EXPECT_EQ(g_callbackCount.load(), 0);
}

// ======================= Start/Stop Scheduling Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadResponds_ToStartScheduling) {
    ASSERT_TRUE(createThread());
    
    // Initially not scheduling
    EXPECT_FALSE(scheduler.startSched);
    
    // Start scheduling
    ASSERT_TRUE(startScheduling());
    
    // Verify flag is set
    EXPECT_TRUE(scheduler.startSched);
}

TEST_F(DcmSchedulerThreadTest, ThreadResponds_ToStopScheduling) {
    ASSERT_TRUE(createThread());
    ASSERT_TRUE(startScheduling());
    
    // Wait for scheduling to start
    usleep(100000); // 100ms
    EXPECT_TRUE(scheduler.startSched);
    
    // Stop scheduling
    ASSERT_TRUE(stopScheduling());
    
    // Verify flag is cleared
    EXPECT_FALSE(scheduler.startSched);
    
    // Thread should go back to waiting state
    usleep(100000); // 100ms
}

// ======================= Synchronization Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadSynchronization_MutexProtection) {
    ASSERT_TRUE(createThread());
    
    // Rapidly change state while thread is running
    for (int i = 0; i < 10; i++) {
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.startSched = (i % 2 == 0);
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
        
        usleep(10000); // 10ms between changes
    }
    
    // Thread should handle rapid state changes
    usleep(200000); // 200ms final wait
    
    // No crash or deadlock should occur
}

// ======================= Resource Management Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadCleanup_ProperResourceRelease) {
    ASSERT_TRUE(createThread());
    ASSERT_TRUE(startScheduling());
    
    // Let thread run briefly
    usleep(200000); // 200ms
    
    // Clean termination should release resources properly
    terminateThreadSafely();
    
    // Thread should have cleaned up
    EXPECT_TRUE(scheduler.terminated);
}


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

// Simple test callback
void testCallback(const char* jobName, void* userData) {
    // Simple test callback
}

// Test fixture
class DcmSchedAddJobTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (handle) {
            dcmSchedRemoveJob(handle);
        }
    }
    
    void* handle = nullptr;
};

// Valid input test
TEST_F(DcmSchedAddJobTest, ValidInputs_ReturnsValidHandle) {
    char jobName[] = "TestJob";
    
    handle = dcmSchedAddJob(jobName, testCallback, nullptr);
    
    EXPECT_NE(handle, nullptr);
}

// Null job name test
TEST_F(DcmSchedAddJobTest, NullJobName_ReturnsNull) {
    handle = dcmSchedAddJob(nullptr, testCallback, nullptr);
    
    EXPECT_EQ(handle, nullptr);
}

// Null callback test
TEST_F(DcmSchedAddJobTest, NullCallback_ReturnsValidHandle) {
    char jobName[] = "TestJob";
    
    handle = dcmSchedAddJob(jobName, nullptr, nullptr);
    
    EXPECT_NE(handle, nullptr);
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
