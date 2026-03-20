/*
 * Mock definitions for config_manager unit tests
 * Copyright 2024 Comcast Cable Communications Management, LLC
 */

#ifndef CONFIG_MANAGER_TEST_MOCKS_H
#define CONFIG_MANAGER_TEST_MOCKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Only define things not already defined in real headers
#ifndef UTILS_SUCCESS
#define UTILS_SUCCESS 0
#endif

// Forward declarations only - actual definitions come from real headers
struct backup_config_t;

// Mock function declarations - these will be wrapped
void RDK_LOG(int level, const char* module, const char* format, ...);
int getIncludePropertyData(const char* property, char* value, int size);
int getDevicePropertyData(const char* property, char* value, int size);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_TEST_MOCKS_H
