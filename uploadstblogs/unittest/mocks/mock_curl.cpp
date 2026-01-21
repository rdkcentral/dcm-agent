/*
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
 */

#include "mock_curl.h"

// Global mock instance
MockCurl* g_mockCurl = nullptr;

extern "C" {

// Note: CURL mocking is complex due to variadic functions
// For now, provide minimal implementations for basic testing
CURL* curl_easy_init() {
    if (g_mockCurl) {
        return g_mockCurl->curl_easy_init();
    }
    return nullptr;
}

void curl_easy_cleanup(CURL* curl) {
    if (g_mockCurl) {
        g_mockCurl->curl_easy_cleanup(curl);
    }
}

CURLcode curl_easy_perform(CURL* curl) {
    if (g_mockCurl) {
        return g_mockCurl->curl_easy_perform(curl);
    }
    return CURLE_FAILED_INIT;
}

const char* curl_easy_strerror(CURLcode code) {
    if (g_mockCurl) {
        return g_mockCurl->curl_easy_strerror(code);
    }
    return "Mock error";
}

CURLcode curl_global_init(long flags) {
    if (g_mockCurl) {
        return g_mockCurl->curl_global_init(flags);
    }
    return CURLE_OK;
}

void curl_global_cleanup() {
    if (g_mockCurl) {
        g_mockCurl->curl_global_cleanup();
    }
}

// Variadic functions - cannot be mocked with GMock
// Provide simple implementations that return success
CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) {
    (void)curl;
    (void)option;
    // In a real mock, you'd process the variadic arguments
    // For testing purposes, just return success
    return CURLE_OK;
}

CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) {
    (void)curl;
    (void)info;
    // In a real mock, you'd process the variadic arguments
    // For testing purposes, just return success
    return CURLE_OK;
}

}
