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
  - Validate configuration parameters
  - Provide configuration data to other modules

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

### 3.2 Memory Management Strategy
- Use fixed-size buffers to avoid dynamic allocation
- Implement memory pools for temporary operations
- Stack-based allocation for small, short-lived data
- Pre-allocated arrays for file lists and paths

## 4. Module Interfaces

### 4.1 Configuration Manager Interface
```c
int config_load(backup_config_t* config);
int config_validate(const backup_config_t* config);
const char* config_get_log_path(void);
bool config_is_hdd_enabled(void);
```

### 4.2 Directory Manager Interface
```c
int dir_create_workspace(const char* path);
int dir_create_if_not_exists(const char* path);
int dir_cleanup(const char* path, const char* pattern);
bool dir_exists(const char* path);
```

### 4.3 Log Backup Engine Interface
```c
int backup_execute_hdd_enabled_strategy(const backup_config_t* config);
int backup_execute_hdd_disabled_strategy(const backup_config_t* config);
int backup_and_recover_logs(const char* source, const char* dest, 
                           backup_operation_type_t op, const char* s_ext, 
                           const char* d_ext);
```

### 4.4 File Operations Interface
```c
int file_move(const char* source, const char* dest);
int file_copy(const char* source, const char* dest);
int file_find_pattern(const char* dir, const char* pattern, 
                     char results[][PATH_MAX], int max_results);
bool file_exists(const char* path);
int file_touch(const char* path);
```

## 5. Data Flow

### 5.1 Main Execution Flow
1. **Initialization Phase**
   - Load system configuration
   - Initialize logging subsystem
   - Validate runtime environment

2. **Preparation Phase**
   - Create required directories
   - Check disk thresholds
   - Determine backup strategy based on HDD status

3. **Backup Execution Phase**
   - Execute appropriate backup strategy
   - Handle log rotation (HDD-disabled devices)
   - Move/copy log files based on strategy

4. **Cleanup Phase**
   - Clean up old log files
   - Copy system version files
   - Send systemd notification

5. **Termination Phase**
   - Release resources
   - Report final status

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

### 8.3 CPU Optimization
- Avoid expensive operations in loops
- Use bit operations for flags and states
- Minimize string operations and use const strings where possible
- Efficient pattern matching algorithms

## 9. Integration Points

### 9.1 System Integration
- **Systemd Integration**: Maintain compatibility with existing service files
- **Configuration Files**: Parse existing shell-format configuration files
- **External Scripts**: Integration with `disk_threshold_check.sh`
- **File System**: Interaction with various mount points and file systems

### 9.2 Backward Compatibility
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

### 11.2 Integration Testing
- Test complete backup scenarios
- Verify compatibility with existing system
- Performance benchmarking against shell script
- Multi-platform validation

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

This HLD provides a comprehensive roadmap for migrating the backup_logs.sh script to a robust, efficient C implementation suitable for embedded RDK environments.
