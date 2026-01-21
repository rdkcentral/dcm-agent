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

#ifndef MOCK_FILE_OPERATIONS_H
#define MOCK_FILE_OPERATIONS_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// File operations function declarations
bool file_exists(const char* filepath);
bool dir_exists(const char* dirpath);
bool create_directory(const char* dirpath);
bool copy_file(const char* src, const char* dest);
void emit_system_validation_event(const char* component, bool success);
void emit_folder_missing_error(void);
int v_secure_system(const char* command, ...);
bool is_directory_empty(const char* dirpath);

#ifdef __cplusplus
}
#endif

// Mock class for file operations
class MockFileOperations {
public:
    MOCK_METHOD1(file_exists, bool(const char* filepath));
    MOCK_METHOD1(dir_exists, bool(const char* dirpath));
    MOCK_METHOD1(create_directory, bool(const char* dirpath));
    MOCK_METHOD2(copy_file, bool(const char* src, const char* dest));
    MOCK_METHOD2(emit_system_validation_event, void(const char* component, bool success));
    MOCK_METHOD0(emit_folder_missing_error, void(void));
    MOCK_METHOD1(v_secure_system, int(const char* command));
    MOCK_METHOD1(is_directory_empty, bool(const char* dirpath));
};

// Global mock instance
extern MockFileOperations* g_mockFileOperations;

#endif /* MOCK_FILE_OPERATIONS_H */
