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
#include <iostream>

// Mock RDK_LOG before including other headers
#ifdef GTEST_ENABLE
#define RDK_LOG(level, module, ...) do {} while(0)
#endif

#include "uploadstblogs_types.h"

// Include system headers for types before extern "C"
#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>

// HTTP upload type constants (from uploadutil/codebig_upload.h)
#define HTTP_SSR_DIRECT      0
#define HTTP_SSR_CODEBIG     1
#define HTTP_XCONF_DIRECT    2
#define HTTP_XCONF_CODEBIG   3
#define HTTP_UNKNOWN         5

// Forward declare external types for mocking
typedef struct {
    int result_code;
    long http_code;
    int curl_code;
    bool upload_completed;
    bool auth_success;
    char error_message[256];
    char fqdn[256];
} UploadStatusDetail;

// Include system headers for types before extern "C"
#include <stdio.h>

// Mock external dependencies
extern "C" {
// Mock system functions
FILE* fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fscanf(FILE *stream, const char *format, ...);


// Mock external module functions
bool calculate_file_md5(const char* filepath, char* md5_hash, size_t hash_size);
void report_mtls_usage(void);
void report_curl_error(int curl_code);
void report_cert_error(int curl_code, const char* fqdn);
UploadResult verify_upload(const SessionState* session);

// Mock telemetry 2.0 functions
void t2_count_notify(const char* marker);
void t2_val_notify(const char* marker, const char* value);

// Mock MtlsAuth_t type
typedef struct {
    char cert_name[256];
    char key_pas[256];
    char cert_type[16];
    char engine[64];
} MtlsAuth_t;

// Mock upload library functions
void __uploadutil_set_ocsp(bool enabled);
void __uploadutil_get_status(long *http_code, int *curl_code);
int performMetadataPostWithCertRotationEx(const char *upload_url, const char *filepath,
                                          const char *extra_fields, MtlsAuth_t *sec_out,
                                          long *http_code_out);
int performS3PutWithCert(const char *s3_url, const char *src_file, MtlsAuth_t *sec);
int performCodeBigMetadataPost(void *curl, const char *filepath,
                               const char *extra_fields, int server_type,
                               long *http_code_out);
int performCodeBigS3Put(const char *s3_url, const char *src_file);
int performS3PutUploadEx(const char* upload_url, const char* src_file,
                         MtlsAuth_t* auth, const char* md5_hash,
                         bool ocsp_enabled, UploadStatusDetail* status);
int extractS3PresignedUrl(const char* httpresult_file, char* s3_url, size_t s3_url_size);
}

#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 0
#endif

// Mock state
static bool mock_calculate_md5_result = true;
static char mock_md5_hash[64] = "abcd1234efgh5678";
static bool mock_file_exists = true;
static char mock_file_content[1024] = "https://s3.bucket.com/path/file.tar.gz?query=123";
static UploadStatusDetail mock_upload_status;
static UploadResult mock_verify_result = UPLOADSTB_SUCCESS;
static int mock_upload_function_result = 0;

// Mock call tracking variables
static int mock_calculate_md5_calls = 0;
static int mock_report_mtls_calls = 0;
static int mock_report_curl_error_calls = 0;
static int mock_report_cert_error_calls = 0;
static int mock_verify_upload_calls = 0;
static int mock_upload_mtls_calls = 0;
static int mock_upload_codebig_calls = 0;
static int mock_upload_s3_calls = 0;
static int mock_fopen_calls = 0;
static int mock_fgets_calls = 0;

// Mock implementations
bool calculate_file_md5(const char* filepath, char* md5_hash, size_t hash_size) {
    mock_calculate_md5_calls++;
    if (mock_calculate_md5_result && md5_hash && hash_size > 0) {
        strncpy(md5_hash, mock_md5_hash, hash_size - 1);
        md5_hash[hash_size - 1] = '\0';
        return true;
    }
    return false;
}

void report_mtls_usage(void) {
    mock_report_mtls_calls++;
}

void report_curl_error(int curl_code) {
    mock_report_curl_error_calls++;
}

void report_cert_error(int curl_code, const char* fqdn) {
    mock_report_cert_error_calls++;
}

static int mock_verify_call_count = 0;
static UploadResult mock_verify_results[10] = {UPLOADSTB_SUCCESS}; // Array for multiple calls

UploadResult verify_upload(const SessionState* session) {
    mock_verify_upload_calls++;
    
    // If we have specific results set for this call index, use it
    if (mock_verify_call_count < 10 && mock_verify_call_count < mock_verify_upload_calls) {
        UploadResult result = mock_verify_results[mock_verify_call_count];
        mock_verify_call_count++;
        return result;
    }
    
    // Otherwise use the default result
    return mock_verify_result;
}

void t2_count_notify(const char* marker) {
    // Mock - do nothing
}

void t2_val_notify(const char* marker, const char* value) {
    // Mock - do nothing
}

void __uploadutil_set_ocsp(bool enabled) {
    // Mock - do nothing
}

static long mock_http_code_status = 200;
static int mock_curl_code_status = 0;

void __uploadutil_get_status(long *http_code, int *curl_code) {
    if (http_code) *http_code = mock_http_code_status;
    if (curl_code) *curl_code = mock_curl_code_status;
}

int performMetadataPostWithCertRotationEx(const char *upload_url, const char *filepath,
                                          const char *extra_fields, MtlsAuth_t *sec_out,
                                          long *http_code_out) {
    mock_upload_mtls_calls++;
    if (http_code_out) {
        *http_code_out = mock_upload_status.http_code;
    }
    if (sec_out) {
        strcpy(sec_out->cert_name, "mock_cert.p12");
        strcpy(sec_out->key_pas, "mock_pass");
        strcpy(sec_out->cert_type, "P12");
    }
    return mock_upload_function_result;
}

int performS3PutWithCert(const char *s3_url, const char *src_file, MtlsAuth_t *sec) {
    mock_upload_s3_calls++;
    return mock_upload_function_result;
}

static int mock_codebig_metadata_result = 0;
static int mock_codebig_s3_result = 0;

int performCodeBigMetadataPost(void *curl, const char *filepath,
                               const char *extra_fields, int server_type,
                               long *http_code_out) {
    mock_upload_codebig_calls++;
    if (http_code_out) {
        *http_code_out = mock_upload_status.http_code;
    }
    // Use specific result if set, otherwise fall back to mock_upload_function_result
    return (mock_codebig_metadata_result != 0) ? mock_codebig_metadata_result : mock_upload_function_result;
}

int performCodeBigS3Put(const char *s3_url, const char *src_file) {
    mock_upload_s3_calls++;
    // Use specific result if set, otherwise fall back to mock_upload_function_result
    return (mock_codebig_s3_result != 0) ? mock_codebig_s3_result : mock_upload_function_result;
}

int performS3PutUploadEx(const char* upload_url, const char* src_file,
                         MtlsAuth_t* auth, const char* md5_hash,
                         bool ocsp_enabled, UploadStatusDetail* status) {
    mock_upload_s3_calls++;
    if (status) {
        *status = mock_upload_status;
    }
    return mock_upload_function_result;
}

int extractS3PresignedUrl(const char* httpresult_file, char* s3_url, size_t s3_url_size) {
    // Check if file exists (simulating file read failure)
    if (!mock_file_exists) {
        return -1;
    }
    
    if (s3_url && s3_url_size > 0 && strlen(mock_file_content) > 0) {
        // Check for valid URL format (must start with https://)
        if (strstr(mock_file_content, "https://") != mock_file_content) {
            return -1; // Invalid URL format
        }
        strncpy(s3_url, mock_file_content, s3_url_size - 1);
        s3_url[s3_url_size - 1] = '\0';
        return 0;
    }
    return -1;
}

FILE* fopen(const char *pathname, const char *mode) {
    mock_fopen_calls++;
    if (mock_file_exists) {
        return (FILE*)0x12345678; // Mock pointer
    }
    return nullptr;
}

int fclose(FILE *stream) {
    return 0;
}

char *fgets(char *s, int size, FILE *stream) {
    mock_fgets_calls++;
    if (s && size > 0 && strlen(mock_file_content) > 0) {
        strncpy(s, mock_file_content, size - 1);
        s[size - 1] = '\0';
        return s;
    }
    return nullptr;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    if (str && size > 0) {
        strcpy(str, "mock_formatted_string");
        return 19; // Length of mock string
    }
    return -1;
}

int fscanf(FILE *stream, const char *format, ...) {
    // Mock fscanf - just return 200 as HTTP code for success scenarios
    va_list args;
    va_start(args, format);
    long* http_code_ptr = va_arg(args, long*);
    if (http_code_ptr) {
        *http_code_ptr = mock_http_code_status;
    }
    va_end(args);
    return 1; // Return 1 item read
}

// Include the actual path handler implementation
#include "path_handler.h"
#include "../src/path_handler.c"

using namespace testing;
using namespace std;

class PathHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        mock_calculate_md5_result = true;
        strcpy(mock_md5_hash, "abcd1234efgh5678");
        mock_file_exists = true;
        strcpy(mock_file_content, "https://s3.bucket.com/path/file.tar.gz?query=123");
        mock_verify_result = UPLOADSTB_SUCCESS;
        mock_upload_function_result = 0;
        
        // Initialize mock upload status
        mock_upload_status.curl_code = 0;
        mock_upload_status.http_code = 200;
        strcpy(mock_upload_status.error_message, "");
        strcpy(mock_upload_status.fqdn, "s3.amazonaws.com");
        
        // Reset call tracking
        mock_calculate_md5_calls = 0;
        mock_report_mtls_calls = 0;
        mock_report_curl_error_calls = 0;
        mock_report_cert_error_calls = 0;
        mock_verify_upload_calls = 0;
        mock_verify_call_count = 0;
        mock_upload_mtls_calls = 0;
        mock_upload_codebig_calls = 0;
        mock_upload_s3_calls = 0;
        mock_fopen_calls = 0;
        mock_fgets_calls = 0;
        
        // Reset CodeBig specific results
        mock_codebig_metadata_result = 0;
        mock_codebig_s3_result = 0;
        
        // Reset verify results array
        for (int i = 0; i < 10; i++) {
            mock_verify_results[i] = UPLOADSTB_SUCCESS;
        }
        
        // Set up default test context
        strcpy(test_ctx.endpoints.endpoint_url, "https://upload.example.com");
        strcpy(test_ctx.endpoints.proxy_bucket, "proxy.bucket.com");
        strcpy(test_ctx.device.device_type, "gateway");
        test_ctx.settings.encryption_enable = false;
        test_ctx.settings.ocsp_enabled = false;
        
        // Set up default test session
        strcpy(test_session.archive_file, "/tmp/logs.tar.gz");
        test_session.strategy = STRAT_DCM;
        test_session.curl_code = 0;
        test_session.http_code = 0;
        test_session.success = false;
    }
    
    void TearDown() override {}
    
    RuntimeContext test_ctx;
    SessionState test_session;
};

// Test execute_direct_path function
TEST_F(PathHandlerTest, ExecuteDirectPath_Success) {
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_TRUE(test_session.success);
    EXPECT_EQ(mock_report_mtls_calls, 1);
    EXPECT_EQ(mock_upload_mtls_calls, 1); // Metadata POST
    EXPECT_EQ(mock_upload_s3_calls, 1);   // S3 PUT
    EXPECT_EQ(mock_verify_upload_calls, 2); // Once for POST, once for S3 PUT
    EXPECT_EQ(test_session.curl_code, 0);
    EXPECT_EQ(test_session.http_code, 200);
}

TEST_F(PathHandlerTest, ExecuteDirectPath_NullContext) {
    UploadResult result = execute_direct_path(nullptr, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_upload_mtls_calls, 0);
}

TEST_F(PathHandlerTest, ExecuteDirectPath_NullSession) {
    UploadResult result = execute_direct_path(&test_ctx, nullptr);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_upload_mtls_calls, 0);
}

TEST_F(PathHandlerTest, ExecuteDirectPath_WithEncryption) {
    test_ctx.settings.encryption_enable = true;
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(mock_calculate_md5_calls, 1);
    EXPECT_EQ(mock_upload_mtls_calls, 1); // Metadata POST
    EXPECT_EQ(mock_upload_s3_calls, 1);   // S3 PUT
}

TEST_F(PathHandlerTest, ExecuteDirectPath_EncryptionMD5Failure) {
    test_ctx.settings.encryption_enable = true;
    mock_calculate_md5_result = false;
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    // Should still proceed with upload even if MD5 calculation fails
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(mock_calculate_md5_calls, 1);
    EXPECT_EQ(mock_upload_mtls_calls, 1); // Metadata POST
    EXPECT_EQ(mock_upload_s3_calls, 1);   // S3 PUT
}

TEST_F(PathHandlerTest, ExecuteDirectPath_CurlError) {
    mock_upload_status.curl_code = 7; // CURLE_COULDNT_CONNECT
    mock_curl_code_status = 7; // Set for __uploadutil_get_status
    mock_verify_results[0] = UPLOADSTB_FAILED; // Verify should fail with curl error
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(mock_report_curl_error_calls, 1);
    EXPECT_EQ(test_session.curl_code, 7);
}

TEST_F(PathHandlerTest, ExecuteDirectPath_CertificateError) {
    mock_upload_status.curl_code = 60; // CURLE_SSL_CACERT
    mock_curl_code_status = 60; // Set for __uploadutil_get_status
    mock_verify_results[0] = UPLOADSTB_FAILED; // Verify should fail with certificate error
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(mock_report_curl_error_calls, 1);
    EXPECT_EQ(mock_report_cert_error_calls, 1);
}

TEST_F(PathHandlerTest, ExecuteDirectPath_UploadFailure) {
    mock_verify_results[0] = UPLOADSTB_FAILED; // Metadata POST verification fails
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_FALSE(test_session.success);
}

TEST_F(PathHandlerTest, ExecuteDirectPath_ProxyFallback_MediaClient) {
    strcpy(test_ctx.device.device_type, "mediaclient");
    strcpy(mock_file_content, "https://original.bucket.com/path/file.tar.gz?query=123\n");
    
    // Set up verify results: first call (metadata POST) succeeds, second call (S3 PUT) fails, third call (proxy) fails
    mock_verify_results[0] = UPLOADSTB_SUCCESS;  // Metadata POST succeeds
    mock_verify_results[1] = UPLOADSTB_FAILED;   // S3 PUT fails -> triggers proxy fallback
    mock_verify_results[2] = UPLOADSTB_FAILED;   // Proxy also fails
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    // Should attempt: metadata POST (succeeds), S3 PUT (fails), then proxy fallback
    EXPECT_EQ(mock_upload_mtls_calls, 1); // Metadata POST
    EXPECT_GE(mock_upload_s3_calls, 1);   // S3 PUT + proxy fallback attempt
    EXPECT_GE(mock_fopen_calls, 1);       // Read httpresult.txt for S3 URL and proxy
}

TEST_F(PathHandlerTest, ExecuteDirectPath_ProxyFallback_NoProxyBucket) {
    strcpy(test_ctx.device.device_type, "mediaclient");
    strcpy(test_ctx.endpoints.proxy_bucket, ""); // No proxy bucket
    
    // Metadata POST succeeds, S3 PUT fails, but no proxy available
    mock_verify_results[0] = UPLOADSTB_SUCCESS;  // Metadata POST succeeds
    mock_verify_results[1] = UPLOADSTB_FAILED;   // S3 PUT fails
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_upload_s3_calls, 1); // S3 PUT attempted, but no proxy fallback due to missing proxy_bucket
}

// Test execute_codebig_path function
TEST_F(PathHandlerTest, ExecuteCodeBigPath_Success) {
    UploadResult result = execute_codebig_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(mock_upload_codebig_calls, 1); // Metadata POST
    EXPECT_EQ(mock_upload_s3_calls, 1);      // S3 PUT
    EXPECT_EQ(test_session.curl_code, 0);
    EXPECT_EQ(test_session.http_code, 200);
    // Note: CodeBig path doesn't call verify_upload or set success flag
    // It returns UPLOADSTB_SUCCESS directly on successful upload
}

TEST_F(PathHandlerTest, ExecuteCodeBigPath_NullContext) {
    UploadResult result = execute_codebig_path(nullptr, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_upload_codebig_calls, 0);
}

TEST_F(PathHandlerTest, ExecuteCodeBigPath_NullSession) {
    UploadResult result = execute_codebig_path(&test_ctx, nullptr);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_upload_codebig_calls, 0);
}

TEST_F(PathHandlerTest, ExecuteCodeBigPath_WithEncryption) {
    test_ctx.settings.encryption_enable = true;
    
    UploadResult result = execute_codebig_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(mock_calculate_md5_calls, 1);
    EXPECT_EQ(mock_upload_codebig_calls, 1);
}

TEST_F(PathHandlerTest, ExecuteCodeBigPath_CurlError) {
    // Make metadata POST succeed but S3 PUT fail
    mock_codebig_metadata_result = 0;  // Metadata POST succeeds
    mock_codebig_s3_result = 28;       // S3 PUT fails with CURLE_OPERATION_TIMEDOUT
    
    UploadResult result = execute_codebig_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_report_curl_error_calls, 1); // report_curl_error called for S3 PUT failure
    EXPECT_EQ(test_session.curl_code, 28);
}

TEST_F(PathHandlerTest, ExecuteCodeBigPath_UploadFailure) {
    // Make metadata POST fail to cause upload failure
    mock_codebig_metadata_result = 1; // Non-zero = failure
    
    UploadResult result = execute_codebig_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(test_session.curl_code, 1); // curl_code set to the error code
    // Note: CodeBig path doesn't set session->success flag
}

// Test proxy fallback functionality
TEST_F(PathHandlerTest, ProxyFallback_FileNotFound) {
    strcpy(test_ctx.device.device_type, "mediaclient");
    mock_file_exists = false; // httpresult.txt doesn't exist
    
    // Metadata POST succeeds, but S3 PUT will fail due to missing file
    mock_verify_results[0] = UPLOADSTB_SUCCESS;  // Metadata POST succeeds
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    // extractS3PresignedUrl fails early, so fopen is not called
    EXPECT_EQ(mock_upload_s3_calls, 0); // No S3 upload due to file error
}

TEST_F(PathHandlerTest, ProxyFallback_InvalidURL) {
    strcpy(test_ctx.device.device_type, "mediaclient");
    strcpy(mock_file_content, "invalid-url-format\n");
    
    // Metadata POST succeeds, but S3 PUT will fail due to invalid URL
    mock_verify_results[0] = UPLOADSTB_SUCCESS;  // Metadata POST succeeds
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_FAILED);
    EXPECT_EQ(mock_upload_s3_calls, 0); // No S3 upload due to URL parsing error
}

TEST_F(PathHandlerTest, ProxyFallback_Success) {
    strcpy(test_ctx.device.device_type, "mediaclient");
    strcpy(mock_file_content, "https://original.bucket.com/path/file.tar.gz?query=123\n");
    
    // Set up verify results: metadata POST succeeds, S3 PUT fails, proxy succeeds
    mock_verify_results[0] = UPLOADSTB_SUCCESS;  // Metadata POST succeeds
    mock_verify_results[1] = UPLOADSTB_FAILED;   // S3 PUT fails -> triggers proxy fallback
    mock_verify_results[2] = UPLOADSTB_SUCCESS;  // Proxy succeeds
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    // Should attempt: metadata POST (succeeds), S3 PUT (fails), then proxy (succeeds)
    EXPECT_EQ(mock_upload_mtls_calls, 1); // Metadata POST
    EXPECT_GE(mock_upload_s3_calls, 1);   // At least S3 PUT attempt (may include proxy)
    EXPECT_EQ(result, UPLOADSTB_SUCCESS); // Proxy fallback succeeded
    EXPECT_TRUE(test_session.success);
}

// Test OCSP functionality
TEST_F(PathHandlerTest, ExecuteDirectPath_WithOCSP) {
    test_ctx.settings.ocsp_enabled = true;
    
    UploadResult result = execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(mock_upload_mtls_calls, 1); // Metadata POST
    EXPECT_EQ(mock_upload_s3_calls, 1);   // S3 PUT
}

TEST_F(PathHandlerTest, ExecuteCodeBigPath_WithOCSP) {
    test_ctx.settings.ocsp_enabled = true;
    
    UploadResult result = execute_codebig_path(&test_ctx, &test_session);
    
    EXPECT_EQ(result, UPLOADSTB_SUCCESS);
    EXPECT_EQ(mock_upload_codebig_calls, 1);
}

// Test certificate error codes
TEST_F(PathHandlerTest, CertificateErrorCodes_AllDetected) {
    // Test various certificate error codes
    int cert_error_codes[] = {35, 51, 53, 54, 58, 59, 60, 64, 66, 77, 80, 82, 83, 90, 91};
    size_t num_codes = sizeof(cert_error_codes) / sizeof(cert_error_codes[0]);
    
    for (size_t i = 0; i < num_codes; i++) {
        SetUp(); // Reset state
        mock_upload_status.curl_code = cert_error_codes[i];
        mock_curl_code_status = cert_error_codes[i]; // Set for __uploadutil_get_status
        mock_verify_results[0] = UPLOADSTB_FAILED; // Verify should fail with certificate error
        
        execute_direct_path(&test_ctx, &test_session);
        
        EXPECT_EQ(mock_report_cert_error_calls, 1) 
            << "Failed for certificate error code: " << cert_error_codes[i];
    }
}

TEST_F(PathHandlerTest, NonCertificateErrorCode_NotReported) {
    mock_upload_status.curl_code = 7; // CURLE_COULDNT_CONNECT (not a cert error)
    mock_curl_code_status = 7; // Set for __uploadutil_get_status
    mock_verify_results[0] = UPLOADSTB_FAILED; // Verify should fail with curl error
    
    execute_direct_path(&test_ctx, &test_session);
    
    EXPECT_EQ(mock_report_cert_error_calls, 0);
    EXPECT_EQ(mock_report_curl_error_calls, 1); // Should still report general curl error
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    cout << "Starting Path Handler Unit Tests" << endl;
    return RUN_ALL_TESTS();
}
