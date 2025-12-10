/**
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
// Mock curl_easy_strerror to avoid conflict with actual curl header
#endif

#include "uploadstblogs_types.h"

// Mock external dependencies
#ifdef GTEST_ENABLE
extern "C" {
// Mock curl_easy_strerror implementation
const char* mock_curl_easy_strerror_impl(int curl_code) {
    switch (curl_code) {
        case 0: return "No error"; // CURLE_OK
        case 7: return "Couldn't connect to server"; // CURLE_COULDNT_CONNECT  
        case 28: return "Timeout was reached"; // CURLE_OPERATION_TIMEDOUT
        case 35: return "SSL connect error"; // CURLE_SSL_CONNECT_ERROR
        case 60: return "SSL peer certificate or SSH remote key was not OK"; // CURLE_SSL_CACERT
        default: return "Unknown error";
    }
}
}
#endif

// Include the actual verification implementation
#include "verification.h"
#include "../src/verification.c"

using namespace testing;

class VerificationTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&session, 0, sizeof(SessionState));
        
        // Set up default session values
        strcpy(session.archive_file, "/tmp/test_archive.tar.gz");
        session.strategy = STRAT_DCM;
        session.http_code = 200;  // Default to success
        session.curl_code = 0;    // CURLE_OK
        session.success = false;
        session.used_fallback = false;
    }
    
    void TearDown() override {}
    
    SessionState session;
};

// Test verify_upload function
TEST_F(VerificationTest, VerifyUpload_NullSession) {
    UploadResult result = verify_upload(nullptr);
    EXPECT_EQ(result, UPLOADSTB_FAILED);
}

TEST_F(VerificationTest, VerifyUpload_Success) {
    session.http_code = 200;
    session.curl_code = 0; // CURLE_OK
    
    UploadResult result = verify_upload(&session);
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
}

TEST_F(VerificationTest, VerifyUpload_HttpFailure) {
    session.http_code = 404;
    session.curl_code = 0; // CURLE_OK
    
    UploadResult result = verify_upload(&session);
    EXPECT_EQ(result, UPLOADSTB_FAILED);
}

TEST_F(VerificationTest, VerifyUpload_CurlFailure) {
    session.http_code = 200;
    session.curl_code = 7; // CURLE_COULDNT_CONNECT
    
    UploadResult result = verify_upload(&session);
    EXPECT_EQ(result, UPLOADSTB_FAILED);
}

TEST_F(VerificationTest, VerifyUpload_BothFailure) {
    session.http_code = 500;
    session.curl_code = 28; // CURLE_OPERATION_TIMEDOUT
    
    UploadResult result = verify_upload(&session);
    EXPECT_EQ(result, UPLOADSTB_FAILED);
}

TEST_F(VerificationTest, VerifyUpload_Http000) {
    session.http_code = 0; // Network failure
    session.curl_code = 0; // CURLE_OK
    
    UploadResult result = verify_upload(&session);
    EXPECT_EQ(result, UPLOADSTB_FAILED);
}

// Test is_http_success function
TEST_F(VerificationTest, IsHttpSuccess_Success) {
    EXPECT_TRUE(is_http_success(200));
}

TEST_F(VerificationTest, IsHttpSuccess_Failure) {
    EXPECT_FALSE(is_http_success(404));
    EXPECT_FALSE(is_http_success(500));
    EXPECT_FALSE(is_http_success(403));
    EXPECT_FALSE(is_http_success(0));
    EXPECT_FALSE(is_http_success(201)); // Not exactly 200
}

// Test is_terminal_failure function
TEST_F(VerificationTest, IsTerminalFailure_Terminal) {
    EXPECT_TRUE(is_terminal_failure(404));
}

TEST_F(VerificationTest, IsTerminalFailure_Retryable) {
    EXPECT_FALSE(is_terminal_failure(500));
    EXPECT_FALSE(is_terminal_failure(503));
    EXPECT_FALSE(is_terminal_failure(0));
    EXPECT_FALSE(is_terminal_failure(200));
    EXPECT_FALSE(is_terminal_failure(403));
}

// Test is_curl_success function  
TEST_F(VerificationTest, IsCurlSuccess_Success) {
    EXPECT_TRUE(is_curl_success(0)); // CURLE_OK
}

TEST_F(VerificationTest, IsCurlSuccess_Failure) {
    EXPECT_FALSE(is_curl_success(7));  // CURLE_COULDNT_CONNECT
    EXPECT_FALSE(is_curl_success(28)); // CURLE_OPERATION_TIMEDOUT
    EXPECT_FALSE(is_curl_success(35)); // CURLE_SSL_CONNECT_ERROR
    EXPECT_FALSE(is_curl_success(60)); // CURLE_SSL_CACERT
}

// Test get_curl_error_desc function
TEST_F(VerificationTest, GetCurlErrorDesc_KnownErrors) {
    const char* desc;
    
    desc = get_curl_error_desc(0);
    EXPECT_STREQ(desc, "No error");
    
    desc = get_curl_error_desc(7);
    EXPECT_STREQ(desc, "Couldn't connect to server");
    
    desc = get_curl_error_desc(28);
    EXPECT_STREQ(desc, "Timeout was reached");
    
    desc = get_curl_error_desc(35);
    EXPECT_STREQ(desc, "SSL connect error");
    
    desc = get_curl_error_desc(60);
    EXPECT_STREQ(desc, "SSL peer certificate or SSH remote key was not OK");
}

TEST_F(VerificationTest, GetCurlErrorDesc_UnknownError) {
    const char* desc = get_curl_error_desc(999);
    EXPECT_STREQ(desc, "Unknown error");
}

// Test various HTTP status code scenarios
TEST_F(VerificationTest, HttpStatusCodes_RedirectionCodes) {
    // Test various 3xx codes
    EXPECT_FALSE(is_http_success(301)); // Moved Permanently
    EXPECT_FALSE(is_http_success(302)); // Found
    EXPECT_FALSE(is_http_success(304)); // Not Modified
    EXPECT_FALSE(is_terminal_failure(301));
    EXPECT_FALSE(is_terminal_failure(302));
}

TEST_F(VerificationTest, HttpStatusCodes_ClientErrorCodes) {
    // Test various 4xx codes
    EXPECT_FALSE(is_http_success(400)); // Bad Request
    EXPECT_FALSE(is_http_success(401)); // Unauthorized
    EXPECT_FALSE(is_http_success(403)); // Forbidden
    EXPECT_TRUE(is_terminal_failure(404)); // Not Found - terminal
    EXPECT_FALSE(is_terminal_failure(400));
    EXPECT_FALSE(is_terminal_failure(401));
    EXPECT_FALSE(is_terminal_failure(403));
}

TEST_F(VerificationTest, HttpStatusCodes_ServerErrorCodes) {
    // Test various 5xx codes
    EXPECT_FALSE(is_http_success(500)); // Internal Server Error
    EXPECT_FALSE(is_http_success(502)); // Bad Gateway
    EXPECT_FALSE(is_http_success(503)); // Service Unavailable
    EXPECT_FALSE(is_http_success(504)); // Gateway Timeout
    
    // 5xx codes are retryable, not terminal
    EXPECT_FALSE(is_terminal_failure(500));
    EXPECT_FALSE(is_terminal_failure(502));
    EXPECT_FALSE(is_terminal_failure(503));
    EXPECT_FALSE(is_terminal_failure(504));
}

// Test common curl error codes
TEST_F(VerificationTest, CurlErrorCodes_NetworkErrors) {
    session.http_code = 200;
    
    // Test various curl network errors
    session.curl_code = 7; // CURLE_COULDNT_CONNECT
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    
    session.curl_code = 6; // CURLE_COULDNT_RESOLVE_HOST
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    
    session.curl_code = 28; // CURLE_OPERATION_TIMEDOUT
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
}

TEST_F(VerificationTest, CurlErrorCodes_SslErrors) {
    session.http_code = 200;
    
    // Test various SSL-related curl errors
    session.curl_code = 35; // CURLE_SSL_CONNECT_ERROR
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    
    session.curl_code = 60; // CURLE_SSL_CACERT
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    
    session.curl_code = 51; // CURLE_PEER_FAILED_VERIFICATION
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
}

// Test edge cases and boundary conditions
TEST_F(VerificationTest, EdgeCases_BoundaryHttpCodes) {
    // Test boundary HTTP codes
    EXPECT_FALSE(is_http_success(199));
    EXPECT_TRUE(is_http_success(200));
    EXPECT_FALSE(is_http_success(201));
    
    // Test negative HTTP codes
    EXPECT_FALSE(is_http_success(-1));
    EXPECT_FALSE(is_terminal_failure(-1));
}

TEST_F(VerificationTest, EdgeCases_BoundaryCurlCodes) {
    // Test boundary curl codes
    EXPECT_TRUE(is_curl_success(0));   // CURLE_OK
    EXPECT_FALSE(is_curl_success(1));  // Not OK
    EXPECT_FALSE(is_curl_success(-1)); // Invalid
}

// Integration test scenarios
TEST_F(VerificationTest, Integration_UploadScenarios) {
    // Scenario 1: Perfect success
    session.http_code = 200;
    session.curl_code = 0;
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_SUCCESS);
    
    // Scenario 2: Network timeout
    session.http_code = 0;
    session.curl_code = 28; // CURLE_OPERATION_TIMEDOUT
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    
    // Scenario 3: Authentication failure
    session.http_code = 401;
    session.curl_code = 0;
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    
    // Scenario 4: Server error (retryable)
    session.http_code = 503;
    session.curl_code = 0;
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    EXPECT_FALSE(is_terminal_failure(503)); // Should be retryable
    
    // Scenario 5: Not found (terminal)
    session.http_code = 404;
    session.curl_code = 0;
    EXPECT_EQ(verify_upload(&session), UPLOADSTB_FAILED);
    EXPECT_TRUE(is_terminal_failure(404)); // Should be terminal
}

TEST_F(VerificationTest, Integration_RealWorldHttpCodes) {
    // Test real-world HTTP response codes
    int success_codes[] = {200};
    int failure_codes[] = {400, 401, 403, 404, 500, 502, 503, 504};
    int terminal_codes[] = {404};
    
    // Test success codes
    for (int code : success_codes) {
        EXPECT_TRUE(is_http_success(code));
    }
    
    // Test failure codes
    for (int code : failure_codes) {
        EXPECT_FALSE(is_http_success(code));
    }
    
    // Test terminal codes
    for (int code : terminal_codes) {
        EXPECT_TRUE(is_terminal_failure(code));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}