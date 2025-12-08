/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 */

/**
 * @file retry_logic_gtest.cpp
 * @brief Google Test implementation for retry_logic.c
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "uploadstblogs_types.h"
#include "retry_logic.h"

// External function declarations needed by retry_logic.c
void report_upload_attempt(void);
bool is_terminal_failure(int http_code);
}

// Mock implementation for external functions
static bool g_mock_terminal_failure = false;
static int g_upload_attempt_count = 0;
static int g_t2_count_notify_calls = 0;

void report_upload_attempt(void) {
    g_upload_attempt_count++;
}

bool is_terminal_failure(int http_code) {
    // Script treats only 404 as terminal failure
    return (http_code == 404) || g_mock_terminal_failure;
}

void t2_count_notify(char* marker) {
    g_t2_count_notify_calls++;
    if (marker && strcmp(marker, "SYST_INFO_LUattempt") == 0) {
        g_upload_attempt_count++;
    }
}

// Include the actual implementation for testing
#ifdef GTEST_ENABLE
#include "../src/retry_logic.c"
#endif

// Test fixture class
class RetryLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset global state
        g_upload_attempt_count = 0;
        g_t2_count_notify_calls = 0;
        g_mock_terminal_failure = false;

        // Initialize test context
        memset(&ctx, 0, sizeof(ctx));
        ctx.retry.direct_max_attempts = 3;
        ctx.retry.codebig_max_attempts = 2;

        // Initialize test session
        memset(&session, 0, sizeof(session));
        session.direct_attempts = 0;
        session.codebig_attempts = 0;
        session.http_code = 200;

        // Reset call counters
        upload_call_count = 0;
        last_upload_result = UPLOADSTB_FAILED;
    }

    void TearDown() override {
        // Clean up any test state
    }

    // Test data
    RuntimeContext ctx;
    SessionState session;

    // Mock upload function state
    static int upload_call_count;
    static UploadResult last_upload_result;

    // Mock upload function that can be configured to succeed/fail
    static UploadResult mock_upload_success(RuntimeContext* ctx, SessionState* session, UploadPath path) {
        upload_call_count++;
        return UPLOADSTB_SUCCESS;
    }

    static UploadResult mock_upload_fail(RuntimeContext* ctx, SessionState* session, UploadPath path) {
        upload_call_count++;
        return UPLOADSTB_FAILED;
    }

    static UploadResult mock_upload_retry(RuntimeContext* ctx, SessionState* session, UploadPath path) {
        upload_call_count++;
        return UPLOADSTB_RETRY;
    }

    static UploadResult mock_upload_aborted(RuntimeContext* ctx, SessionState* session, UploadPath path) {
        upload_call_count++;
        return UPLOADSTB_ABORTED;
    }

    static UploadResult mock_upload_configurable(RuntimeContext* ctx, SessionState* session, UploadPath path) {
        upload_call_count++;
        return last_upload_result;
    }

    static UploadResult mock_upload_succeed_on_nth_call(RuntimeContext* ctx, SessionState* session, UploadPath path) {
        upload_call_count++;
        if (upload_call_count >= 2) {
            return UPLOADSTB_SUCCESS;
        }
        return UPLOADSTB_FAILED;
    }
};

// Initialize static members
int RetryLogicTest::upload_call_count = 0;
UploadResult RetryLogicTest::last_upload_result = UPLOADSTB_FAILED;

// Tests for retry_upload function
TEST_F(RetryLogicTest, RetryUpload_SuccessOnFirstTry) {
    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_success);

    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(upload_call_count, 1);
    EXPECT_EQ(session.direct_attempts, 1);
    EXPECT_EQ(session.codebig_attempts, 0);
}

TEST_F(RetryLogicTest, RetryUpload_NullContext) {
    UploadResult result = retry_upload(nullptr, &session, PATH_DIRECT, mock_upload_success);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 0);
}

TEST_F(RetryLogicTest, RetryUpload_NullSession) {
    UploadResult result = retry_upload(&ctx, nullptr, PATH_DIRECT, mock_upload_success);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 0);
}

TEST_F(RetryLogicTest, RetryUpload_NullAttemptFunction) {
    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, nullptr);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 0);
}

TEST_F(RetryLogicTest, RetryUpload_DirectPath_RetriesUntilMaxAttempts) {
    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_fail);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 3); // ctx.retry.direct_max_attempts
    EXPECT_EQ(session.direct_attempts, 3);
    EXPECT_EQ(session.codebig_attempts, 0);
}

TEST_F(RetryLogicTest, RetryUpload_CodeBigPath_RetriesUntilMaxAttempts) {
    UploadResult result = retry_upload(&ctx, &session, PATH_CODEBIG, mock_upload_fail);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 2); // ctx.retry.codebig_max_attempts
    EXPECT_EQ(session.direct_attempts, 0);
    EXPECT_EQ(session.codebig_attempts, 2);
}

TEST_F(RetryLogicTest, RetryUpload_SuccessOnSecondTry) {
    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_succeed_on_nth_call);

    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(upload_call_count, 2);
    EXPECT_EQ(session.direct_attempts, 2);
}

TEST_F(RetryLogicTest, RetryUpload_AbortedResult_NoRetry) {
    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_aborted);

    EXPECT_EQ(result, UPLOADSTB_ABORTED);
    EXPECT_EQ(upload_call_count, 1);
    EXPECT_EQ(session.direct_attempts, 1);
}

TEST_F(RetryLogicTest, RetryUpload_RetryResult_RetriesUntilMax) {
    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_retry);

    EXPECT_EQ(result, UPLOADSTB_RETRY);
    EXPECT_EQ(upload_call_count, 3); // Should retry until max attempts
    EXPECT_EQ(session.direct_attempts, 3);
}

TEST_F(RetryLogicTest, RetryUpload_InvalidPath) {
    UploadResult result = retry_upload(&ctx, &session, PATH_NONE, mock_upload_success);

    // Invalid path still makes one attempt, but should_retry prevents further retries
    // The result depends on what the upload function returns - in this case SUCCESS
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(upload_call_count, 1);
}

TEST_F(RetryLogicTest, RetryUpload_InvalidPath_WithFailure) {
    UploadResult result = retry_upload(&ctx, &session, PATH_NONE, mock_upload_fail);

    // Invalid path makes one attempt, but should_retry returns false preventing retries
    // The result is FAILED since the upload failed and no retries occurred
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 1); // Only one attempt, no retries
}

// Tests for should_retry function
TEST_F(RetryLogicTest, ShouldRetry_NullContext) {
    bool result = should_retry(nullptr, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_NullSession) {
    bool result = should_retry(&ctx, nullptr, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_SuccessResult_NoRetry) {
    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_SUCCESS);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_AbortedResult_NoRetry) {
    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_ABORTED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_NetworkFailure_HTTP000_NoRetry) {
    session.http_code = 0; // Network failure
    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_TerminalFailure_HTTP404_NoRetry) {
    session.http_code = 404; // Terminal failure (404 only per script)
    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_DirectPath_WithinAttemptLimit) {
    session.direct_attempts = 2;
    ctx.retry.direct_max_attempts = 3;
    session.http_code = 500; // Non-terminal failure

    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_TRUE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_DirectPath_ExceededAttemptLimit) {
    session.direct_attempts = 3;
    ctx.retry.direct_max_attempts = 3;
    session.http_code = 500; // Non-terminal failure

    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_CodeBigPath_WithinAttemptLimit) {
    session.codebig_attempts = 1;
    ctx.retry.codebig_max_attempts = 2;
    session.http_code = 500; // Non-terminal failure

    bool result = should_retry(&ctx, &session, PATH_CODEBIG, UPLOADSTB_FAILED);
    EXPECT_TRUE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_CodeBigPath_ExceededAttemptLimit) {
    session.codebig_attempts = 2;
    ctx.retry.codebig_max_attempts = 2;
    session.http_code = 500; // Non-terminal failure

    bool result = should_retry(&ctx, &session, PATH_CODEBIG, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_RetryResult_WithinLimit) {
    session.direct_attempts = 1;
    ctx.retry.direct_max_attempts = 3;
    session.http_code = 500; // Non-terminal failure

    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_RETRY);
    EXPECT_TRUE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_InvalidPath) {
    bool result = should_retry(&ctx, &session, PATH_NONE, UPLOADSTB_FAILED);
    EXPECT_FALSE(result);
}

TEST_F(RetryLogicTest, ShouldRetry_NonTerminalHttpCodes) {
    session.http_code = 500; // Server error - should retry
    bool result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_TRUE(result);

    session.http_code = 503; // Service unavailable - should retry
    result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_TRUE(result);

    session.http_code = 408; // Timeout - should retry
    result = should_retry(&ctx, &session, PATH_DIRECT, UPLOADSTB_FAILED);
    EXPECT_TRUE(result);
}

// Tests for increment_attempts function
TEST_F(RetryLogicTest, IncrementAttempts_NullSession) {
    increment_attempts(nullptr, PATH_DIRECT);
    // Should not crash, just return
}

TEST_F(RetryLogicTest, IncrementAttempts_DirectPath) {
    session.direct_attempts = 0;
    session.codebig_attempts = 0;

    increment_attempts(&session, PATH_DIRECT);

    EXPECT_EQ(session.direct_attempts, 1);
    EXPECT_EQ(session.codebig_attempts, 0);
}

TEST_F(RetryLogicTest, IncrementAttempts_CodeBigPath) {
    session.direct_attempts = 0;
    session.codebig_attempts = 0;

    increment_attempts(&session, PATH_CODEBIG);

    EXPECT_EQ(session.direct_attempts, 0);
    EXPECT_EQ(session.codebig_attempts, 1);
}

TEST_F(RetryLogicTest, IncrementAttempts_InvalidPath) {
    session.direct_attempts = 0;
    session.codebig_attempts = 0;

    increment_attempts(&session, PATH_NONE);

    // Should not increment any counter
    EXPECT_EQ(session.direct_attempts, 0);
    EXPECT_EQ(session.codebig_attempts, 0);
}

TEST_F(RetryLogicTest, IncrementAttempts_MultipleIncrements) {
    session.direct_attempts = 0;
    session.codebig_attempts = 0;

    increment_attempts(&session, PATH_DIRECT);
    increment_attempts(&session, PATH_DIRECT);
    increment_attempts(&session, PATH_CODEBIG);

    EXPECT_EQ(session.direct_attempts, 2);
    EXPECT_EQ(session.codebig_attempts, 1);
}

// Integration tests
TEST_F(RetryLogicTest, Integration_RetryLogicWithTelemetry) {
    // Test that telemetry is reported for each attempt
    g_upload_attempt_count = 0;

    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_fail);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(g_upload_attempt_count, 3); // Should report telemetry for each attempt
}

TEST_F(RetryLogicTest, Integration_TerminalFailurePreventsRetry) {
    // Set up mock to report terminal failure
    g_mock_terminal_failure = true;
    session.http_code = 500; // Non-404 code, but mock will say it's terminal

    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_fail);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 1); // Should not retry on terminal failure
}

TEST_F(RetryLogicTest, Integration_NetworkFailurePreventsRetry) {
    session.http_code = 0; // Network failure

    UploadResult result = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_fail);

    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(upload_call_count, 1); // Should not retry on network failure
}

TEST_F(RetryLogicTest, Integration_MixedPathAttempts) {
    // Test that attempts are tracked separately for different paths
    ctx.retry.direct_max_attempts = 2;
    ctx.retry.codebig_max_attempts = 3;

    // Try direct path first
    UploadResult result1 = retry_upload(&ctx, &session, PATH_DIRECT, mock_upload_fail);
    EXPECT_EQ(result1, UPLOADSTB_FAILED);
    EXPECT_EQ(session.direct_attempts, 2);
    EXPECT_EQ(session.codebig_attempts, 0);

    // Now try CodeBig path
    upload_call_count = 0; // Reset for second test
    UploadResult result2 = retry_upload(&ctx, &session, PATH_CODEBIG, mock_upload_fail);
    EXPECT_EQ(result2, UPLOADSTB_FAILED);
    EXPECT_EQ(session.direct_attempts, 2); // Should remain unchanged
    EXPECT_EQ(session.codebig_attempts, 3);
}

// Entry point for the test executable
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
