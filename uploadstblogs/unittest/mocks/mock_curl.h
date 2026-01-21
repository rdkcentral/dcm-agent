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

#ifndef MOCK_CURL_H
#define MOCK_CURL_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <curl/curl.h>

// Undefine CURL macros that conflict with our mock methods
#ifdef curl_easy_setopt
#undef curl_easy_setopt
#endif
#ifdef curl_easy_getinfo
#undef curl_easy_getinfo
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Mock class for CURL functions
// Note: curl_easy_setopt and curl_easy_getinfo have variadic arguments
// and cannot be mocked with GMock. They are provided as regular functions.
class MockCurl {
public:
    MOCK_METHOD0(curl_easy_init, CURL*());
    MOCK_METHOD1(curl_easy_perform, CURLcode(CURL* curl));
    MOCK_METHOD1(curl_easy_cleanup, void(CURL* curl));
    MOCK_METHOD1(curl_easy_strerror, const char*(CURLcode code));
    MOCK_METHOD1(curl_global_init, CURLcode(long flags));
    MOCK_METHOD0(curl_global_cleanup, void());
};

// Global mock instance
extern MockCurl* g_mockCurl;

#ifdef __cplusplus
}
#endif

#endif /* MOCK_CURL_H */
