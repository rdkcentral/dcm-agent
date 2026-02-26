# USB Log Upload Module

## Overview

This module provides the C implementation of USB log upload functionality, migrated from the original `usbLogUpload.sh` shell script. It enables transfer of system logs from embedded devices to external USB storage with compression and proper naming conventions.

## Architecture

The module follows a layered modular architecture:

### Core Modules

1. **Main Control Module** (`usb_log_main.c/.h`)
   - Application entry point and argument parsing
   - High-level workflow orchestration
   - Exit code management

2. **Validation Module** (`usb_log_validation.c/.h`)
   - Device compatibility verification
   - USB mount point validation
   - Input parameter validation

3. **File Manager Module** (`usb_log_file_manager.c/.h`)
   - Log file discovery and management
   - Directory operations
   - File movement and copying

4. **Archive Manager Module** (`usb_log_archive.c/.h`)
   - Log file compression and archiving
   - Archive naming convention implementation
   - Compression error handling

5. **Utility Module** (`usb_log_utils.c/.h`)
   - Common utility functions
   - Logging and configuration management
   - Error handling

## Building

### Prerequisites

- GCC compiler
- Autotools (autoconf, automake)
- Standard C library

### Build Instructions

```bash
# Using configure script
./configure
make
make install

# Or using direct Makefile
make all
```

### Debug Build

```bash
./configure --enable-debug
make
```

## Usage

```bash
usblogupload <USB_mount_point>
```

### Example

```bash
usblogupload /mnt/usb
```

## Exit Codes

- `0`: Success
- `2`: USB not mounted
- `3`: Writing error to USB
- `4`: Invalid usage or unsupported device

## Configuration

The module reads configuration from:
- `/etc/include.properties`
- `/etc/device.properties`

### Environment Variables

- `DEVICE_NAME`: Device type identifier (must be "PLATCO")
- `RDK_PATH`: RDK library path (default: `/lib/rdk`)
- `LOG_PATH`: System log directory path
- `SYSLOG_NG_ENABLED`: Syslog-ng service status

## Features

- **Device Validation**: Supports PLATCO devices only
- **Log Archival**: Creates compressed `.tgz` archives
- **Naming Convention**: `<MAC>_Logs_<timestamp>.tgz`
- **Service Management**: Reloads syslog-ng after log transfer
- **Error Handling**: Comprehensive error checking and reporting

## Testing

### Unit Tests

```bash
# Build and run unit tests
make test
./bin/test_usblogupload
```

### Google Test Framework

Unit tests use Google Test and Google Mock frameworks:

```bash
# Run GTest unit tests
cd unittest
make && ./run_tests
```

## Development

### Directory Structure

```
usbLogUpload/
├── include/                 # Header files
│   ├── usb_log_main.h
│   ├── usb_log_validation.h
│   ├── usb_log_file_manager.h
│   ├── usb_log_archive.h
│   └── usb_log_utils.h
├── src/                     # Source files
│   ├── usb_log_main.c
│   ├── usb_log_validation.c
│   ├── usb_log_file_manager.c
│   ├── usb_log_archive.c
│   ├── usb_log_utils.c
│   └── test/               # Integration tests
│       └── test_main.c
├── unittest/               # Unit tests (GTest)
│   ├── usb_log_main_gtest.cpp
│   ├── usb_log_validation_gtest.cpp
│   └── usb_log_file_manager_gtest.cpp
├── docs/                   # Documentation
│   ├── usb-log-upload-requirements.md
│   ├── usb-log-upload-hld.md
│   └── usb-log-upload-flowcharts.md
├── Makefile                # Build configuration
├── Makefile.am             # Automake configuration
├── configure.ac            # Autoconf configuration
└── README.md              # This file
```

### Coding Standards

- Follow embedded C coding standards
- Use static memory allocation where possible
- Minimize resource usage for embedded systems
- Include comprehensive error handling
- Document all public APIs

## License

Licensed under the Apache License, Version 2.0. See the LICENSE file for details.

## Support

For issues and support, contact: support@rdkcentral.com