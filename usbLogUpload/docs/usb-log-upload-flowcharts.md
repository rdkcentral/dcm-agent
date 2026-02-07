# USB Log Upload - Flowcharts and Diagrams

## Overview
This document contains detailed flowcharts and sequence diagrams for the USB Log Upload functionality migration from shell script to C code.

## 1. Main Processing Flowchart

### 1.1 Complete Process Flow (Mermaid)

```mermaid
flowchart TD
    Start([Start USB Log Upload]) --> ParseArgs[Parse Command Line Arguments]
    ParseArgs --> ValidateArgs{Arguments Valid?}
    
    ValidateArgs -->|No| Usage[Print Usage Message]
    Usage --> Exit4[Exit Code 4: Invalid Usage]
    
    ValidateArgs -->|Yes| LoadConfig[Load System Configuration]
    LoadConfig --> ConfigOK{Configuration Loaded?}
    ConfigOK -->|No| Exit6[Exit Code 6: Config Error]
    
    ConfigOK -->|Yes| DeviceCheck[Check Device Compatibility]
    DeviceCheck --> DeviceOK{Device == PLATCO?}
    DeviceOK -->|No| Exit4_Device[Exit Code 4: Unsupported Device]
    
    DeviceOK -->|Yes| USBCheck[Validate USB Mount Point]
    USBCheck --> USBMounted{USB Drive Mounted?}
    USBMounted -->|No| Exit2[Exit Code 2: USB Not Available]
    
    USBMounted -->|Yes| CreateLogDir[Create USB Log Directory]
    CreateLogDir --> LogDirOK{Directory Created?}
    LogDirOK -->|No| Exit3_Dir[Exit Code 3: Write Error]
    
    LogDirOK -->|Yes| GenFilename[Generate Archive Filename]
    GenFilename --> CreateTempDir[Create Temporary Directory]
    CreateTempDir --> TempDirOK{Temp Directory Created?}
    TempDirOK -->|No| Exit3_Temp[Exit Code 3: Write Error]
    
    TempDirOK -->|Yes| MoveFiles[Move Log Files to Temp]
    MoveFiles --> FilesMovedOK{Files Moved Successfully?}
    FilesMovedOK -->|No| CleanupFail[Cleanup and Exit 3]
    
    FilesMovedOK -->|Yes| CheckSyslog{Syslog-ng Enabled?}
    CheckSyslog -->|Yes| ReloadSyslog[Send SIGHUP to syslog-ng]
    CheckSyslog -->|No| CreateArchive[Create Archive from Temp Files]
    ReloadSyslog --> SyslogOK{Reload Successful?}
    SyslogOK -->|Yes| CreateArchive
    SyslogOK -->|No| LogWarning[Log Warning] --> CreateArchive
    
    CreateArchive --> ArchiveOK{Archive Created?}
    ArchiveOK -->|No| CleanupArchiveFail[Cleanup and Exit 3]
    
    ArchiveOK -->|Yes| CleanupTemp[Remove Temporary Directory]
    CleanupTemp --> SyncFS[Sync Filesystem]
    SyncFS --> Success[Log Success Message]
    Success --> Exit0[Exit Code 0: Success]
    
    CleanupFail --> Exit3[Exit Code 3: Write Error]
    CleanupArchiveFail --> Exit3
```

### 1.2 Simplified Text-Based Flowchart

```
START
  ↓
Parse Arguments
  ↓
Valid? ──NO──→ Print Usage → EXIT(4)
  ↓ YES
Load Configuration  
  ↓
Config OK? ──NO──→ EXIT(6)
  ↓ YES
Check Device Type
  ↓
PLATCO Device? ──NO──→ EXIT(4)
  ↓ YES  
Validate USB Mount
  ↓
USB Mounted? ──NO──→ EXIT(2)
  ↓ YES
Create USB Log Directory
  ↓
Directory OK? ──NO──→ EXIT(3)
  ↓ YES
Generate Archive Name
  ↓
Create Temp Directory
  ↓
Temp Dir OK? ──NO──→ EXIT(3)
  ↓ YES
Move Log Files
  ↓
Files Moved? ──NO──→ Cleanup → EXIT(3)
  ↓ YES
Syslog Enabled? ──YES──→ Reload Syslog
  ↓ NO                    ↓
Create Archive ←─────────── 
  ↓
Archive OK? ──NO──→ Cleanup → EXIT(3)
  ↓ YES
Cleanup Temp Files
  ↓
Sync Filesystem
  ↓
EXIT(0)
```

## 2. Module Interaction Flowcharts

### 2.1 Validation Module Flow

```mermaid
flowchart TD
    ValidateStart([Validation Start]) --> CheckDevice[Check Device Name]
    CheckDevice --> DeviceMatch{Device == PLATCO?}
    DeviceMatch -->|No| DeviceFail[Return Device Error]
    DeviceMatch -->|Yes| CheckUSB[Validate USB Mount Point]
    
    CheckUSB --> USBExists{USB Path Exists?}
    USBExists -->|No| USBFail[Return USB Error]
    USBExists -->|Yes| CheckPerm[Check Write Permissions]
    
    CheckPerm --> PermOK{Write Access?}
    PermOK -->|No| PermFail[Return Permission Error]
    PermOK -->|Yes| CheckSpace[Check Available Space]
    
    CheckSpace --> SpaceOK{Sufficient Space?}
    SpaceOK -->|No| SpaceFail[Return Space Error]
    SpaceOK -->|Yes| ValidateOK[Return Success]
```

### 2.2 File Operations Flow

```mermaid
flowchart TD
    FileOpStart([File Operations Start]) --> CreateLogDir[Create USB Log Directory]
    CreateLogDir --> LogDirSuccess{Directory Created?}
    LogDirSuccess -->|No| LogDirFail[Return Create Error]
    
    LogDirSuccess -->|Yes| CreateTempDir[Create Temporary Directory]
    CreateTempDir --> TempDirSuccess{Temp Dir Created?}
    TempDirSuccess -->|No| TempDirFail[Return Temp Error]
    
    TempDirSuccess -->|Yes| ScanLogFiles[Scan Source Log Directory]
    ScanLogFiles --> FilesFound{Log Files Found?}
    FilesFound -->|No| NoFilesFail[Return No Files Error]
    
    FilesFound -->|Yes| MoveLoop[Move Files Loop]
    MoveLoop --> NextFile{More Files?}
    NextFile -->|No| MoveComplete[All Files Moved]
    NextFile -->|Yes| MoveFile[Move Single File]
    
    MoveFile --> MoveSuccess{Move OK?}
    MoveSuccess -->|No| MoveFail[Return Move Error]
    MoveSuccess -->|Yes| MoveLoop
    
    MoveComplete --> FileOpsSuccess[Return Success]
```

## 3. Sequence Diagrams

### 3.1 Main Process Sequence

```mermaid
sequenceDiagram
    participant CLI as Command Line
    participant Main as Main Process
    participant Config as Config Module
    participant Valid as Validation Module
    participant FileMgr as File Manager
    participant ArchMgr as Archive Manager
    participant SysMgr as System Manager
    
    CLI->>Main: usb_mount_point
    Main->>Config: load_system_configuration()
    Config-->>Main: config_data
    
    Main->>Valid: validate_device_compatibility()
    Valid-->>Main: validation_result
    
    Main->>Valid: validate_usb_mount_point(usb_path)
    Valid-->>Main: usb_validation_result
    
    Main->>FileMgr: create_usb_log_directory(usb_path)
    FileMgr-->>Main: directory_status
    
    Main->>FileMgr: generate_archive_filename()
    FileMgr-->>Main: archive_filename
    
    Main->>FileMgr: move_log_files(source, temp)
    FileMgr-->>Main: move_status
    
    Main->>SysMgr: reload_syslog_service()
    SysMgr-->>Main: reload_status
    
    Main->>ArchMgr: create_log_archive(temp, usb_archive)
    ArchMgr-->>Main: archive_status
    
    Main->>FileMgr: cleanup_temporary_files(temp)
    FileMgr-->>Main: cleanup_status
    
    Main->>SysMgr: sync_filesystem()
    SysMgr-->>Main: sync_status
    
    Main-->>CLI: exit_code
```

### 3.2 File Management Sequence

```mermaid
sequenceDiagram
    participant FM as File Manager
    participant FS as File System
    participant Logger as Logger
    
    Note over FM: Directory Creation Phase
    FM->>FS: mkdir(usb_log_path)
    FS-->>FM: creation_result
    alt directory creation failed
        FM->>Logger: log_error("Failed to create USB log directory")
        FM-->>FM: return error
    end
    
    Note over FM: File Movement Phase
    FM->>FS: opendir(source_path)
    FS-->>FM: directory_handle
    
    loop for each log file
        FM->>FS: readdir()
        FS-->>FM: file_entry
        FM->>FS: copy_file(source, destination)
        FS-->>FM: copy_result
        alt copy successful
            FM->>FS: unlink(source_file)
            FS-->>FM: delete_result
        else copy failed
            FM->>Logger: log_error("File copy failed")
            FM-->>FM: return error
        end
    end
    
    FM->>FS: closedir(directory_handle)
    FS-->>FM: close_result
```

### 3.3 Error Handling Sequence

```mermaid
sequenceDiagram
    participant Module as Any Module
    participant ErrHandler as Error Handler
    participant Logger as Logger
    participant Main as Main Process
    
    Module->>ErrHandler: report_error(error_code, context)
    ErrHandler->>Logger: log_error(formatted_message)
    
    alt fatal error
        ErrHandler->>ErrHandler: cleanup_resources()
        ErrHandler->>Main: signal_fatal_error(error_code)
        Main->>Main: exit(error_code)
    else recoverable error
        ErrHandler->>Logger: log_warning(error_message)
        ErrHandler-->>Module: error_handled
        Module->>Module: continue_operation()
    end
```

## 4. Component Interaction Diagrams

### 4.1 System Service Interaction

```mermaid
flowchart LR
    subgraph "USB Log Upload Process"
        Main[Main Process]
        SysMgr[System Manager]
    end
    
    subgraph "System Services"
        SyslogNG[syslog-ng]
        FileSystem[File System]
        USBDriver[USB Driver]
    end
    
    Main --> SysMgr
    SysMgr -->|SIGHUP| SyslogNG
    SysMgr -->|sync| FileSystem
    SysMgr -->|mount check| USBDriver
    
    SyslogNG -->|log rotation| FileSystem
    FileSystem -->|USB I/O| USBDriver
```

### 4.2 Configuration Flow Diagram

```mermaid
flowchart TD
    ConfigStart([Configuration Loading]) --> ReadInclude[Read /etc/include.properties]
    ReadInclude --> ReadDevice[Read /etc/device.properties]
    ReadDevice --> ReadEnvVars[Read Environment Variables]
    
    ReadEnvVars --> CheckDeviceName{DEVICE_NAME Set?}
    CheckDeviceName -->|No| SetDefault1[Set Default Device]
    CheckDeviceName -->|Yes| CheckRDKPath{RDK_PATH Set?}
    SetDefault1 --> CheckRDKPath
    
    CheckRDKPath -->|No| SetDefault2[Set Default RDK Path]
    CheckRDKPath -->|Yes| CheckLogPath{LOG_PATH Set?}
    SetDefault2 --> CheckLogPath
    
    CheckLogPath -->|No| SetDefault3[Set Default Log Path]
    CheckLogPath -->|Yes| ValidateConfig[Validate Configuration]
    SetDefault3 --> ValidateConfig
    
    ValidateConfig --> ConfigValid{All Required Set?}
    ConfigValid -->|No| ConfigFail[Return Config Error]
    ConfigValid -->|Yes| ConfigSuccess[Return Config Success]
```

## 5. Text-Based Alternative Diagrams

### 5.1 Module Interaction (Text Format)

```
Main Process
    ├── Configuration Module
    │   ├── Reads: /etc/include.properties
    │   ├── Reads: /etc/device.properties
    │   └── Exports: system_config
    │
    ├── Validation Module
    │   ├── Uses: system_config
    │   ├── Checks: device compatibility
    │   └── Validates: USB mount point
    │
    ├── File Manager Module
    │   ├── Creates: USB directories
    │   ├── Moves: log files
    │   └── Manages: temporary storage
    │
    ├── Archive Manager Module
    │   ├── Generates: archive filenames
    │   ├── Compresses: log files
    │   └── Creates: .tgz archives
    │
    └── System Manager Module
        ├── Controls: syslog-ng service
        ├── Executes: system commands
        └── Manages: filesystem sync
```

### 5.2 Data Flow (Text Format)

```
Input: USB Mount Point
    ↓
[Validation] → Device Check → USB Check
    ↓
[File Operations] → Create Directories → Move Files
    ↓
[Service Management] → Reload syslog-ng
    ↓
[Archival] → Generate Name → Compress Files
    ↓
[Cleanup] → Remove Temp → Sync FS
    ↓
Output: Archive on USB + Exit Code
```

### 5.3 Error Propagation (Text Format)

```
Module Error → Error Handler → Log Error → Decision Point
                                              ├── Fatal: Exit Process
                                              └── Recoverable: Continue
```

## 6. Implementation Flow Diagrams

### 6.1 Memory Management Flow

```mermaid
flowchart TD
    MemStart([Memory Management Start]) --> StaticAlloc[Allocate Static Buffers]
    StaticAlloc --> BufferInit[Initialize Buffer Pools]
    BufferInit --> ValidateSize{Buffer Sizes Valid?}
    ValidateSize -->|No| MemFail[Return Memory Error]
    ValidateSize -->|Yes| MemReady[Memory System Ready]
    
    MemReady --> ProcessOps[Process Operations]
    ProcessOps --> CheckUsage[Monitor Memory Usage]
    CheckUsage --> UsageOK{Within Limits?}
    UsageOK -->|No| MemWarning[Log Memory Warning]
    UsageOK -->|Yes| ContinueOps[Continue Operations]
    MemWarning --> ContinueOps
    
    ContinueOps --> MoreOps{More Operations?}
    MoreOps -->|Yes| ProcessOps
    MoreOps -->|No| CleanupMem[Cleanup Memory]
    CleanupMem --> MemComplete[Memory Management Complete]
```

### 6.2 Resource Cleanup Flow

```mermaid
flowchart TD
    CleanupStart([Cleanup Start]) --> CheckTempDir{Temp Directory Exists?}
    CheckTempDir -->|Yes| RemoveTemp[Remove Temporary Files]
    CheckTempDir -->|No| CheckHandles[Check Open File Handles]
    
    RemoveTemp --> TempRemoved{Removal Success?}
    TempRemoved -->|No| LogTempError[Log Cleanup Error]
    TempRemoved -->|Yes| CheckHandles
    LogTempError --> CheckHandles
    
    CheckHandles --> HandlesOpen{Open Handles?}
    HandlesOpen -->|Yes| CloseHandles[Close File Handles]
    HandlesOpen -->|No| CheckMemory[Check Allocated Memory]
    
    CloseHandles --> HandlesClosed{All Closed?}
    HandlesClosed -->|No| LogHandleError[Log Handle Error]
    HandlesClosed -->|Yes| CheckMemory
    LogHandleError --> CheckMemory
    
    CheckMemory --> MemoryAllocated{Memory to Free?}
    MemoryAllocated -->|Yes| FreeMemory[Free Allocated Memory]
    MemoryAllocated -->|No| CleanupComplete[Cleanup Complete]
    
    FreeMemory --> MemoryFreed{Free Success?}
    MemoryFreed -->|No| LogMemError[Log Memory Error]
    MemoryFreed -->|Yes| CleanupComplete
    LogMemError --> CleanupComplete
```
