# Functional Requirements: backup_logs.sh Migration

## 1. Overview

This document outlines the detailed functional requirements for migrating the `backup_logs.sh` shell script to a C implementation for embedded RDK systems.

## 2. Functional Requirements

### 2.1 Configuration Management (REQ-001)
**Description**: The system must load and parse configuration from multiple sources
**Requirements**:
- Parse `/etc/include.properties` for log path configuration
- Parse `/etc/device.properties` for device-specific settings  
- Parse `/etc/env_setup.sh` if available for environment variables
- Extract `LOG_PATH`, `HDD_ENABLED`, and `APP_PERSISTENT_PATH` variables
- Validate all configuration parameters before proceeding
- Handle missing or malformed configuration files gracefully

**Input**: Configuration files in shell variable format
**Output**: Structured configuration data
**Error Handling**: Log configuration errors and exit with appropriate error code

### 2.2 Directory Management (REQ-002)
**Description**: Create and manage required log directory structures
**Requirements**:
- Create `$LOG_PATH` directory if it doesn't exist
- Create `$LOG_PATH/PreviousLogs` directory structure
- Create `$LOG_PATH/PreviousLogs_backup` directory structure  
- Clean existing backup directory contents before use
- Set appropriate permissions on created directories
- Handle directory creation failures gracefully

**Input**: Configuration paths
**Output**: Created directory structures
**Constraints**: Must work with various filesystem types and permissions

### 2.3 Disk Threshold Monitoring (REQ-003)
**Description**: Monitor disk usage and trigger cleanup when necessary
**Requirements**:
- Execute disk threshold check if `/lib/rdk/disk_threshold_check.sh` exists
- Pass parameter `0` to the disk check script
- Handle script execution failures without stopping backup process
- Log disk check results for monitoring

**Input**: Disk check script path
**Output**: Disk status information
**Dependencies**: External `disk_threshold_check.sh` script

### 2.4 HDD-Disabled Device Backup Strategy (REQ-004)
**Description**: Implement 4-level log rotation for devices without HDD
**Requirements**:
- Support up to 4 backup levels: base, bak1_, bak2_, bak3_
- Move files based on existing backup level:
  - Level 0: Move current logs to PreviousLogs
  - Level 1: Move current logs with `bak1_` prefix
  - Level 2: Move current logs with `bak2_` prefix  
  - Level 3: Rotate all levels (bak1→base, bak2→bak1, bak3→bak2, current→bak3)
- Create `last_reboot` marker file after each backup
- Clean current log directory after backup completion

**Input**: Current log files and existing backup state
**Output**: Rotated backup files with appropriate naming
**File Patterns**: `*.txt*`, `*.log*`, `*.bin*`, `bootlog` files

### 2.5 HDD-Enabled Device Backup Strategy (REQ-005)
**Description**: Implement timestamped backup for devices with HDD
**Requirements**:
- Check for existing `messages.txt` in PreviousLogs directory
- If no existing backup: Move all logs to PreviousLogs directory
- If backup exists: Create timestamped backup directory (`logbackup-MM-DD-YY-HH-MM-SSAM`)
- Move current logs to timestamped directory
- Remove any existing `last_reboot` markers before creating new one
- Create `last_reboot` marker in appropriate location

**Input**: Current log files and existing backup state  
**Output**: Timestamped backup directories with organized log files
**File Patterns**: `*.txt*`, `*.log*`, `bootlog` files (no `.bin*` files)

### 2.6 File Operations (REQ-006)
**Description**: Perform reliable file and directory operations
**Requirements**:
- Move files with error handling and validation
- Support pattern-based file finding (find with depth and type constraints)
- Handle both regular files and symbolic links
- Implement atomic file operations where possible
- Validate file operations and report failures
- Support large numbers of files efficiently

**Input**: Source and destination paths, file patterns
**Output**: Moved/copied files with status reporting
**Constraints**: Must handle filesystem limitations and permissions

### 2.7 Version File Management (REQ-007)
**Description**: Copy system version information to log directory
**Requirements**:
- Copy `/version.txt` to current log directory
- Copy `/etc/skyversion.txt` to current log directory as `skyversion.txt`  
- Copy `/etc/rippleversion.txt` to current log directory as `rippleversion.txt`
- Handle missing version files gracefully (non-fatal errors)
- Preserve file timestamps and permissions where possible

**Input**: System version files
**Output**: Version files in log directory
**Error Handling**: Log warnings for missing files but continue execution

### 2.8 Special Log File Handling (REQ-008)
**Description**: Handle temporary and special log files
**Requirements**:
- Move `/tmp/disk_cleanup.log` to current log directory if exists
- Move `/tmp/mount_log.txt` to current log directory if exists  
- Move `/tmp/mount-ta_log.txt` to current log directory if exists
- Handle file moves atomically to prevent data loss
- Continue execution if special files are missing

**Input**: Temporary log files from `/tmp` directory
**Output**: Special log files moved to permanent location
**Timing**: Execute after main backup but before cleanup

### 2.9 System Integration (REQ-009)  
**Description**: Integrate with systemd and system services
**Requirements**:
- Send systemd ready notification upon completion
- Set systemd status message: "Logs Backup Done..!"
- Create persistent marker file at `$PERSISTENT_PATH/logFileBackup`
- Handle systemd notification failures gracefully
- Support operation in non-systemd environments

**Input**: Completion status
**Output**: System notifications and marker files
**Dependencies**: systemd-notify command availability

### 2.10 Logging and Monitoring (REQ-010)
**Description**: Provide comprehensive logging and error reporting
**Requirements**:
- Log all major operations with timestamps
- Use format: `timestamp scriptname: message`
- Support different log levels (info, warning, error, debug)
- Log to stdout/stderr for systemd journal integration
- Include function name and line numbers in error logs
- Support structured logging for automated processing

**Input**: Operation status and error conditions
**Output**: Formatted log messages with timestamps
**Format**: Compatible with existing RDK logging standards

## 3. Non-Functional Requirements

### 3.1 Performance Requirements (NFR-001)
- Memory usage must be ≤ 512KB peak during operation
- Startup time must be ≤ 2 seconds on target hardware
- File operations must complete within 30 seconds for typical log volumes
- CPU usage should not exceed 10% during backup operations

### 3.2 Reliability Requirements (NFR-002)  
- System must handle unexpected shutdowns gracefully
- Backup operations must be atomic (complete or rollback)
- Must recover from partial backup states on restart
- Handle filesystem full conditions without data loss

### 3.3 Portability Requirements (NFR-003)
- Support ARM, MIPS, and x86 architectures
- Compatible with various embedded Linux distributions
- Work with different filesystem types (ext4, JFFS2, UBIFS)
- Support cross-compilation toolchains

### 3.4 Security Requirements (NFR-004)
- Validate all file paths to prevent directory traversal
- Handle file permissions correctly without privilege escalation  
- Sanitize all inputs from configuration files
- Protect against symlink attacks during file operations

## 4. Input/Output Specifications

### 4.1 Inputs
- **Configuration Files**: `/etc/include.properties`, `/etc/device.properties`, `/etc/env_setup.sh`
- **Log Files**: Files matching patterns `*.txt*`, `*.log*`, `*.bin*`, `bootlog`
- **Version Files**: `/version.txt`, `/etc/skyversion.txt`, `/etc/rippleversion.txt`
- **Temporary Files**: `/tmp/disk_cleanup.log`, `/tmp/mount_log.txt`, `/tmp/mount-ta_log.txt`

### 4.2 Outputs  
- **Backup Directories**: Organized log file backups with appropriate naming
- **Marker Files**: `last_reboot` markers for tracking backup cycles
- **System Notifications**: systemd ready notifications and status messages
- **Log Messages**: Timestamped operation logs for monitoring

### 4.3 Error Codes
- **0**: Success - All operations completed successfully
- **1**: Configuration Error - Invalid or missing configuration  
- **2**: Filesystem Error - Directory creation or file operation failure
- **3**: Permission Error - Insufficient permissions for required operations
- **4**: Resource Error - Insufficient disk space or memory
- **5**: System Error - External script or system call failure

## 5. Dependencies

### 5.1 System Dependencies
- POSIX-compliant filesystem
- Standard C library (libc)  
- systemd-notify utility (optional)
- `/bin/timestamp` utility for log formatting
- Access to `/proc` filesystem for system information

### 5.2 External Scripts
- `/lib/rdk/disk_threshold_check.sh` - Disk usage monitoring
- Configuration parsing utilities for shell variable format

### 5.3 File System Requirements
- Write access to log directories
- Sufficient disk space for log rotation (minimum 2x current log size)
- Support for atomic file operations (rename)

## 6. Constraints

### 6.1 Timing Constraints
- Must complete within systemd service timeout (typically 90 seconds)
- Backup rotation should complete within 10 seconds for typical volumes
- Configuration loading must complete within 1 second

### 6.2 Memory Constraints  
- Peak memory usage limited to 512KB on embedded systems
- No dynamic memory allocation for file lists exceeding 100MB
- Stack usage limited to 64KB maximum depth

### 6.3 Storage Constraints
- Must work with log directories up to 1GB in size
- Support up to 10,000 individual log files  
- Handle filenames up to 255 characters (filesystem limit)

## 7. Edge Cases and Error Scenarios

### 7.1 Configuration Edge Cases
- Missing configuration files
- Malformed shell variable syntax
- Invalid path specifications  
- Conflicting configuration values
- Unicode characters in paths

### 7.2 Filesystem Edge Cases
- Disk full during backup operations
- Permission changes during execution
- Network filesystem disconnections
- Corrupted filesystem states
- Very large individual log files (>100MB)

### 7.3 System Edge Cases  
- System shutdown during backup
- Multiple backup processes running simultaneously
- Clock adjustments affecting timestamps
- Filesystem readonly states
- Missing system utilities

## 8. Acceptance Criteria

### 8.1 Functional Acceptance
- [ ] All backup strategies work correctly for both HDD configurations
- [ ] Log rotation maintains proper sequence and naming
- [ ] Version file copying works without data loss
- [ ] System integrations (systemd) function properly
- [ ] Error handling provides useful diagnostic information

### 8.2 Performance Acceptance
- [ ] Memory usage stays within embedded system constraints
- [ ] Startup and completion times meet target requirements  
- [ ] File operations scale appropriately with log volume
- [ ] CPU usage remains reasonable during operation

### 8.3 Reliability Acceptance
- [ ] Operations complete successfully in normal conditions
- [ ] System handles error conditions gracefully
- [ ] Recovery from partial states works correctly
- [ ] No data loss occurs during operations
- [ ] Cross-platform compatibility verified

This requirements document provides the foundation for implementing a robust, efficient C replacement for the backup_logs.sh script that meets the needs of embedded RDK systems.
