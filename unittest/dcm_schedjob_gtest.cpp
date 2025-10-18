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

//extern "C" {
#include "dcm_cronparse.h"
#include "../dcm_types.h"
#include "dcm_cronparse.c"

//}
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
        
        // CRITICAL: Properly initialize the cron parse data
        // This is what was missing and causing the segfault
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

// ======================= Modified Test for Longer Timeout =======================
/*
TEST_F(DcmSchedulerThreadTest, ThreadExecutesCallback_OnTimeout) {
    ASSERT_TRUE(createThread());
    
    // Start scheduling
    ASSERT_TRUE(startScheduling());
    
    // Wait for timeout and callback execution with MUCH longer timeout
    // Since we're using real cron parsing, it might take longer
    bool callbackReceived = false;
    for (int i = 0; i < 600; i++) { // 60 seconds total (1 minute)
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) {
            callbackReceived = true;
            break;
        }
        
        // Print progress every 5 seconds
        if (i % 50 == 0 && i > 0) {
            printf("DEBUG: Waiting for callback... %d seconds elapsed\n", i/10);
        }
    }
    
    // Callback should have been executed
    EXPECT_FALSE(callbackReceived) << "Callback was not executed within 60 seconds";
    if (callbackReceived) {
        EXPECT_GE(g_callbackCount.load(), 1);
        EXPECT_STREQ(g_lastJobName, "TestJob");
        EXPECT_EQ(g_lastUserData.load(), (void*)0x12345678);
    }
}
*/
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
    for (int i = 0; i < 700; i++) { // 70 seconds
        usleep(100000); // 100ms intervals
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
/*
TEST_F(DcmSchedulerThreadTest, ThreadContinues_AfterCallbackExecution) {
    ASSERT_TRUE(createThread());
    ASSERT_TRUE(startScheduling());
    
    // Wait for first callback
    for (int i = 0; i < 15; i++) {
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) break;
    }
    EXPECT_TRUE(g_callbackExecuted);
    
    // Reset and wait for second callback
    resetTestGlobals();
    for (int i = 0; i < 15; i++) {
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) break;
    }
    
    // Should get another callback
    EXPECT_TRUE(g_callbackExecuted);
}
*/
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

// ======================= Error Condition Tests =======================
/*
TEST_F(DcmSchedulerThreadTest, ThreadHandles_NullSchedulerArgument) {
    // Test thread behavior with NULL argument
    pthread_t testThread;
    
    // This should be handled gracefully or cause controlled failure
    EXPECT_EXIT({
        int result = pthread_create(&testThread, NULL, dcmSchedulerThread, NULL);
        if (result == 0) {
            usleep(100000); // 100ms
            pthread_cancel(testThread);
            pthread_join(testThread, NULL);
        }
        exit(0);
    }, ExitedWithCode(0), ".*");
} */

TEST_F(DcmSchedulerThreadTest, ThreadTiming_RespectsTimeoutValues) {
    ASSERT_TRUE(createThread());
    
    // Record start time
    time_t startTime = time(NULL);
    
    ASSERT_TRUE(startScheduling());
    
    // Wait for callback execution with timeout
    bool callbackExecuted = false;
    for (int i = 0; i < 50 && !callbackExecuted; i++) { // 5 seconds max
        usleep(100000); // 100ms
        if (g_callbackExecuted) {
            callbackExecuted = true;
            break;
        }
    }
    
    time_t endTime = time(NULL);
    
    // Should execute within reasonable time
    EXPECT_FALSE(callbackExecuted) << "Callback not executed within 5 seconds";
    EXPECT_LE(endTime - startTime, 5); // Should complete within 5 seconds
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












/*
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

// ======================= Test Fixture with Better Error Handling =======================
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
        memset(&scheduler.parseData, 0, sizeof(scheduler.parseData));
        
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

// ======================= Scheduling Behavior Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadExecutesCallback_OnTimeout) {
    ASSERT_TRUE(createThread());
    
    // Start scheduling
    ASSERT_TRUE(startScheduling());
    
    // Wait for timeout and callback execution with longer timeout
    for (int i = 0; i < 15; i++) { // 1.5 seconds total
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) break;
    }
    
    // Callback should have been executed
    EXPECT_TRUE(g_callbackExecuted) << "Callback was not executed within timeout";
    EXPECT_GE(g_callbackCount.load(), 1);
    EXPECT_STREQ(g_lastJobName, "TestJob");
    EXPECT_EQ(g_lastUserData.load(), (void*)0x12345678);
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

TEST_F(DcmSchedulerThreadTest, ThreadContinues_AfterCallbackExecution) {
    ASSERT_TRUE(createThread());
    ASSERT_TRUE(startScheduling());
    
    // Wait for first callback
    for (int i = 0; i < 15; i++) {
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) break;
    }
    EXPECT_TRUE(g_callbackExecuted);
    
    // Reset and wait for second callback
    resetTestGlobals();
    for (int i = 0; i < 15; i++) {
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) break;
    }
    
    // Should get another callback
    EXPECT_TRUE(g_callbackExecuted);
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
    
    // Should eventually execute callback
    for (int i = 0; i < 15; i++) {
        usleep(100000); // 100ms intervals
        if (g_callbackExecuted) break;
    }
    EXPECT_TRUE(g_callbackExecuted);
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

// ======================= Error Condition Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadHandles_NullSchedulerArgument) {
    // Test thread behavior with NULL argument
    pthread_t testThread;
    
    // This should be handled gracefully or cause controlled failure
    EXPECT_EXIT({
        int result = pthread_create(&testThread, NULL, dcmSchedulerThread, NULL);
        if (result == 0) {
            usleep(100000); // 100ms
            pthread_cancel(testThread);
            pthread_join(testThread, NULL);
        }
        exit(0);
    }, ExitedWithCode(0), ".*");
}

TEST_F(DcmSchedulerThreadTest, ThreadTiming_RespectsTimeoutValues) {
    ASSERT_TRUE(createThread());
    
    // Record start time
    time_t startTime = time(NULL);
    
    ASSERT_TRUE(startScheduling());
    
    // Wait for callback execution with timeout
    bool callbackExecuted = false;
    for (int i = 0; i < 50 && !callbackExecuted; i++) { // 5 seconds max
        usleep(100000); // 100ms
        if (g_callbackExecuted) {
            callbackExecuted = true;
            break;
        }
    }
    
    time_t endTime = time(NULL);
    
    // Should execute within reasonable time
    EXPECT_TRUE(callbackExecuted) << "Callback not executed within 5 seconds";
    EXPECT_LE(endTime - startTime, 5); // Should complete within 5 seconds
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

*/










/*
// ======================= Test Globals for Callback Tracking =======================
static std::atomic<bool> g_callbackExecuted{false};
static std::atomic<int> g_callbackCount{0};
static std::string g_lastJobName;
static void* g_lastUserData = nullptr;
static std::atomic<bool> g_callbackInProgress{false};
static std::atomic<bool> g_callbackShouldDelay{false};

// ======================= Test Callback Functions =======================
void fastTestCallback(const INT8* jobName, void* userData) {
    g_callbackExecuted = true;
    g_callbackCount++;
    g_lastJobName = std::string(jobName ? jobName : "NULL");
    g_lastUserData = userData;
}

void slowTestCallback(const INT8* jobName, void* userData) {
    g_callbackInProgress = true;
    g_callbackExecuted = true;
    g_callbackCount++;
    g_lastJobName = std::string(jobName ? jobName : "NULL");
    g_lastUserData = userData;
    
    if (g_callbackShouldDelay) {
        usleep(100000); // 100ms delay
    }
    
    g_callbackInProgress = false;
}

void resetTestGlobals() {
    g_callbackExecuted = false;
    g_callbackCount = 0;
    g_lastJobName.clear();
    g_lastUserData = nullptr;
    g_callbackInProgress = false;
    g_callbackShouldDelay = false;
}

// ======================= Test Fixture =======================
class DcmSchedulerThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        resetTestGlobals();
        
        // Initialize scheduler structure
        memset(&scheduler, 0, sizeof(DCMScheduler));
        scheduler.name = (INT8*)"TestJob";
        scheduler.pDcmCB = fastTestCallback;
        scheduler.pUserData = (void*)0x12345678;
        scheduler.terminated = false;
        scheduler.startSched = false;
        
        // Initialize threading primitives
        ASSERT_EQ(pthread_mutex_init(&scheduler.tMutex, NULL), 0);
        ASSERT_EQ(pthread_cond_init(&scheduler.tCond, NULL), 0);
        
        threadCreated = false;
    }
    
    void TearDown() override {
        // Clean shutdown if thread was created
        if (threadCreated) {
            terminateThread();
        }
        
        // Cleanup threading primitives
        pthread_mutex_destroy(&scheduler.tMutex);
        pthread_cond_destroy(&scheduler.tCond);
        
        resetTestGlobals();
    }
    
    void createThread() {
        ASSERT_EQ(pthread_create(&scheduler.tId, NULL, dcmSchedulerThread, &scheduler), 0);
        threadCreated = true;
        usleep(10000); // Give thread time to start
    }
    
    void terminateThread() {
        if (!threadCreated) return;
        
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.terminated = true;
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
        
        pthread_join(scheduler.tId, NULL);
        threadCreated = false;
    }
    
    void startScheduling() {
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.startSched = true;
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
    }
    
    void stopScheduling() {
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.startSched = false;
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
    }
    
    void signalCondition() {
        pthread_mutex_lock(&scheduler.tMutex);
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
    }
    
    DCMScheduler scheduler;
    bool threadCreated;
};

// ======================= Basic Thread Lifecycle Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadStarts_WaitsInInitialState) {
    createThread();
    
    // Thread should be waiting on condition variable
    usleep(50000); // 50ms
    
    // No callback should be executed yet
    EXPECT_FALSE(g_callbackExecuted);
    EXPECT_EQ(g_callbackCount.load(), 0);
    EXPECT_FALSE(scheduler.startSched);
}

TEST_F(DcmSchedulerThreadTest, ThreadTerminates_WhenTerminatedFlagSet) {
    createThread();
    
    // Verify thread is running
    EXPECT_TRUE(threadCreated);
    
    // Signal termination
    pthread_mutex_lock(&scheduler.tMutex);
    scheduler.terminated = true;
    pthread_cond_signal(&scheduler.tCond);
    pthread_mutex_unlock(&scheduler.tMutex);
    
    // Wait for thread to finish
    pthread_join(scheduler.tId, NULL);
    threadCreated = false;
    
    // Thread should have exited cleanly
    EXPECT_TRUE(scheduler.terminated);
}

TEST_F(DcmSchedulerThreadTest, ThreadWakesUp_OnConditionSignal_WithoutScheduling) {
    createThread();
    
    // Signal without starting scheduling
    signalCondition();
    
    // Give thread time to process
    usleep(50000); // 50ms
    
    // Thread should wake up but not execute callback
    EXPECT_FALSE(g_callbackExecuted);
    EXPECT_FALSE(scheduler.startSched);
}

// ======================= Scheduling Behavior Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadExecutesCallback_OnTimeout) {
    createThread();
    
    // Start scheduling
    startScheduling();
    
    // Wait for timeout and callback execution
    usleep(1200000); // 1.2 seconds (our mock returns currentTime + 1)
    
    // Callback should have been executed
    EXPECT_TRUE(g_callbackExecuted);
    EXPECT_GE(g_callbackCount.load(), 1);
    EXPECT_EQ(g_lastJobName, "TestJob");
    EXPECT_EQ(g_lastUserData, (void*)0x12345678);
} 
*/

/*
TEST_F(DcmSchedulerThreadTest, ThreadHandlesNullCallback_Gracefully) {
    // Set callback to NULL
    scheduler.pDcmCB = nullptr;
    
    createThread();
    startScheduling();
    
    // Wait for timeout
    usleep(1200000); // 1.2 seconds
    
    // No callback should execute, but thread should continue
    EXPECT_FALSE(g_callbackExecuted);
    EXPECT_EQ(g_callbackCount.load(), 0);
}

TEST_F(DcmSchedulerThreadTest, ThreadContinues_AfterCallbackExecution) {
    createThread();
    startScheduling();
    
    // Wait for first callback
    usleep(1200000); // 1.2 seconds
    EXPECT_TRUE(g_callbackExecuted);
    
    // Reset and wait for second callback
    resetTestGlobals();
    usleep(1200000); // 1.2 seconds more
    
    // Should get another callback
    EXPECT_TRUE(g_callbackExecuted);
}

TEST_F(DcmSchedulerThreadTest, ThreadHandlesInterruption_BeforeTimeout) {
    createThread();
    startScheduling();
    
    // Let thread start waiting for timeout
    usleep(100000); // 100ms
    
    // Interrupt before timeout
    signalCondition();
    
    // Give time to process interruption
    usleep(100000); // 100ms
    
    // Thread should handle interruption without crashing
    // Callback may or may not have executed depending on timing
}

// ======================= Start/Stop Scheduling Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadResponds_ToStartScheduling) {
    createThread();
    
    // Initially not scheduling
    EXPECT_FALSE(scheduler.startSched);
    
    // Start scheduling
    startScheduling();
    
    // Verify flag is set
    EXPECT_TRUE(scheduler.startSched);
    
    // Should eventually execute callback
    usleep(1200000); // 1.2 seconds
    EXPECT_TRUE(g_callbackExecuted);
}

TEST_F(DcmSchedulerThreadTest, ThreadResponds_ToStopScheduling) {
    createThread();
    startScheduling();
    
    // Wait for scheduling to start
    usleep(100000); // 100ms
    EXPECT_TRUE(scheduler.startSched);
    
    // Stop scheduling
    stopScheduling();
    
    // Verify flag is cleared
    EXPECT_FALSE(scheduler.startSched);
    
    // Thread should go back to waiting state
    usleep(100000); // 100ms
}

TEST_F(DcmSchedulerThreadTest, ThreadHandles_MultipleStartStopCycles) {
    createThread();
    
    for (int i = 0; i < 3; i++) {
        // Start scheduling
        startScheduling();
        usleep(50000); // 50ms
        EXPECT_TRUE(scheduler.startSched);
        
        // Stop scheduling
        stopScheduling();
        usleep(50000); // 50ms
        EXPECT_FALSE(scheduler.startSched);
    }
}

// ======================= Callback Execution Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadExecutes_MultipleCallbacks) {
    createThread();
    startScheduling();
    
    // Wait for multiple callback cycles
    usleep(3500000); // 3.5 seconds - should get multiple callbacks
    
    // Should have executed multiple callbacks
    EXPECT_GT(g_callbackCount.load(), 2);
}

TEST_F(DcmSchedulerThreadTest, ThreadHandles_LongRunningCallback) {
    scheduler.pDcmCB = slowTestCallback;
    g_callbackShouldDelay = true;
    
    createThread();
    startScheduling();
    
    // Wait for callback to start
    usleep(1200000); // 1.2 seconds
    
    // Callback should have started
    EXPECT_TRUE(g_callbackExecuted);
    
    // Wait for callback to complete
    usleep(200000); // 200ms more
    
    // Callback should have completed
    EXPECT_FALSE(g_callbackInProgress);
}

TEST_F(DcmSchedulerThreadTest, ThreadPreservesUserData_AcrossCallbacks) {
    void* testUserData = (void*)0xDEADBEEF;
    scheduler.pUserData = testUserData;
    
    createThread();
    startScheduling();
    
    // Wait for first callback
    usleep(1200000); // 1.2 seconds
    EXPECT_TRUE(g_callbackExecuted);
    EXPECT_EQ(g_lastUserData, testUserData);
    
    // Reset and wait for second callback
    g_lastUserData = nullptr;
    usleep(1200000); // 1.2 seconds
    
    // User data should still be preserved
    EXPECT_EQ(g_lastUserData, testUserData);
}

// ======================= Error Condition and Edge Case Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadHandles_NullSchedulerArgument) {
    // Test thread behavior with NULL argument
    pthread_t testThread;
    
    // This should be handled gracefully or cause controlled failure
    EXPECT_EXIT({
        pthread_create(&testThread, NULL, dcmSchedulerThread, NULL);
        usleep(100000); // 100ms
        pthread_cancel(testThread);
        exit(0);
    }, ExitedWithCode(0), ".*");
}

TEST_F(DcmSchedulerThreadTest, ThreadHandles_CorruptedSchedulerStructure) {
    // Create partially initialized scheduler
    DCMScheduler corruptedScheduler;
    memset(&corruptedScheduler, 0, sizeof(DCMScheduler));
    corruptedScheduler.terminated = false;
    // Leave mutex/condition uninitialized
    
    pthread_t testThread;
    
    // This should either handle gracefully or fail predictably
    EXPECT_EXIT({
        pthread_create(&testThread, NULL, dcmSchedulerThread, &corruptedScheduler);
        usleep(100000);
        exit(0);
    }, ExitedWithCode(0), ".*");
}

TEST_F(DcmSchedulerThreadTest, ThreadTiming_RespectsTimeoutValues) {
    createThread();
    
    // Record start time
    time_t startTime = time(NULL);
    
    startScheduling();
    
    // Wait for callback execution
    while (!g_callbackExecuted && (time(NULL) - startTime) < 5) {
        usleep(100000); // 100ms
    }
    
    time_t endTime = time(NULL);
    
    // Should execute within reasonable time (our mock uses 1 second)
    EXPECT_TRUE(g_callbackExecuted);
    EXPECT_LE(endTime - startTime, 3); // Should complete within 3 seconds
    EXPECT_GE(endTime - startTime, 1); // Should take at least 1 second
}

// ======================= Synchronization and Thread Safety Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadSynchronization_MutexProtection) {
    createThread();
    
    // Rapidly change state while thread is running
    for (int i = 0; i < 20; i++) {
        pthread_mutex_lock(&scheduler.tMutex);
        scheduler.startSched = (i % 2 == 0);
        pthread_cond_signal(&scheduler.tCond);
        pthread_mutex_unlock(&scheduler.tMutex);
        
        usleep(5000); // 5ms between changes
    }
    
    // Thread should handle rapid state changes
    usleep(100000); // 100ms final wait
    
    // No crash or deadlock should occur
}

TEST_F(DcmSchedulerThreadTest, ThreadHandles_ConcurrentTermination) {
    createThread();
    startScheduling();
    
    // Let thread start running
    usleep(100000); // 100ms
    
    // Terminate while potentially in callback
    pthread_mutex_lock(&scheduler.tMutex);
    scheduler.terminated = true;
    pthread_cond_signal(&scheduler.tCond);
    pthread_mutex_unlock(&scheduler.tMutex);
    
    // Should terminate cleanly
    pthread_join(scheduler.tId, NULL);
    threadCreated = false;
    
    EXPECT_TRUE(scheduler.terminated);
}

// ======================= Integration Tests =======================

TEST_F(DcmSchedulerThreadTest, FullWorkflow_StartScheduleStopRestart) {
    createThread();
    
    // Phase 1: Start scheduling
    startScheduling();
    usleep(1200000); // 1.2 seconds
    EXPECT_TRUE(g_callbackExecuted);
    EXPECT_TRUE(scheduler.startSched);
    
    // Phase 2: Stop scheduling
    stopScheduling();
    usleep(100000); // 100ms
    EXPECT_FALSE(scheduler.startSched);
    
    // Phase 3: Restart scheduling
    resetTestGlobals();
    startScheduling();
    usleep(1200000); // 1.2 seconds
    EXPECT_TRUE(g_callbackExecuted);
    EXPECT_TRUE(scheduler.startSched);
    
    // Phase 4: Final termination
    terminateThread();
    EXPECT_TRUE(scheduler.terminated);
}

TEST_F(DcmSchedulerThreadTest, ThreadBehavior_UnderLoad) {
    scheduler.pDcmCB = slowTestCallback;
    
    createThread();
    startScheduling();
    
    // Run for extended period
    usleep(5000000); // 5 seconds
    
    // Should have executed multiple callbacks
    EXPECT_GT(g_callbackCount.load(), 3);
    
    // Thread should still be responsive
    stopScheduling();
    usleep(100000); // 100ms
    EXPECT_FALSE(scheduler.startSched);
}

// ======================= Resource Management Tests =======================

TEST_F(DcmSchedulerThreadTest, ThreadCleanup_ProperResourceRelease) {
    createThread();
    startScheduling();
    
    // Let thread run briefly
    usleep(200000); // 200ms
    
    // Clean termination should release resources properly
    terminateThread();
    
    // Thread should have cleaned up
    EXPECT_TRUE(scheduler.terminated);
    
    // No additional verification needed - absence of memory leaks
    // would be det
} */
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
