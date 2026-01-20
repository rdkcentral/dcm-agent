# UploadLogsNow - High-Level Design Document

## Document Information
- **Version**: 1.0
- **Date**: January 20, 2026
- **Author**: RDK Management
- **Component**: DCM Agent - UploadLogsNow Module

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Component Design](#component-design)
4. [Workflow](#workflow)
5. [Data Structures](#data-structures)
6. [API Specifications](#api-specifications)
7. [Integration Points](#integration-points)
8. [Error Handling](#error-handling)
9. [Security Considerations](#security-considerations)
10. [Performance Considerations](#performance-considerations)

---

## 1. Overview

### 1.1 Purpose
The UploadLogsNow module provides on-demand log upload functionality as part of the DCM Agent's logupload binary. It implements the original UploadLogsNow.sh shell script functionality in C, providing better performance, reliability, and integration with the existing log upload infrastructure.

### 1.2 Scope
This module handles:
- On-demand triggering of log uploads via SNMP/TR69
- Log file collection and preparation
- Integration with existing archive creation and upload mechanisms
- Status tracking and reporting
- Temporary storage management

### 1.3 Goals
- **Compatibility**: Maintain functional equivalence with the original shell script
- **Integration**: Seamlessly integrate with existing logupload infrastructure
- **Reliability**: Provide robust error handling and status reporting
- **Performance**: Efficient file operations and resource management
- **Maintainability**: Clear, well-documented C implementation

---

## 2. Architecture

### 2.1 System Context

```
┌─────────────────────────────────────────────────────────┐
│                    External Triggers                    │
│              (SNMP/TR69/Management Interface)           │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│                   Logupload Binary                      │
│  ┌───────────────────────────────────────────────────┐  │
│  │         UploadLogsNow Module                      │  │
│  │  ┌──────────────────────────────────────────┐    │  │
│  │  │  Workflow Orchestration                  │    │  │
│  │  └──────────────────────────────────────────┘    │  │
│  │  ┌──────────────┐  ┌──────────────────────┐     │  │
│  │  │ File Copy    │  │ Status Management    │     │  │
│  │  └──────────────┘  └──────────────────────┘     │  │
│  └───────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────┐  │
│  │         Existing Infrastructure                   │  │
│  │  ┌──────────────┐  ┌──────────────────────┐     │  │
│  │  │ Archive Mgr  │  │ Upload Engine        │     │  │
│  │  └──────────────┘  └──────────────────────┘     │  │
│  │  ┌──────────────┐  ┌──────────────────────┐     │  │
│  │  │ File Ops     │  │ Strategy Handler     │     │  │
│  │  └──────────────┘  └──────────────────────┘     │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│              File System & Network                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Log Files    │  │ DCM Temp Dir │  │ Upload Server│  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 2.2 Module Architecture

```
┌─────────────────────────────────────────────────────────┐
│            execute_uploadlogsnow_workflow()             │
│                  (Main Entry Point)                     │
└───────────────────────┬─────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
        ↓               ↓               ↓
┌──────────────┐ ┌─────────────┐ ┌─────────────┐
│   Status     │ │    File     │ │  Archive &  │
│  Management  │ │   Copy      │ │   Upload    │
└──────────────┘ └─────────────┘ └─────────────┘
        │               │               │
        ↓               ↓               ↓
┌──────────────┐ ┌─────────────┐ ┌─────────────┐
│write_upload  │ │copy_files   │ │create_      │
│_status()     │ │_to_dcm_path │ │archive()    │
└──────────────┘ └─────────────┘ └─────────────┘
                        │               │
                        ↓               ↓
                ┌─────────────┐ ┌─────────────┐
                │should_      │ │execute_     │
                │exclude_file │ │upload_cycle │
                └─────────────┘ └─────────────┘
```

---

## 3. Component Design

### 3.1 Core Components

#### 3.1.1 Workflow Orchestrator
**Function**: `execute_uploadlogsnow_workflow(RuntimeContext* ctx)`

**Responsibilities**:
- Overall workflow coordination
- Initialization and cleanup
- Error propagation and handling
- Integration with existing infrastructure

**Key Operations**:
1. Initialize status tracking
2. Create temporary DCM directory
3. Copy log files with filtering
4. Add timestamps to log files
5. Create archive using archive_manager
6. Execute upload using upload_engine
7. Clean up temporary resources

#### 3.1.2 Status Manager
**Function**: `write_upload_status(const char* message)`

**Responsibilities**:
- Track upload workflow state
- Provide visibility to external monitoring
- Maintain status file integrity

**Status States**:
- **Triggered**: Upload request received
- **In progress**: Archive creation and upload in progress
- **Complete**: Upload completed successfully
- **Failed**: Error occurred during workflow

**Status File**: `/opt/loguploadstatus.txt`

#### 3.1.3 File Copy Manager
**Function**: `copy_files_to_dcm_path(const char* src_path, const char* dest_path)`

**Responsibilities**:
- Copy all log files from source to temporary directory
- Filter excluded files/directories
- Handle errors and report statistics
- Prevent path traversal and buffer overflows

**Helper Function**: `should_exclude_file(const char* filename)`

**Exclusion List**:
- `dcm` - DCM agent directory
- `PreviousLogs_backup` - Previous backup logs
- `PreviousLogs` - Previous logs directory

### 3.2 Integration Components

#### 3.2.1 Archive Manager Integration
**Module**: `archive_manager.h`

**Functions Used**:
- `create_archive()` - Creates tar/tar.gz archive of log files
- `add_timestamp_to_files_uploadlogsnow()` - Adds timestamps with UploadLogsNow-specific exclusions

**Integration Pattern**:
```c
SessionState session = {0};
session.strategy = STRAT_ONDEMAND;
create_archive(ctx, &session, dcm_log_path);
```

#### 3.2.2 Upload Engine Integration
**Module**: `upload_engine.h`

**Functions Used**:
- `decide_paths()` - Determines upload destination paths
- `execute_upload_cycle()` - Performs actual upload operation

**Integration Pattern**:
```c
decide_paths(ctx, &session);
execute_upload_cycle(ctx, &session);
```

#### 3.2.3 File Operations Integration
**Module**: `file_operations.h`

**Functions Used**:
- `create_directory()` - Creates directories with proper permissions
- `copy_file()` - Copies individual files
- `remove_directory()` - Recursive directory removal
- `file_exists()` - Checks file/directory existence

---

## 4. Workflow

### 4.1 Overall Workflow Diagram

```
┌─────────────────────────────────────────────────────────┐
│                     START                               │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 1. Write Status: "Triggered"                           │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 2. Initialize DCM_LOG_PATH                             │
│    - Use context path or default to /tmp/DCM           │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 3. Create DCM_LOG_PATH Directory                       │
│    - Handle creation errors                            │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 4. Copy Log Files                                      │
│    - Read source directory                             │
│    - Filter excluded files                             │
│    - Copy each file/directory                          │
│    - Track copy count                                  │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 5. Add Timestamps to Files                             │
│    - Use UploadLogsNow-specific exclusions             │
│    - Non-critical operation (continue on failure)      │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 6. Write Status: "In progress"                         │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 7. Create Archive                                      │
│    - Initialize session with STRAT_ONDEMAND            │
│    - Call create_archive()                             │
│    - Verify archive exists                             │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 8. Upload Archive                                      │
│    - Call decide_paths()                               │
│    - Call execute_upload_cycle()                       │
└───────────────────────┬─────────────────────────────────┘
                        │
                ┌───────┴───────┐
                │               │
         Success│               │Failure
                ↓               ↓
    ┌─────────────────┐ ┌─────────────────┐
    │ Status:         │ │ Status:         │
    │ "Complete"      │ │ "Failed"        │
    └────────┬────────┘ └────────┬────────┘
             │                   │
             └─────────┬─────────┘
                       ↓
    ┌─────────────────────────────────────┐
    │ 9. Cleanup DCM_LOG_PATH             │
    │    - Remove temporary directory     │
    └─────────────────┬───────────────────┘
                      │
                      ↓
    ┌─────────────────────────────────────┐
    │              END                    │
    └─────────────────────────────────────┘
```

### 4.2 File Copy Workflow

```
┌────────────────────────────────────────┐
│ Open Source Directory                  │
└──────────────┬─────────────────────────┘
               │
               ↓
┌────────────────────────────────────────┐
│ Read Directory Entry                   │
└──────────────┬─────────────────────────┘
               │
        ┌──────┴──────┐
        │             │
     Yes│             │No More
        │             │Entries
        ↓             │
┌────────────────┐    │
│ Is . or .. ?   │    │
└────┬───────────┘    │
     │ No             │
     ↓                │
┌────────────────┐    │
│ Excluded file? │    │
└────┬───────────┘    │
     │ No             │
     ↓                │
┌────────────────┐    │
│ Check Path Len │    │
└────┬───────────┘    │
     │ OK             │
     ↓                │
┌────────────────┐    │
│ Copy File      │    │
└────┬───────────┘    │
     │                │
     └────────────────┤
                      ↓
        ┌─────────────────────────┐
        │ Return Copied Count     │
        └─────────────────────────┘
```

### 4.3 Error Handling Flow

```
┌─────────────────────────────────────────┐
│            Error Detected               │
└───────────────┬─────────────────────────┘
                │
        ┌───────┴────────┬──────────────┐
        │                │              │
        ↓                ↓              ↓
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ Critical     │ │ Non-Critical │ │ Warning      │
│ (Fatal)      │ │ (Continue)   │ │ (Log Only)   │
└──────┬───────┘ └──────┬───────┘ └──────┬───────┘
       │                │              │
       ↓                ↓              ↓
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ Log Error    │ │ Log Warning  │ │ Log Debug    │
└──────┬───────┘ └──────┬───────┘ └──────────────┘
       │                │
       ↓                ↓
┌──────────────┐ ┌──────────────┐
│ Write Status │ │ Continue     │
│ "Failed"     │ │ Workflow     │
└──────┬───────┘ └──────────────┘
       │
       ↓
┌──────────────┐
│ Cleanup      │
└──────┬───────┘
       │
       ↓
┌──────────────┐
│ Return Error │
└──────────────┘
```

---

## 5. Data Structures

### 5.1 RuntimeContext
```c
typedef struct {
    char log_path[MAX_PATH_LENGTH];        // Source log directory
    char dcm_log_path[MAX_PATH_LENGTH];    // Temporary DCM directory
    // ... other context fields from uploadstblogs_types.h
} RuntimeContext;
```

**Usage**: Passed through entire workflow to maintain state and configuration.

### 5.2 SessionState
```c
typedef struct {
    UploadStrategy strategy;                // STRAT_ONDEMAND for UploadLogsNow
    char archive_file[MAX_PATH_LENGTH];     // Archive file path
    // ... other session fields from uploadstblogs_types.h
} SessionState;
```

**Usage**: Tracks archive creation and upload session state.

### 5.3 File Path Handling
- **MAX_PATH_LENGTH**: Maximum path length (typically 1024 or 4096)
- **Path Construction**: Uses `snprintf()` with bounds checking
- **Path Validation**: Checks for truncation and buffer overflows

---

## 6. API Specifications

### 6.1 Public Interface

#### 6.1.1 execute_uploadlogsnow_workflow()
```c
int execute_uploadlogsnow_workflow(RuntimeContext* ctx)
```

**Parameters**:
- `ctx`: Runtime context containing configuration and paths

**Returns**:
- `0`: Success
- `-1`: Failure

**Preconditions**:
- RuntimeContext must be initialized
- log_path must be valid and readable
- Sufficient disk space available

**Postconditions**:
- Status file updated
- Temporary directory cleaned up
- Logs uploaded or error logged

**Thread Safety**: Not thread-safe (single instance expected)

### 6.2 Internal Functions

#### 6.2.1 write_upload_status()
```c
static int write_upload_status(const char* message)
```

**Parameters**:
- `message`: Status message to write

**Returns**:
- `0`: Success
- `-1`: Failure

#### 6.2.2 should_exclude_file()
```c
static int should_exclude_file(const char* filename)
```

**Parameters**:
- `filename`: Name of file to check

**Returns**:
- `1`: File should be excluded
- `0`: File should be included

#### 6.2.3 copy_files_to_dcm_path()
```c
static int copy_files_to_dcm_path(const char* src_path, const char* dest_path)
```

**Parameters**:
- `src_path`: Source directory path
- `dest_path`: Destination directory path

**Returns**:
- `>= 0`: Number of files copied
- `-1`: Failure

---

## 7. Integration Points

### 7.1 Dependencies

```
UploadLogsNow Module
    ├── uploadstblogs_types.h    (Data structures)
    ├── strategy_handler.h        (Strategy definitions)
    ├── archive_manager.h         (Archive creation)
    ├── file_operations.h         (File utilities)
    ├── upload_engine.h           (Upload execution)
    └── rdk_debug.h              (Logging)
```

### 7.2 External Interfaces

#### 7.2.1 RDK Logger
**Interface**: `RDK_LOG(level, module, format, ...)`

**Log Levels Used**:
- `RDK_LOG_ERROR`: Critical errors requiring attention
- `RDK_LOG_WARN`: Non-critical warnings
- `RDK_LOG_INFO`: Workflow milestones and progress
- `RDK_LOG_DEBUG`: Detailed debugging information

**Log Module**: `LOG.RDK.UPLOADSTB`

#### 7.2.2 Status File
**Path**: `/opt/loguploadstatus.txt`

**Format**: `<status> <timestamp>`

**Access**: External monitoring systems read this file

#### 7.2.3 File System Locations
- **Source Logs**: `ctx->log_path` (typically `/opt/logs`)
- **Temporary Dir**: `ctx->dcm_log_path` or `/tmp/DCM`
- **Archive Output**: Within temporary directory

---

## 8. Error Handling

### 8.1 Error Categories

#### 8.1.1 Fatal Errors
Errors that prevent workflow completion:
- Cannot create DCM_LOG_PATH
- Cannot open source directory
- Archive creation failure
- Upload failure

**Handling**: Log error, write "Failed" status, cleanup, return error

#### 8.1.2 Non-Fatal Errors
Errors that allow workflow to continue:
- Individual file copy failure
- Timestamp addition failure
- Status file write failure (logged only)

**Handling**: Log warning, continue workflow

#### 8.1.3 Warnings
Non-error conditions requiring attention:
- Path length approaching limit
- Excluded file encountered
- Cleanup failure

**Handling**: Log debug/warning, continue

### 8.2 Error Recovery

```
┌────────────────────────────────────────┐
│          Error Detected                │
└──────────────┬─────────────────────────┘
               │
               ↓
┌────────────────────────────────────────┐
│      Log Error with Context            │
│  - Function name                       │
│  - Line number                         │
│  - Error details                       │
└──────────────┬─────────────────────────┘
               │
               ↓
┌────────────────────────────────────────┐
│    Update Status (if critical)         │
└──────────────┬─────────────────────────┘
               │
               ↓
┌────────────────────────────────────────┐
│         Cleanup Resources              │
│  - Close file handles                  │
│  - Remove temporary directory          │
└──────────────┬─────────────────────────┘
               │
               ↓
┌────────────────────────────────────────┐
│         Return Error Code              │
└────────────────────────────────────────┘
```

### 8.3 Logging Strategy

**Format**: `[function:line] Message`

**Example**:
```
RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
        "[%s:%d] Failed to create DCM_LOG_PATH: %s\n", 
        __FUNCTION__, __LINE__, dcm_log_path);
```

---

## 9. Security Considerations

### 9.1 Path Traversal Prevention
- All paths constructed using `snprintf()` with bounds checking
- Path length validation before use
- No user-supplied path components without validation

### 9.2 Buffer Overflow Protection
- Fixed-size buffers with `MAX_PATH_LENGTH`
- `snprintf()` used instead of `sprintf()`
- Truncation detection and error handling
- Pre-construction length checks

### 9.3 File System Security
- Temporary directory cleanup ensures no residual data
- Exclusion list prevents copying sensitive directories
- File permissions inherited from existing infrastructure

### 9.4 Privilege Management
- Runs with DCM agent privileges
- No privilege escalation
- Status file in protected location

---

## 10. Performance Considerations

### 10.1 File Operations
- Single-pass directory traversal
- Direct file copying (no intermediate buffers)
- Efficient exclusion checking (constant-time array lookup)
- Lazy status file updates (only on state changes)

### 10.2 Memory Management
- Fixed-size stack allocations (no dynamic memory)
- Path buffers reused across loop iterations
- Minimal heap usage through existing infrastructure

### 10.3 I/O Optimization
- Sequential file access patterns
- Directory handle properly closed
- Temporary file cleanup prevents disk space exhaustion

### 10.4 Scalability
**File Count**: Handles arbitrary number of log files efficiently

**File Size**: Archive creation handles large files through streaming

**Directory Depth**: Single-level copy (no deep recursion)

---

## 11. Testing Considerations

### 11.1 Unit Testing
- Status file write/read operations
- Exclusion list checking
- Path construction and validation
- Error handling paths

### 11.2 Integration Testing
- End-to-end workflow execution
- Archive creation integration
- Upload engine integration
- Cleanup verification

### 11.3 Error Injection Testing
- Directory creation failure
- File copy failure
- Archive creation failure
- Upload failure
- Disk space exhaustion

### 11.4 Performance Testing
- Large log file handling
- Many small files scenario
- Concurrent execution (if applicable)

---

## 12. Future Enhancements

### 12.1 Potential Improvements
- Parallel file copying for performance
- Incremental log collection
- Compression before upload
- Progress reporting (percentage)
- Retry mechanism for transient failures

### 12.2 Extensibility Points
- Pluggable exclusion list (configuration file)
- Custom timestamp formats
- Alternative upload strategies
- Post-upload callbacks

---

## 13. References

### 13.1 Related Documents
- UploadLogsNow_Migration_HLD.md - Migration strategy
- UploadLogsNow_C_Implementation.md - Implementation details

### 13.2 Code Modules
- uploadlogsnow.c - Main implementation
- uploadlogsnow.h - Public interface
- archive_manager.h - Archive creation
- upload_engine.h - Upload execution
- file_operations.h - File utilities

### 13.3 Standards
- RDK Coding Standards
- C99 Standard
- POSIX File System API

---

## 14. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | January 20, 2026 | RDK Management | Initial HLD document |

---

## 15. Appendix

### 15.1 Constants and Macros
```c
#define LOG_UPLOADSTB "LOG.RDK.UPLOADSTB"
#define STATUS_FILE "/opt/loguploadstatus.txt"
#define DCM_TEMP_DIR "/tmp/DCM"
```

### 15.2 Status File Format
```
<Status Message> <Day> <Month> <Date> <Time> <Year>
```

**Example**:
```
Triggered Mon Jan 20 14:30:00 2026
In progress Mon Jan 20 14:30:15 2026
Complete Mon Jan 20 14:31:00 2026
```

### 15.3 Exclusion List Configuration
Current hardcoded exclusions:
- `dcm` - DCM agent directory
- `PreviousLogs_backup` - Previous log backups
- `PreviousLogs` - Previous logs directory

Future: Move to configuration file for flexibility.

---

**End of Document**
