# Low-Level Design: backup_logs.sh Migration to C

## 1. Overview

This Low-Level Design (LLD) document provides detailed implementation specifications for migrating the `backup_logs.sh` shell script to C code for embedded RDK systems.

## 2. Detailed Data Structures

### 2.1 Core Configuration Structure
```c
#define MAX_PATH_LEN 4096
#define MAX_MESSAGE_LEN 256
#define MAX_EXTENSION_LEN 32
#define MAX_FILES_PER_DIR 10000
#define MAX_CONFIG_LINE 512

typedef struct backup_config {
    char log_path[MAX_PATH_LEN];
    char prev_log_path[MAX_PATH_LEN];
    char prev_log_backup_path[MAX_PATH_LEN];
    char persistent_path[MAX_PATH_LEN];
    bool hdd_enabled;
    bool config_loaded;
} backup_config_t;
```

### 2.2 File Operation Structures
```c
typedef enum {
    BACKUP_OP_MOVE,
    BACKUP_OP_COPY,
    BACKUP_OP_DELETE,
    BACKUP_OP_TOUCH
} backup_operation_type_t;

typedef struct file_entry {
    char filename[MAX_PATH_LEN];
    char source_path[MAX_PATH_LEN];
    char dest_path[MAX_PATH_LEN];
    backup_operation_type_t operation;
    time_t timestamp;
} file_entry_t;

typedef struct file_list {
    file_entry_t files[MAX_FILES_PER_DIR];
    int count;
    int capacity;
} file_list_t;
```

### 2.3 Error Handling Structures
```c
typedef enum {
    ERROR_SUCCESS = 0,
    ERROR_CONFIG_INVALID = 1,
    ERROR_FILESYSTEM = 2,
    ERROR_PERMISSION = 3,
    ERROR_RESOURCE = 4,
    ERROR_SYSTEM = 5
} error_code_t;

typedef struct error_context {
    error_code_t code;
    char message[MAX_MESSAGE_LEN];
    const char* function_name;
    int line_number;
    time_t timestamp;
} error_context_t;
```

### 2.4 Backup Level Tracking
```c
typedef enum {
    BACKUP_LEVEL_NONE = -1,
    BACKUP_LEVEL_BASE = 0,
    BACKUP_LEVEL_BAK1 = 1,
    BACKUP_LEVEL_BAK2 = 2,
    BACKUP_LEVEL_BAK3 = 3
} backup_level_t;

typedef struct backup_state {
    backup_level_t current_level;
    bool has_existing_backup;
    char timestamp_str[32];  // Format: MM-DD-YY-HH-MM-SSAM
} backup_state_t;
```

## 3. Module Interface Definitions

### 3.1 Configuration Manager Module
```c
// config_manager.h

// Initialize configuration system
int config_init(void);

// Load configuration from files
int config_load(backup_config_t* config);

// Validate loaded configuration
int config_validate(const backup_config_t* config);

// Parse shell-format property file
int config_parse_properties(const char* filepath, backup_config_t* config);

// Parse environment setup script
int config_parse_env_script(const char* filepath, backup_config_t* config);

// Get specific configuration values
const char* config_get_log_path(const backup_config_t* config);
bool config_is_hdd_enabled(const backup_config_t* config);
const char* config_get_persistent_path(const backup_config_t* config);

// Cleanup configuration resources
void config_cleanup(backup_config_t* config);
```

### 3.2 Directory Manager Module
```c
// directory_manager.h

// Create directory with proper permissions
int dir_create(const char* path, mode_t mode);

// Create directory recursively (like mkdir -p)
int dir_create_recursive(const char* path, mode_t mode);

// Check if directory exists
bool dir_exists(const char* path);

// Create all required backup directories
int dir_create_workspace(const backup_config_t* config);

// Clean directory contents matching pattern
int dir_cleanup(const char* path, const char* pattern);

// Get directory size (total bytes)
long dir_get_size(const char* path);

// Validate directory permissions
int dir_check_permissions(const char* path, int required_perms);
```

### 3.3 File Operations Module
```c
// file_operations.h

// Initialize file operations system
int fileops_init(void);

// Move file atomically
int fileops_move(const char* source, const char* dest);

// Copy file with verification
int fileops_copy(const char* source, const char* dest);

// Create empty file (touch)
int fileops_touch(const char* path);

// Check file existence
bool fileops_exists(const char* path);

// Find files matching pattern
int fileops_find_pattern(const char* directory, const char* pattern, 
                        file_list_t* results);

// Get file size
long fileops_get_size(const char* path);

// Verify file integrity after operation
int fileops_verify_integrity(const char* path, const char* checksum);

// Remove file safely
int fileops_remove(const char* path);

// Cleanup file operations resources
void fileops_cleanup(void);
```

### 3.4 Backup Engine Module
```c
// backup_engine.h

// Initialize backup engine
int backup_init(const backup_config_t* config);

// Determine current backup level for HDD-disabled devices
backup_level_t backup_get_level_hdd_disabled(const backup_config_t* config);

// Execute HDD-enabled backup strategy
int backup_execute_hdd_enabled(const backup_config_t* config);

// Execute HDD-disabled backup strategy  
int backup_execute_hdd_disabled(const backup_config_t* config);

// Perform log rotation for HDD-disabled devices
int backup_rotate_logs(const backup_config_t* config, backup_level_t current_level);

// Create timestamped backup directory
int backup_create_timestamped_dir(const backup_config_t* config, 
                                 char* timestamp_dir, size_t dir_size);

// Create last_reboot marker file
int backup_create_reboot_marker(const char* directory);

// Remove old reboot markers
int backup_remove_old_markers(const char* directory);

// Cleanup backup engine resources
void backup_cleanup(void);
```

### 3.5 System Integration Module
```c
// system_integration.h

// Initialize system integration
int sysint_init(void);

// Send systemd notification
int sysint_notify_systemd(const char* message);

// Execute external script safely
int sysint_execute_script(const char* script_path, const char* args);

// Check disk threshold
int sysint_check_disk_threshold(void);

// Generate timestamp string
int sysint_get_timestamp(char* buffer, size_t buffer_size);

// Create persistent marker file
int sysint_create_persistent_marker(const char* path);

// Cleanup system integration resources
void sysint_cleanup(void);
```

### 3.6 Error Handler and Logger Module
```c
// error_logger.h

// Initialize logging system
int logger_init(const char* program_name);

// Log message with level
int logger_log(int level, const char* function, int line, const char* format, ...);

// Log error with context
int logger_error(const error_context_t* context);

// Set error context
void logger_set_error(error_context_t* context, error_code_t code, 
                     const char* function, int line, const char* message);

// Get last error
const error_context_t* logger_get_last_error(void);

// Clear error state
void logger_clear_error(void);

// Cleanup logging resources
void logger_cleanup(void);

// Convenience macros
#define LOG_ERROR(ctx, code, msg) \
    logger_set_error(ctx, code, __FUNCTION__, __LINE__, msg)
    
#define LOG_INFO(msg, ...) \
    logger_log(LOG_LEVEL_INFO, __FUNCTION__, __LINE__, msg, ##__VA_ARGS__)
    
#define LOG_WARN(msg, ...) \
    logger_log(LOG_LEVEL_WARN, __FUNCTION__, __LINE__, msg, ##__VA_ARGS__)
```

## 4. Detailed Algorithms

### 4.1 Configuration Parsing Algorithm
```c
int config_parse_shell_variable(const char* line, char* key, char* value) {
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\0' || line[0] == '\n') {
        return 0;
    }
    
    // Find assignment operator
    char* equals = strchr(line, '=');
    if (!equals) {
        return -1;  // Invalid format
    }
    
    // Extract key (trim whitespace)
    size_t key_len = equals - line;
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    // Trim trailing whitespace from key
    while (key_len > 0 && isspace(key[key_len-1])) {
        key[--key_len] = '\0';
    }
    
    // Extract value (handle quotes and expansion)
    char* value_start = equals + 1;
    // Skip leading whitespace
    while (*value_start && isspace(*value_start)) {
        value_start++;
    }
    
    // Handle quoted values
    if (*value_start == '"' || *value_start == '\'') {
        char quote = *value_start;
        value_start++;
        char* quote_end = strchr(value_start, quote);
        if (quote_end) {
            size_t value_len = quote_end - value_start;
            strncpy(value, value_start, value_len);
            value[value_len] = '\0';
        } else {
            return -1;  // Unterminated quote
        }
    } else {
        // Unquoted value - take until newline or comment
        strcpy(value, value_start);
        char* comment = strchr(value, '#');
        if (comment) {
            *comment = '\0';
        }
        // Trim trailing whitespace
        size_t value_len = strlen(value);
        while (value_len > 0 && isspace(value[value_len-1])) {
            value[--value_len] = '\0';
        }
    }
    
    return 1;  // Success
}
```

### 4.2 HDD-Disabled Backup Level Detection
```c
backup_level_t backup_detect_level_hdd_disabled(const backup_config_t* config) {
    char filepath[MAX_PATH_LEN];
    
    // Check for messages.txt (base level)
    snprintf(filepath, sizeof(filepath), "%s/messages.txt", config->prev_log_path);
    if (!fileops_exists(filepath)) {
        return BACKUP_LEVEL_NONE;
    }
    
    // Check for bak1_messages.txt
    snprintf(filepath, sizeof(filepath), "%s/bak1_messages.txt", config->prev_log_path);
    if (!fileops_exists(filepath)) {
        return BACKUP_LEVEL_BASE;
    }
    
    // Check for bak2_messages.txt
    snprintf(filepath, sizeof(filepath), "%s/bak2_messages.txt", config->prev_log_path);
    if (!fileops_exists(filepath)) {
        return BACKUP_LEVEL_BAK1;
    }
    
    // Check for bak3_messages.txt  
    snprintf(filepath, sizeof(filepath), "%s/bak3_messages.txt", config->prev_log_path);
    if (!fileops_exists(filepath)) {
        return BACKUP_LEVEL_BAK2;
    }
    
    return BACKUP_LEVEL_BAK3;  // All levels exist, need rotation
}
```

### 4.3 File Pattern Matching Algorithm
```c
int fileops_find_pattern_impl(const char* directory, const char* pattern, 
                             file_list_t* results) {
    DIR* dir = opendir(directory);
    if (!dir) {
        return -1;
    }
    
    struct dirent* entry;
    results->count = 0;
    
    while ((entry = readdir(dir)) != NULL && results->count < results->capacity) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if filename matches any of the patterns
        bool matches = false;
        
        // Support multiple patterns: *.txt*, *.log*, *.bin*, bootlog
        if (strstr(pattern, "*.txt*") && 
            (strstr(entry->d_name, ".txt") || strstr(entry->d_name, ".TXT"))) {
            matches = true;
        } else if (strstr(pattern, "*.log*") && 
                  (strstr(entry->d_name, ".log") || strstr(entry->d_name, ".LOG"))) {
            matches = true;
        } else if (strstr(pattern, "*.bin*") && 
                  (strstr(entry->d_name, ".bin") || strstr(entry->d_name, ".BIN"))) {
            matches = true;
        } else if (strstr(pattern, "bootlog") && 
                  strcmp(entry->d_name, "bootlog") == 0) {
            matches = true;
        }
        
        if (matches) {
            // Build full path
            snprintf(results->files[results->count].filename, 
                    sizeof(results->files[results->count].filename),
                    "%s", entry->d_name);
            snprintf(results->files[results->count].source_path,
                    sizeof(results->files[results->count].source_path),
                    "%s/%s", directory, entry->d_name);
            results->count++;
        }
    }
    
    closedir(dir);
    return results->count;
}
```

### 4.4 Log Rotation Algorithm for HDD-Disabled Devices
```c
int backup_rotate_files_hdd_disabled(const backup_config_t* config) {
    char source_path[MAX_PATH_LEN];
    char dest_path[MAX_PATH_LEN];
    file_list_t file_list = {0};
    file_list.capacity = MAX_FILES_PER_DIR;
    
    // Step 1: Move bak1_ files to base names (bak1_messages.txt -> messages.txt)
    if (fileops_find_pattern(config->prev_log_path, "bak1_*", &file_list) > 0) {
        for (int i = 0; i < file_list.count; i++) {
            // Remove bak1_ prefix
            const char* base_name = file_list.files[i].filename + 5; // Skip "bak1_"
            snprintf(dest_path, sizeof(dest_path), "%s/%s", 
                    config->prev_log_path, base_name);
            
            if (fileops_move(file_list.files[i].source_path, dest_path) != 0) {
                LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, 
                         "Failed to rotate bak1 file to base");
                return -1;
            }
        }
    }
    
    // Step 2: Move bak2_ files to bak1_ names 
    file_list.count = 0;  // Reset list
    if (fileops_find_pattern(config->prev_log_path, "bak2_*", &file_list) > 0) {
        for (int i = 0; i < file_list.count; i++) {
            // Replace bak2_ with bak1_
            const char* base_name = file_list.files[i].filename + 5; // Skip "bak2_"
            snprintf(dest_path, sizeof(dest_path), "%s/bak1_%s", 
                    config->prev_log_path, base_name);
            
            if (fileops_move(file_list.files[i].source_path, dest_path) != 0) {
                LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, 
                         "Failed to rotate bak2 file to bak1");
                return -1;
            }
        }
    }
    
    // Step 3: Move bak3_ files to bak2_ names
    file_list.count = 0;  // Reset list
    if (fileops_find_pattern(config->prev_log_path, "bak3_*", &file_list) > 0) {
        for (int i = 0; i < file_list.count; i++) {
            // Replace bak3_ with bak2_
            const char* base_name = file_list.files[i].filename + 5; // Skip "bak3_"
            snprintf(dest_path, sizeof(dest_path), "%s/bak2_%s", 
                    config->prev_log_path, base_name);
            
            if (fileops_move(file_list.files[i].source_path, dest_path) != 0) {
                LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, 
                         "Failed to rotate bak3 file to bak2");
                return -1;
            }
        }
    }
    
    // Step 4: Move current logs to bak3_ names
    file_list.count = 0;  // Reset list
    char log_patterns[] = "*.txt*,*.log*,*.bin*,bootlog";
    if (fileops_find_pattern(config->log_path, log_patterns, &file_list) > 0) {
        for (int i = 0; i < file_list.count; i++) {
            snprintf(dest_path, sizeof(dest_path), "%s/bak3_%s", 
                    config->prev_log_path, file_list.files[i].filename);
            
            if (fileops_move(file_list.files[i].source_path, dest_path) != 0) {
                LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, 
                         "Failed to move current log to bak3");
                return -1;
            }
        }
    }
    
    return 0;  // Success
}
```

### 4.5 Timestamp Generation Algorithm
```c
int sysint_generate_timestamp(char* buffer, size_t buffer_size) {
    time_t raw_time;
    struct tm* time_info;
    
    // Get current time
    time(&raw_time);
    time_info = localtime(&raw_time);
    
    if (!time_info) {
        return -1;
    }
    
    // Format: MM-DD-YY-HH-MM-SSAM (e.g., 03-05-26-02-30-45PM)
    char am_pm = (time_info->tm_hour >= 12) ? 'P' : 'A';
    int hour_12 = time_info->tm_hour;
    if (hour_12 == 0) {
        hour_12 = 12;  // 12 AM
    } else if (hour_12 > 12) {
        hour_12 -= 12;  // Convert to 12-hour format
    }
    
    int bytes_written = snprintf(buffer, buffer_size, 
                               "%02d-%02d-%02d-%02d-%02d-%02d%cM",
                               time_info->tm_mon + 1,  // Month (1-12)
                               time_info->tm_mday,     // Day (1-31)
                               time_info->tm_year % 100, // Year (2-digit)
                               hour_12,                // Hour (1-12)
                               time_info->tm_min,      // Minute (0-59)
                               time_info->tm_sec,      // Second (0-59)
                               am_pm);                 // AM/PM
    
    if (bytes_written < 0 || bytes_written >= buffer_size) {
        return -1;  // Buffer overflow or formatting error
    }
    
    return 0;  // Success
}
```

## 5. Error Handling Implementation

### 5.1 Error Context Management
```c
static error_context_t g_last_error = {0};

void logger_set_error(error_context_t* context, error_code_t code,
                     const char* function, int line, const char* message) {
    if (!context) {
        context = &g_last_error;
    }
    
    context->code = code;
    context->function_name = function;
    context->line_number = line;
    time(&context->timestamp);
    
    // Copy message safely
    if (message) {
        strncpy(context->message, message, sizeof(context->message) - 1);
        context->message[sizeof(context->message) - 1] = '\0';
    } else {
        context->message[0] = '\0';
    }
}

int logger_error(const error_context_t* context) {
    struct tm* time_info = localtime(&context->timestamp);
    char time_str[64];
    
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
    
    fprintf(stderr, "[%s] ERROR %d in %s:%d: %s\n",
            time_str, context->code, context->function_name,
            context->line_number, context->message);
    
    return context->code;
}
```

### 5.2 Recovery Strategies
```c
int backup_recover_from_partial_state(const backup_config_t* config) {
    // Check for incomplete operations by looking for temporary files
    file_list_t temp_files = {0};
    temp_files.capacity = MAX_FILES_PER_DIR;
    
    // Look for .tmp, .bak, or other temporary extensions
    if (fileops_find_pattern(config->log_path, "*.tmp", &temp_files) > 0) {
        LOG_WARN("Found %d temporary files, attempting recovery", temp_files.count);
        
        for (int i = 0; i < temp_files.count; i++) {
            // Try to determine original filename
            char original_name[MAX_PATH_LEN];
            strncpy(original_name, temp_files.files[i].filename, 
                   strlen(temp_files.files[i].filename) - 4); // Remove .tmp
            original_name[strlen(temp_files.files[i].filename) - 4] = '\0';
            
            char original_path[MAX_PATH_LEN];
            snprintf(original_path, sizeof(original_path), "%s/%s",
                    config->log_path, original_name);
            
            // If original doesn't exist, restore from temp
            if (!fileops_exists(original_path)) {
                if (fileops_move(temp_files.files[i].source_path, original_path) == 0) {
                    LOG_INFO("Recovered file: %s", original_name);
                }
            } else {
                // Original exists, remove temp file
                fileops_remove(temp_files.files[i].source_path);
            }
        }
    }
    
    // Check for incomplete backup directories
    DIR* dir = opendir(config->prev_log_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // Look for directories with .incomplete suffix
            if (strstr(entry->d_name, ".incomplete")) {
                char incomplete_path[MAX_PATH_LEN];
                snprintf(incomplete_path, sizeof(incomplete_path), "%s/%s",
                        config->prev_log_path, entry->d_name);
                
                LOG_WARN("Found incomplete backup directory: %s", incomplete_path);
                // Remove incomplete backup directory
                dir_cleanup(incomplete_path, "*");
                rmdir(incomplete_path);
            }
        }
        closedir(dir);
    }
    
    return 0;
}
```

## 6. Memory Management Strategy

### 6.1 Fixed Buffer Pool Implementation
```c
#define BUFFER_POOL_SIZE 10
#define BUFFER_SIZE 4096

static struct {
    char buffers[BUFFER_POOL_SIZE][BUFFER_SIZE];
    bool in_use[BUFFER_POOL_SIZE];
    int allocated_count;
} g_buffer_pool = {0};

char* buffer_pool_allocate(void) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!g_buffer_pool.in_use[i]) {
            g_buffer_pool.in_use[i] = true;
            g_buffer_pool.allocated_count++;
            return g_buffer_pool.buffers[i];
        }
    }
    return NULL; // Pool exhausted
}

void buffer_pool_free(char* buffer) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (g_buffer_pool.buffers[i] == buffer) {
            g_buffer_pool.in_use[i] = false;
            g_buffer_pool.allocated_count--;
            return;
        }
    }
}

int buffer_pool_get_usage(void) {
    return g_buffer_pool.allocated_count;
}
```

### 6.2 Stack-based File Operation
```c
int fileops_move_safe(const char* source, const char* dest) {
    char temp_dest[MAX_PATH_LEN];  // Stack allocation
    error_context_t error_ctx = {0}; // Stack allocation
    
    // Create temporary destination name
    snprintf(temp_dest, sizeof(temp_dest), "%s.tmp", dest);
    
    // Step 1: Copy to temporary location
    if (fileops_copy(source, temp_dest) != 0) {
        LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, "Failed to copy to temporary location");
        return -1;
    }
    
    // Step 2: Verify copy integrity
    if (fileops_get_size(source) != fileops_get_size(temp_dest)) {
        fileops_remove(temp_dest);
        LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, "File size mismatch after copy");
        return -1;
    }
    
    // Step 3: Atomic rename
    if (rename(temp_dest, dest) != 0) {
        fileops_remove(temp_dest);
        LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, "Failed to rename to final destination");
        return -1;
    }
    
    // Step 4: Remove original
    if (fileops_remove(source) != 0) {
        // Log warning but don't fail the operation
        LOG_WARN("Failed to remove source file: %s", source);
    }
    
    return 0;  // Success
}
```

## 7. Performance Optimization Techniques

### 7.1 Batch File Operations
```c
int fileops_batch_move(const file_list_t* file_list, const char* dest_dir) {
    int success_count = 0;
    int total_files = file_list->count;
    
    // Pre-allocate destination paths to avoid repeated allocations
    char dest_paths[MAX_FILES_PER_DIR][MAX_PATH_LEN];
    
    // Prepare all destination paths first
    for (int i = 0; i < total_files; i++) {
        snprintf(dest_paths[i], sizeof(dest_paths[i]), "%s/%s",
                dest_dir, file_list->files[i].filename);
    }
    
    // Execute moves in batch with progress tracking
    for (int i = 0; i < total_files; i++) {
        if (fileops_move_safe(file_list->files[i].source_path, dest_paths[i]) == 0) {
            success_count++;
        } else {
            LOG_WARN("Failed to move file %d of %d: %s", 
                    i + 1, total_files, file_list->files[i].filename);
        }
        
        // Report progress every 100 files for large operations
        if (total_files > 100 && (i + 1) % 100 == 0) {
            LOG_INFO("Moved %d of %d files (%d%%)", success_count, i + 1, 
                    ((i + 1) * 100) / total_files);
        }
    }
    
    LOG_INFO("Batch move completed: %d of %d files successful", 
             success_count, total_files);
    
    return (success_count == total_files) ? 0 : -1;
}
```

### 7.2 Efficient Directory Traversal
```c
int fileops_find_pattern_optimized(const char* directory, const char* pattern,
                                  file_list_t* results) {
    DIR* dir = opendir(directory);
    if (!dir) {
        return -1;
    }
    
    struct dirent* entry;
    results->count = 0;
    
    // Pre-compile pattern matching criteria for efficiency
    bool match_txt = strstr(pattern, "*.txt*") != NULL;
    bool match_log = strstr(pattern, "*.log*") != NULL;
    bool match_bin = strstr(pattern, "*.bin*") != NULL;
    bool match_bootlog = strstr(pattern, "bootlog") != NULL;
    
    while ((entry = readdir(dir)) != NULL && results->count < results->capacity) {
        // Quick checks first (most common rejects)
        if (entry->d_name[0] == '.') {
            continue; // Skip hidden files and . / ..
        }
        
        bool matches = false;
        const char* name = entry->d_name;
        size_t name_len = strlen(name);
        
        // Optimized pattern matching
        if (match_bootlog && name_len == 7 && strcmp(name, "bootlog") == 0) {
            matches = true;
        } else if (name_len >= 4) { // Minimum length for extensions
            // Check extensions efficiently
            if (match_txt && (strcasestr(name, ".txt") != NULL)) {
                matches = true;
            } else if (match_log && (strcasestr(name, ".log") != NULL)) {
                matches = true;
            } else if (match_bin && (strcasestr(name, ".bin") != NULL)) {
                matches = true;
            }
        }
        
        if (matches) {
            // Use pointer arithmetic for efficiency
            snprintf(results->files[results->count].filename, MAX_PATH_LEN, "%s", name);
            snprintf(results->files[results->count].source_path, MAX_PATH_LEN,
                    "%s/%s", directory, name);
            results->count++;
        }
    }
    
    closedir(dir);
    return results->count;
}
```

## 8. Resource Management

### 8.1 Resource Cleanup Framework
```c
typedef struct cleanup_handler {
    void (*cleanup_func)(void*);
    void* resource;
    struct cleanup_handler* next;
} cleanup_handler_t;

static cleanup_handler_t* g_cleanup_list = NULL;

int register_cleanup(void (*cleanup_func)(void*), void* resource) {
    cleanup_handler_t* handler = malloc(sizeof(cleanup_handler_t));
    if (!handler) {
        return -1;
    }
    
    handler->cleanup_func = cleanup_func;
    handler->resource = resource;
    handler->next = g_cleanup_list;
    g_cleanup_list = handler;
    
    return 0;
}

void execute_all_cleanup(void) {
    cleanup_handler_t* current = g_cleanup_list;
    while (current) {
        if (current->cleanup_func && current->resource) {
            current->cleanup_func(current->resource);
        }
        cleanup_handler_t* next = current->next;
        free(current);
        current = next;
    }
    g_cleanup_list = NULL;
}

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    LOG_INFO("Received signal %d, cleaning up resources", sig);
    execute_all_cleanup();
    exit(sig);
}
```

### 8.2 File Descriptor Management
```c
#define MAX_OPEN_FILES 64

static struct {
    FILE* handles[MAX_OPEN_FILES];
    char paths[MAX_OPEN_FILES][MAX_PATH_LEN];
    int count;
} g_file_registry = {0};

FILE* managed_fopen(const char* path, const char* mode) {
    if (g_file_registry.count >= MAX_OPEN_FILES) {
        LOG_ERROR(NULL, ERROR_RESOURCE, "Too many open files");
        return NULL;
    }
    
    FILE* fp = fopen(path, mode);
    if (fp) {
        g_file_registry.handles[g_file_registry.count] = fp;
        strncpy(g_file_registry.paths[g_file_registry.count], path, MAX_PATH_LEN - 1);
        g_file_registry.count++;
    }
    
    return fp;
}

void managed_fclose_all(void) {
    for (int i = 0; i < g_file_registry.count; i++) {
        if (g_file_registry.handles[i]) {
            fclose(g_file_registry.handles[i]);
            g_file_registry.handles[i] = NULL;
        }
    }
    g_file_registry.count = 0;
}
```

## 9. Main Program Structure

### 9.1 Main Function Implementation
```c
int main(int argc, char* argv[]) {
    backup_config_t config = {0};
    error_context_t error_ctx = {0};
    int exit_code = ERROR_SUCCESS;
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    do {
        // Initialize all subsystems
        if (logger_init("backup_logs") != 0) {
            fprintf(stderr, "Failed to initialize logging system\n");
            exit_code = ERROR_SYSTEM;
            break;
        }
        
        if (config_init() != 0) {
            LOG_ERROR(&error_ctx, ERROR_SYSTEM, "Failed to initialize configuration system");
            exit_code = ERROR_SYSTEM;
            break;
        }
        
        // Load and validate configuration
        if (config_load(&config) != 0) {
            LOG_ERROR(&error_ctx, ERROR_CONFIG_INVALID, "Failed to load configuration");
            exit_code = ERROR_CONFIG_INVALID;
            break;
        }
        
        if (config_validate(&config) != 0) {
            LOG_ERROR(&error_ctx, ERROR_CONFIG_INVALID, "Configuration validation failed");
            exit_code = ERROR_CONFIG_INVALID;
            break;
        }
        
        LOG_INFO("Configuration loaded successfully");
        
        // Create workspace directories
        if (dir_create_workspace(&config) != 0) {
            LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, "Failed to create workspace directories");
            exit_code = ERROR_FILESYSTEM;
            break;
        }
        
        // Check disk threshold
        if (sysint_check_disk_threshold() != 0) {
            LOG_WARN("Disk threshold check failed or reported issues");
            // Continue execution - not a fatal error
        }
        
        // Attempt recovery from any partial state
        if (backup_recover_from_partial_state(&config) != 0) {
            LOG_WARN("Partial state recovery had issues");
            // Continue execution
        }
        
        // Execute appropriate backup strategy
        if (config.hdd_enabled) {
            LOG_INFO("Executing HDD-enabled backup strategy");
            if (backup_execute_hdd_enabled(&config) != 0) {
                LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, "HDD-enabled backup failed");
                exit_code = ERROR_FILESYSTEM;
                break;
            }
        } else {
            LOG_INFO("Executing HDD-disabled backup strategy");
            if (backup_execute_hdd_disabled(&config) != 0) {
                LOG_ERROR(&error_ctx, ERROR_FILESYSTEM, "HDD-disabled backup failed");
                exit_code = ERROR_FILESYSTEM;
                break;
            }
        }
        
        // Clean current log directory
        if (dir_cleanup(config.log_path, "*.txt*,*.log*,*-*-*-*-*M-") != 0) {
            LOG_WARN("Failed to clean current log directory");
            // Continue - not fatal
        }
        
        // Copy version files
        if (backup_copy_version_files(&config) != 0) {
            LOG_WARN("Failed to copy some version files");
            // Continue - not fatal
        }
        
        // Handle special log files
        if (backup_handle_special_files(&config) != 0) {
            LOG_WARN("Failed to handle some special log files");
            // Continue - not fatal
        }
        
        // Create persistent marker
        if (sysint_create_persistent_marker(config.persistent_path) != 0) {
            LOG_WARN("Failed to create persistent marker");
            // Continue - not fatal
        }
        
        // Send systemd notification
        if (sysint_notify_systemd("Logs Backup Done..!") != 0) {
            LOG_WARN("Failed to send systemd notification");
            // Continue - not fatal
        }
        
        LOG_INFO("Backup operation completed successfully");
        
    } while (0);  // Single execution with break-based error handling
    
    // Cleanup all resources
    execute_all_cleanup();
    managed_fclose_all();
    config_cleanup(&config);
    logger_cleanup();
    
    // Log final status
    if (exit_code != ERROR_SUCCESS) {
        logger_error(&error_ctx);
    }
    
    return exit_code;
}
```

This LLD provides comprehensive implementation details for migrating the backup_logs.sh script to C, including detailed algorithms, data structures, error handling, and performance optimizations specifically designed for embedded RDK systems.
