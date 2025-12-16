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

#include "mock_file_operations.h"
#include <unistd.h>
#include <sys/stat.h>
#include <cstdarg>

// Global mock instance
MockFileOperations* g_mockFileOperations = nullptr;

extern "C" {

// Mock implementations that delegate to the global mock object or provide defaults
bool file_exists(const char* filepath) {
    if (g_mockFileOperations) {
        return g_mockFileOperations->file_exists(filepath);
    }
    // Default implementation using access()
    if (!filepath) return false;
    return (access(filepath, F_OK) == 0);
}

bool dir_exists(const char* dirpath) {
    if (g_mockFileOperations) {
        return g_mockFileOperations->dir_exists(dirpath);
    }
    // Default implementation using stat()
    if (!dirpath) return false;
    struct stat st;
    return (stat(dirpath, &st) == 0 && S_ISDIR(st.st_mode));
}

bool create_directory(const char* dirpath) {
    if (g_mockFileOperations) {
        return g_mockFileOperations->create_directory(dirpath);
    }
    // Default implementation - assume success
    (void)dirpath;
    return true;
}

void emit_system_validation_event(const char* component, bool success) {
    if (g_mockFileOperations) {
        g_mockFileOperations->emit_system_validation_event(component, success);
        return;
    }
    // Default implementation - do nothing
    (void)component;
    (void)success;
}

void emit_folder_missing_error(void) {
    if (g_mockFileOperations) {
        g_mockFileOperations->emit_folder_missing_error();
        return;
    }
    // Default implementation - do nothing
}

int v_secure_system(const char* command, ...) {
    if (g_mockFileOperations) {
        return g_mockFileOperations->v_secure_system(command);
    }
    // Default implementation - return success
    (void)command;
    return 0;
}

bool is_directory_empty(const char* dirpath) {
    if (g_mockFileOperations) {
        return g_mockFileOperations->is_directory_empty(dirpath);
    }
    // Default implementation - assume directory is not empty
    (void)dirpath;
    return false;
}

}
