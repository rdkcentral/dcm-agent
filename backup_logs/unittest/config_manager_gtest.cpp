/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <climits>

extern "C" {
    #include "config_manager.h"
    #include "backup_types.h"
}

// ================================================================================================
// Mock Function Control Variables
// ================================================================================================

static struct {
    // RDK_LOG mock control
    volatile bool rdk_log_enabled = false;

    // getIncludePropertyData mock controls
    volatile int getIncludePropertyData_return = -1;
    volatile bool getIncludePropertyData_called = false;
    char getIncludePropertyData_last_property[64] = {0};
    char getIncludePropertyData_value[PATH_MAX] = {0};

    // getDevicePropertyData mock controls
    volatile int getDevicePropertyData_return = -1;
    volatile bool getDevicePropertyData_called = false;
    char getDevicePropertyData_last_property[64] = {0};

    // Per-property return values for getDevicePropertyData
    // (allows different return values for APP_PERSISTENT_PATH vs HDD_ENABLED)
    volatile int getDevicePropertyData_APP_PERSISTENT_PATH_return = -1;
    char getDevicePropertyData_APP_PERSISTENT_PATH_value[PATH_MAX] = {0};

    volatile int getDevicePropertyData_HDD_ENABLED_return = -1;
    char getDevicePropertyData_HDD_ENABLED_value[32] = {0};

} mock_control;

// ================================================================================================
// Mock Function Implementations
// ================================================================================================

extern "C" {
    void __wrap_RDK_LOG(int level, const char* module, const char* format, ...) {
        (void)level; (void)module; (void)format;
        mock_control.rdk_log_enabled = true;
    }

    int __wrap_getIncludePropertyData(const char* property, char* value, int size) {
        mock_control.getIncludePropertyData_called = true;
        if (property) {
            strncpy(mock_control.getIncludePropertyData_last_property, property,
                    sizeof(mock_control.getIncludePropertyData_last_property) - 1);
            mock_control.getIncludePropertyData_last_property[
                sizeof(mock_control.getIncludePropertyData_last_property) - 1] = '\0';
        }
        if (value && size > 0) {
            snprintf(value, size, "%s", mock_control.getIncludePropertyData_value);
        }
        return mock_control.getIncludePropertyData_return;
    }

    int __wrap_getDevicePropertyData(const char* property, char* value, int size) {
        mock_control.getDevicePropertyData_called = true;
        if (property) {
            strncpy(mock_control.getDevicePropertyData_last_property, property,
                    sizeof(mock_control.getDevicePropertyData_last_property) - 1);
            mock_control.getDevicePropertyData_last_property[
                sizeof(mock_control.getDevicePropertyData_last_property) - 1] = '\0';

            // Return per-property values
            if (strcmp(property, "APP_PERSISTENT_PATH") == 0) {
                if (value && size > 0) {
                    snprintf(value, size, "%s",
                             mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_value);
                }
                return mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return;
            }
            if (strcmp(property, "HDD_ENABLED") == 0) {
                if (value && size > 0) {
                    snprintf(value, size, "%s",
                             mock_control.getDevicePropertyData_HDD_ENABLED_value);
                }
                return mock_control.getDevicePropertyData_HDD_ENABLED_return;
            }
        }
        // Fallback for unknown properties
        return mock_control.getDevicePropertyData_return;
    }
}

// ================================================================================================
// Test Fixture
// ================================================================================================

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&mock_control, 0, sizeof(mock_control));
        mock_control.getIncludePropertyData_return = -1;
        mock_control.getDevicePropertyData_return = -1;
        mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = -1;
        mock_control.getDevicePropertyData_HDD_ENABLED_return = -1;

        memset(&test_config, 0, sizeof(test_config));
    }

    backup_config_t test_config;
};

// ================================================================================================
// config_load() Tests — NULL parameter
// ================================================================================================

TEST_F(ConfigManagerTest, ConfigLoad_NullConfig) {
    int result = config_load(nullptr);
    EXPECT_EQ(result, BACKUP_ERROR_INVALID_PARAM);
}

// ================================================================================================
// config_load() Tests — LOG_PATH
// ================================================================================================

TEST_F(ConfigManagerTest, ConfigLoad_LogPathFromProperties) {
    mock_control.getIncludePropertyData_return = 0;
    strncpy(mock_control.getIncludePropertyData_value, "/var/logs",
            sizeof(mock_control.getIncludePropertyData_value) - 1);

    // Provide device properties so the rest of config_load completes
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = -1;
    mock_control.getDevicePropertyData_HDD_ENABLED_return = -1;

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_TRUE(mock_control.getIncludePropertyData_called);
    EXPECT_STREQ(test_config.log_path, "/opt/logs");
    EXPECT_STREQ(test_config.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_STREQ(test_config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
}

TEST_F(ConfigManagerTest, ConfigLoad_LogPathDefault) {
    mock_control.getIncludePropertyData_return = -1; // Property not found

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.log_path, "/opt/logs");
    EXPECT_STREQ(test_config.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_STREQ(test_config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
}

TEST_F(ConfigManagerTest, ConfigLoad_LogPathEmptyString) {
    mock_control.getIncludePropertyData_return = 0;
    mock_control.getIncludePropertyData_value[0] = '\0'; // Empty

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    // Empty string should fall through to default
    EXPECT_STREQ(test_config.log_path, "/opt/logs");
}

// ================================================================================================
// config_load() Tests — APP_PERSISTENT_PATH
// ================================================================================================

TEST_F(ConfigManagerTest, ConfigLoad_PersistentPathFromProperties) {
    mock_control.getIncludePropertyData_return = -1; // Use default log path
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = UTILS_SUCCESS;
    strncpy(mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_value, "/opt/persistent",
            sizeof(mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_value) - 1);

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.persistent_path, "/opt/persistent");
}

TEST_F(ConfigManagerTest, ConfigLoad_PersistentPathDefault) {
    mock_control.getIncludePropertyData_return = -1;
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = -1;

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.persistent_path, "/opt/persistent");
}

TEST_F(ConfigManagerTest, ConfigLoad_PersistentPathEmptyString) {
    mock_control.getIncludePropertyData_return = -1;
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = UTILS_SUCCESS;
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_value[0] = '\0';

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    // Empty string should fall through to default
    EXPECT_STREQ(test_config.persistent_path, "/opt/persistent");
}

// ================================================================================================
// config_load() Tests — HDD_ENABLED
// ================================================================================================

TEST_F(ConfigManagerTest, ConfigLoad_HddEnabledFalse) {
    mock_control.getIncludePropertyData_return = -1;
    mock_control.getDevicePropertyData_HDD_ENABLED_return = UTILS_SUCCESS;
    strncpy(mock_control.getDevicePropertyData_HDD_ENABLED_value, "false",
            sizeof(mock_control.getDevicePropertyData_HDD_ENABLED_value) - 1);

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_FALSE(test_config.hdd_enabled);
}

TEST_F(ConfigManagerTest, ConfigLoad_HddEnabledNotFound) {
    mock_control.getIncludePropertyData_return = -1;
    mock_control.getDevicePropertyData_HDD_ENABLED_return = -1;

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_FALSE(test_config.hdd_enabled); // Default: false
}

// ================================================================================================
// config_load() Tests — Full configuration
// ================================================================================================

TEST_F(ConfigManagerTest, ConfigLoad_AllPropertiesSet) {
    mock_control.getIncludePropertyData_return = 0;
    strncpy(mock_control.getIncludePropertyData_value, "/opt/logs",
            sizeof(mock_control.getIncludePropertyData_value) - 1);

    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = UTILS_SUCCESS;
    strncpy(mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_value, "/opt/persistent",
            sizeof(mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_value) - 1);

    mock_control.getDevicePropertyData_HDD_ENABLED_return = UTILS_SUCCESS;
    strncpy(mock_control.getDevicePropertyData_HDD_ENABLED_value, "false",
            sizeof(mock_control.getDevicePropertyData_HDD_ENABLED_value) - 1);

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.log_path, "/opt/logs");
    EXPECT_STREQ(test_config.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_STREQ(test_config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
    EXPECT_STREQ(test_config.persistent_path, "/opt/persistent");
EXPECT_FALSE(test_config.hdd_enabled);
}

TEST_F(ConfigManagerTest, ConfigLoad_AllPropertiesMissing) {
    mock_control.getIncludePropertyData_return = -1;
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = -1;
    mock_control.getDevicePropertyData_HDD_ENABLED_return = -1;

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.log_path, "/opt/logs");
    EXPECT_STREQ(test_config.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_STREQ(test_config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
    EXPECT_STREQ(test_config.persistent_path, "/opt/persistent");
    EXPECT_FALSE(test_config.hdd_enabled);
}

// ================================================================================================
// config_load() Tests — Derived path construction
// ================================================================================================

TEST_F(ConfigManagerTest, ConfigLoad_DerivedPathsCorrect) {
    mock_control.getIncludePropertyData_return = 0;
    strncpy(mock_control.getIncludePropertyData_value, "/opt/logs",
            sizeof(mock_control.getIncludePropertyData_value) - 1);

    int result = config_load(&test_config);

    EXPECT_EQ(result, BACKUP_SUCCESS);
    EXPECT_STREQ(test_config.prev_log_path, "/opt/logs/PreviousLogs");
    EXPECT_STREQ(test_config.prev_log_backup_path, "/opt/logs/PreviousLogs_backup");
}

TEST_F(ConfigManagerTest, ConfigLoad_PropertyQueriedCorrectly) {
    mock_control.getIncludePropertyData_return = -1;
    mock_control.getDevicePropertyData_APP_PERSISTENT_PATH_return = -1;
    mock_control.getDevicePropertyData_HDD_ENABLED_return = -1;

    config_load(&test_config);

    EXPECT_TRUE(mock_control.getIncludePropertyData_called);
    EXPECT_STREQ(mock_control.getIncludePropertyData_last_property, "LOG_PATH");
    EXPECT_TRUE(mock_control.getDevicePropertyData_called);
}

// ================================================================================================
// Main
// ================================================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
