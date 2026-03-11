# High-Level Design: backup_logs.sh Migration to C

## 1. Overview

### 1.1 Purpose
This document outlines the High-Level Design (HLD) for migrating the existing `backup_logs.sh` shell script to a C implementation. The migration aims to improve performance, reduce memory footprint, and enhance reliability for embedded RDK systems.

### 1.2 Scope
- Migration of all functionality from `backup_logs.sh` to C code
- Support for both HDD-enabled and HDD-disabled devices
- Maintain compatibility with existing systemd integration
- Preserve log backup and rotation functionality

### 1.3 Constraints
- Target embedded systems with limited memory (few KBs to few MBs)
- CPU resources are constrained with low clock speeds
- Must be platform-neutral and portable across multiple architectures
- Minimize dynamic memory allocation
- Avoid floating-point arithmetic where possible
- Thread-safe implementation required

## 2. System Architecture

### 2.1 Architecture Overview
The C implementation will follow a modular design with the following key components:

```
backup_logs (main executable)
├── Configuration Manager
├── Directory Manager
├── Log Backup Engine
├── File Operations Manager
├── Disk Threshold Monitor
├── System Integration Module
└── Error Handler & Logger
```

### 2.2 Component Description

#### 2.2.1 Configuration Manager
- **Purpose**: Load and parse system configuration files
- **Responsibilities**:
  - Parse `/etc/include.properties`
  - Parse `/etc/device.properties`
  - Parse `/etc/env_setup.sh` if available
  - Parse `/etc/special_files.properties` for special files handling
  - Validate configuration parameters
  - Provide configuration data to other modules
  - Load and manage special files configuration for `/tmp` and `/etc` operations

#### 2.2.2 Directory Manager
- **Purpose**: Handle directory creation and validation
- **Responsibilities**:
  - Create log workspace directories
  - Validate directory permissions
  - Manage directory path resolution
  - Handle directory cleanup operations

#### 2.2.3 Log Backup Engine
- **Purpose**: Core backup logic implementation
- **Responsibilities**:
  - Implement HDD-enabled device backup strategy
  - Implement HDD-disabled device backup strategy with rotation
  - Handle log file identification and filtering
  - Execute backup operations based on device type

#### 2.2.4 File Operations Manager
- **Purpose**: Low-level file operations
- **Responsibilities**:
  - File moving and copying operations
  - File existence checking
  - Pattern-based file finding
  - Timestamp generation and management

#### 2.2.5 Disk Threshold Monitor
- **Purpose**: Monitor disk usage and trigger cleanup
- **Responsibilities**:
  - Check disk usage percentages
  - Trigger cleanup scripts when thresholds exceed
  - Integration with existing disk_threshold_check.sh

#### 2.2.6 System Integration Module
- **Purpose**: System-level integrations
- **Responsibilities**:
  - Systemd notification handling
  - Integration with external scripts
  - Process status reporting

#### 2.2.7 Error Handler & Logger
- **Purpose**: Centralized error handling and logging
- **Responsibilities**:
  - Structured error reporting
  - Log message formatting with timestamps
  - Error code standardization

## 3. Data Structures

### 3.1 Core Data Structures

```c
typedef struct {
    char log_path[PATH_MAX];
    char prev_log_path[PATH_MAX];
    char prev_log_backup_path[PATH_MAX];
    char persistent_path[PATH_MAX];
    bool hdd_enabled;
} backup_config_t;

typedef struct {
    char source_path[PATH_MAX];
    char dest_path[PATH_MAX];
    backup_operation_t operation;
    char source_extension[32];
    char dest_extension[32];
} backup_operation_t;

typedef enum {
    BACKUP_OP_MOVE,
    BACKUP_OP_COPY,
    BACKUP_OP_DELETE
} backup_operation_type_t;

typedef struct {
    int error_code;
    char error_message[256];
    const char* function_name;
    int line_number;
} error_info_t;
```

### 3.3 Special Files Configuration

#### 3.3.1 Special Files Data Structure
```c
typedef enum {
    SPECIAL_FILE_COPY = 0,  // cp operation
    SPECIAL_FILE_MOVE = 1   // mv operation
} special_file_operation_t;

typedef struct {
    char source_path[PATH_MAX];
    char destination_path[PATH_MAX];
    special_file_operation_t operation;
    char conditional_check[64];  // Optional condition variable name
} special_file_entry_t;

typedef struct {
    special_file_entry_t entries[MAX_SPECIAL_FILES];
    size_t count;
    bool config_loaded;
} special_files_config_t;

#define MAX_SPECIAL_FILES 32  // Maximum number of special files to handle
```
```

### 3.2 Memory Management Strategy
- Use fixed-size buffers to avoid dynamic allocation
- Implement memory pools for temporary operations
- Stack-based allocation for small, short-lived data
- Pre-allocated arrays for file lists and paths

#### 3.3.2 Special Files Configuration Format
The special files configuration follows a simple one-filename-per-line format for embedded system efficiency:
```properties
# Special Files Configuration for backup_logs
# Format: One source file path per line
# Comments start with # and empty lines are ignored
# 
# Operation determination is handled by the implementation:
#   - Files in /tmp are typically moved (mv operation)
#   - Configuration and version files are typically copied (cp operation)
#   - Destination is automatically determined based on source filename

# Files from /tmp directory (will be moved)
/tmp/disk_cleanup.log
/tmp/mount_log.txt
/tmp/mount-ta_log.txt

# Files from /etc directory (will be copied)
/etc/skyversion.txt
/etc/rippleversion.txt

# Version file from root (will be copied)
/version.txt
```

## 3.4 Special Files Configuration Management

### 3.4.1 Configuration Loading
The special files configuration is loaded from `/etc/special_files.properties` using a simple line-based parser that:
- Supports one filename per line format with comments (lines starting with #)
- Automatically determines operation type based on source file location
- Automatically determines destination filename from source pathname
- Uses LOG_PATH from backup configuration for destination directory
- Provides error reporting for missing or invalid files

### 3.4.2 Configuration Processing
- Files in `/tmp/` directory are moved (mv operation) to preserve space
- Configuration and version files are copied (cp operation) to preserve originals
- Destination directory is automatically set to the configured LOG_PATH
- Destination filename is extracted from the source file path
- Log warnings for entries with non-existent source files but continue processing

## 4. Module Interfaces

### 4.1 Configuration Manager Interface
```c
int config_load(backup_config_t* config);
int config_validate(const backup_config_t* config);
const char* config_get_log_path(void);
bool config_is_hdd_enabled(void);

// Special files configuration interface
int special_files_load_config(special_files_config_t* config, const char* config_file);
int special_files_validate_entry(const special_file_entry_t* entry);
int special_files_execute_entry(const special_file_entry_t* entry, 
                               const backup_config_t* backup_config);
int special_files_execute_all(const special_files_config_t* config, 
                             const backup_config_t* backup_config);
void special_files_cleanup(void);
```

### 4.2 Directory Manager Interface
```c
// Implemented using RDK system utilities
int createDir(char* path);           // Create directory if not exists
int emptyFolder(char* path);         // Clean directory contents
int filePresentCheck(const char* path); // Check if file/directory exists
int removeFile(const char* path);    // Remove file or directory
```

### 4.3 Log Backup Engine Interface
```c
int backup_execute_hdd_enabled_strategy(const backup_config_t* config);
int backup_execute_hdd_disabled_strategy(const backup_config_t* config);
int backup_execute_common_operations(const backup_config_t* config);
int move_log_files_by_pattern(const char* source_dir, const char* dest_dir);
```

### 4.4 File Operations Interface
```c
// Implemented using RDK system utilities
int copyFiles(const char* source, const char* dest);  // Copy file operation
int removeFile(const char* path);                     // Remove file operation
int filePresentCheck(const char* path);               // Check file existence
// Move is implemented as copy + remove sequence
```

## 5. Data Flow

### 5.1 Main Execution Flow (As Implemented)
1. **Initialization Phase**
   - Initialize RDK logging system with extended configuration
   - Load system configuration from RDK property APIs (`getIncludePropertyData`, `getDevicePropertyData`)
   - Create required directories: LOG_PATH, PREV_LOG_PATH, PREV_LOG_BACKUP_PATH
   - Clean PREV_LOG_BACKUP_PATH directory
   - Create persistent file marker (`logFileBackup`)
   - Run disk threshold check script if available

2. **Pre-Execution Phase**
   - Find and remove existing `last_reboot` marker files
   - Determine backup strategy based on HDD_ENABLED configuration

3. **Backup Execution Phase**
   - Execute appropriate backup strategy (HDD-enabled or HDD-disabled)
   - Move log files using pattern matching (*.txt*, *.log*, bootlog)
   - Handle log rotation for HDD-disabled devices

4. **Common Operations Phase**
   - Execute special files operations based on configuration
   - Copy system version files (skyversion.txt, rippleversion.txt, version.txt)
   - Create new `last_reboot` marker file
   - Send systemd notification if available

5. **Cleanup Phase**
   - Cleanup special files manager resources
   - Release all allocated resources
   - Report final execution status

### 5.2 Error Handling Flow
- Centralized error handling through error_info_t structure
- Error propagation through return codes
- Logging of all error conditions with context
- Graceful degradation on non-critical failures

### 5.3 Visual Flow Representation

The system workflow is represented through detailed diagrams stored in the [diagrams directory](diagrams/backup_logs_flowcharts.md). This includes both Mermaid-format diagrams and text-based alternatives for environments with limited diagram rendering capabilities.

#### 5.3.1 Main Backup Process Flow
The primary execution flow showing the complete backup process from initialization to completion, including decision points for HDD-enabled vs HDD-disabled devices. Available in both Mermaid and text formats.

#### 5.3.2 Component Interaction Sequence
A detailed sequence diagram illustrating the interactions between all system components, showing the order of operations and data flow between modules. Includes timing annotations and error paths.

#### 5.3.3 HDD Disabled Strategy Detail
A specialized flowchart focusing on the complex log rotation logic for HDD-disabled devices, showing the 4-level backup rotation mechanism with all decision points clearly marked.

#### 5.3.4 Error Handling and Recovery Flow
A comprehensive error handling flowchart showing how different types of errors are categorized, handled, and recovered from in the system. Includes recovery strategies and escalation paths.

## 6. Key Algorithms

### 6.1 HDD-Disabled Backup Algorithm
```
1. Check for existing backup levels (messages.txt, bak1_messages.txt, etc.)
2. If no existing backups:
   - Move all logs to PreviousLogs
3. If backup level 1 exists but not level 2:
   - Move current logs to PreviousLogs with bak1_ prefix
4. If backup levels 1-2 exist but not level 3:
   - Move current logs to PreviousLogs with bak2_ prefix
5. If all backup levels exist:
   - Rotate: bak1->root, bak2->bak1, bak3->bak2, current->bak3
6. Create last_reboot marker file
```

### 6.2 HDD-Enabled Backup Algorithm
```
1. If no messages.txt in PreviousLogs:
   - Move all logs to PreviousLogs
   - Create last_reboot marker
2. If messages.txt exists:
   - Remove existing last_reboot markers
   - Create timestamped backup directory
   - Move current logs to timestamped directory
   - Create last_reboot marker in timestamped directory
```

### 6.3 File Pattern Matching Algorithm
- Use POSIX-compliant pattern matching
- Support for wildcard patterns (*.txt, *.log, etc.)
- Efficient directory traversal with depth control
- Filter by file type (regular files vs. symbolic links)

### 6.5 Special Files Processing Algorithm
```
1. Load special files configuration from /etc/special_files.properties
2. Parse each line as a single source file path
3. Skip comment lines (starting with #) and empty lines
4. For each special file entry:
   a. Check if source file exists (log warning if not found, continue)
   b. Determine operation based on source path:
      - Files in /tmp/: move operation (copy + delete)
      - All other files: copy operation
   c. Extract filename from source path for destination
   d. Build full destination path using ${LOG_PATH}/filename
   e. Verify destination directory exists, create if needed
   f. Execute operation (copy or move) based on determination
   g. Log operation result and any errors
5. Update operation statistics and cleanup resources
```

### 6.6 Special Files Configuration Parser Algorithm
```
1. Open /etc/special_files.properties file
2. For each line:
   a. Skip empty lines and comments (lines starting with #)
   b. Trim whitespace and newline characters
   c. Store entire line as source_path in special_file_entry_t structure
   d. Extract filename from source path for destination_path
   e. Set default operation to SPECIAL_FILE_COPY (will be determined at execution)
   f. Set conditional_check to empty string
3. Return parsed configuration with entry count or error code
```

# Additional Data Structures

### 3.5 Enhanced Error Handling and Configuration Flags

```c
/* Complete error code enumeration */
typedef enum {
    BACKUP_SUCCESS = 0,
    BACKUP_ERROR_CONFIG = -1,
    BACKUP_ERROR_FILESYSTEM = -2,
    BACKUP_ERROR_PERMISSIONS = -3,
    BACKUP_ERROR_MEMORY = -4,
    BACKUP_ERROR_INVALID_PARAM = -5,
    BACKUP_ERROR_NOT_FOUND = -6,
    BACKUP_ERROR_DISK_FULL = -7,
    BACKUP_ERROR_SYSTEM = -8
} backup_result_t;

/* Configuration flags for advanced options */
typedef struct {
    bool debug_enabled;
    bool force_rotation;
    bool skip_disk_check;
    bool cleanup_enabled;
} backup_flags_t;

/* Additional constants */
#define MAX_ERROR_MESSAGE_LEN 256
#define MAX_FUNCTION_NAME_LEN 64
#define MAX_DESCRIPTION_LEN 128
#define MAX_CONDITIONAL_LEN 64
#define LOG_BACKUP_LOGS "LOG.RDK.BACKUPLOGS"
```
The special files processing is integrated into the main backup flow as follows:
- **Phase 1**: Load special files configuration during initialization
- **Phase 2**: Execute special file copy operations before log backup
- **Phase 3**: Execute special file move operations during cleanup phase
- **Phase 4**: Report special files operation status in final logging

## 7. Threading and Concurrency

### 7.1 Threading Strategy
- Single-threaded design for simplicity and reliability
- Thread-safe utility functions for potential future extensions
- Use of atomic operations for shared state (if any)

### 7.2 Synchronization
- File locking for critical operations
- Mutex protection for shared resources (if threading is added later)
- Process-level coordination through lockfiles

## 8. Performance Considerations

### 8.1 Memory Optimization
- Fixed-size buffers with compile-time sizing
- Stack allocation preference over heap allocation
- Minimal memory fragmentation through planned allocation patterns
- Efficient string handling with bounded operations

### 8.2 I/O Optimization
- Batch file operations where possible
- Minimize system calls through buffered operations
- Efficient directory traversal algorithms
- Streaming operations for large files

## 8.5 RDK Logger Integration

The implementation includes comprehensive RDK logging framework integration:

### 8.5.1 Logger Configuration
```c
#ifdef RDK_LOGGER_EXT
    rdk_logger_ext_config_t logger_config = {
        .pModuleName = "LOG.RDK.BACKUPLOGS",
        .loglevel = RDK_LOG_INFO,
        .output = RDKLOG_OUTPUT_CONSOLE,
        .format = RDKLOG_FORMAT_WITH_TS,
        .pFilePolicy = NULL
    };
    rdk_logger_ext_init(&logger_config);
#endif
```

### 8.5.2 Logger Features
- **Extended Logger Support**: Uses RDK_LOGGER_EXT for enhanced configuration
- **Timestamped Output**: All log messages include timestamps
- **Console Fallback**: Graceful handling when logger initialization fails
- **Debug INI Integration**: Reads configuration from `/etc/debug.ini`
- **Module-Specific Logging**: Uses dedicated "LOG.RDK.BACKUPLOGS" component

### 8.5.3 Build-time Configuration
```c
#ifdef RDK_LOGGER_EXT
#define RDK_LOGGER_ENABLED
#endif
```

The system supports both basic RDK logger and extended RDK logger configurations through compile-time flags.
### 8.6 CPU Optimization
- Avoid expensive operations in loops
- Use bit operations for flags and states
- Minimize string operations and use const strings where possible
- Efficient pattern matching algorithms

## 9. Integration Points

### 9.1 RDK System Integration
- **RDK Property System**: Integration with `getIncludePropertyData()` and `getDevicePropertyData()` APIs
- **RDK Logger Framework**: Full support for RDK logging with extended configuration
- **RDK Firmware Utils**: Integration with `fwutils` library for system operations
- **Systemd Integration**: Maintain compatibility with existing service files
- **External Scripts**: Integration with `disk_threshold_check.sh` and other system scripts

### 9.2 Configuration File Dependencies
- **Include Properties**: `/etc/include.properties` for LOG_PATH and other system paths
- **Device Properties**: `/etc/device.properties` for HDD_ENABLED and device-specific settings
- **Debug Configuration**: `/etc/debug.ini` for RDK logger configuration
- **Special Files**: `/etc/special_files.properties` for configurable file operations (one filename per line format)
- **Environment Setup**: `/etc/env_setup.sh` if available for additional environment variables

### 9.3 Build System Dependencies
```makefile
# Required libraries and flags
backup_logs_LDADD = -lm -lrdkloggers -lfwutils -lsystemd
backup_logs_CPPFLAGS = -DRDK_LOGGER_EXT
```

**Required Dependencies**:
- `librdkloggers` - RDK logging framework
- `libfwutils` - RDK firmware utilities for configuration and system operations
- `libsystemd` - Systemd integration for service notifications
- `libm` - Math library for any mathematical operations

### 9.4 Backward Compatibility
- Maintain existing directory structure and naming conventions
- Preserve log file formats and timestamps
- Keep existing environment variable usage
- Maintain compatibility with log analysis tools

## 10. Error Handling Strategy

### 10.1 Error Categories
- **Fatal Errors**: Configuration failures, permission issues
- **Recoverable Errors**: Individual file operation failures
- **Warnings**: Non-critical issues that don't prevent execution

### 10.2 Error Reporting
- Structured error codes for programmatic handling
- Human-readable error messages for debugging
- Integration with existing logging infrastructure
- Syslog integration for system-level error reporting

## 11. Testing Strategy

### 11.1 Unit Testing
- Test individual modules in isolation
- Mock external dependencies (file system, system calls)
- Comprehensive error condition testing
- Memory leak detection and prevention
- Special files configuration parser testing with various input formats
- Special files operation testing with different file permissions and paths

### 11.2 Integration Testing
- Test complete backup scenarios
- Verify compatibility with existing system
- Performance benchmarking against shell script
- Multi-platform validation
- Special files configuration end-to-end testing
- Variable substitution testing for different environment setups

### 11.3 System Testing
- End-to-end functionality verification
- Stress testing with large log volumes
- Resource constraint testing
- Recovery testing after various failure scenarios

## 12. Deployment Considerations

### 12.1 Build System
- Integration with existing autotools configuration
- Cross-compilation support for multiple architectures  
- Compiler optimization flags for embedded targets
- Static linking considerations for deployment
- **RDK-Specific Build Requirements**:
  - RDK Logger framework integration (`-lrdkloggers`)
  - RDK Firmware utilities integration (`-lfwutils`)
  - Systemd integration (`-lsystemd`)
  - Extended logger compile flag (`-DRDK_LOGGER_EXT`)

### 12.2 Installation
- Backward-compatible installation process
- Service file updates for systemd integration
- Configuration migration support
- Rollback capability

### 12.3 Monitoring
- Health check mechanisms
- Performance metrics collection
- Resource usage monitoring
- Integration with existing monitoring infrastructure

## 13. Future Enhancements

### 13.1 Planned Features
- Configuration hot-reloading capability
- Enhanced compression for archived logs
- Remote log backup capability
- Advanced filtering and retention policies

### 13.2 Extensibility
- Plugin architecture for custom backup strategies
- Configurable backup policies
- API for external tools integration
- Event-driven architecture support

## 14. Risk Analysis

### 14.1 Technical Risks
- **Memory Management**: Risk of memory leaks in embedded environment
- **File System Operations**: Race conditions with concurrent access
- **Configuration Parsing**: Compatibility issues with shell variable expansion
- **Performance**: Potential performance regression compared to shell script

### 14.2 Mitigation Strategies
- Comprehensive testing with memory analysis tools
- File locking and atomic operations for critical sections
- Robust configuration parsing with validation
- Performance benchmarking and optimization

## 15. Success Criteria

### 15.1 Functional Requirements
- ✅ Complete feature parity with existing shell script
- ✅ Support for both HDD-enabled and HDD-disabled devices
- ✅ Proper log rotation and backup functionality
- ✅ Integration with systemd and existing infrastructure

### 15.2 Non-Functional Requirements
- ✅ Memory usage reduction of at least 20% compared to shell process
- ✅ Startup time improvement of at least 30%
- ✅ CPU usage reduction during backup operations
- ✅ Cross-platform compatibility across target embedded systems

## 16. Implementation Status

### 16.1 Completed Features
- ✅ Complete modular architecture with defined components
- ✅ Core backup strategies for HDD-enabled and HDD-disabled devices
- ✅ RDK Logger framework integration with extended configuration
- ✅ Configuration management using RDK property APIs
- ✅ Special files handling with simplified configuration format
- ✅ Error handling with comprehensive error codes
- ✅ Build system integration with RDK dependencies
- ✅ Pattern-based log file identification and movement
- ✅ Systemd integration and external script execution

### 16.2 Current Limitations / Future Work
- ⚠️ **Command Line Options**: Function definitions exist but CLI parsing not implemented
- ⚠️ **Special Files Format**: Simplified one-filename-per-line format instead of full pipe-separated specification
- ⚠️ **Configuration Validation**: Basic validation implemented, could be enhanced
- ⚠️ **Advanced Configuration**: Variable substitution not implemented in special files

### 16.3 Architecture Decisions Made
- **Configuration Format**: Chose simplicity over full pipe-separated format for embedded efficiency
- **RDK Integration**: Deep integration with RDK APIs rather than generic POSIX-only approach
- **Error Handling**: Comprehensive error codes with graceful degradation on non-critical failures
- **Logging**: Full RDK logger integration with extended configuration options

This HLD provides a comprehensive roadmap for migrating the backup_logs.sh script to a robust, efficient C implementation suitable for embedded RDK environments.
