# USB Log Upload - Shared Functions Analysis

## Functions from uploadstblogs that can be reused:

### File Operations (`file_operations.h/c`)
**Highly Reusable:**
- `file_exists(const char* filepath)` - Check if file exists
- `dir_exists(const char* dirpath)` - Check if directory exists  
- `create_directory(const char* dirpath)` - Create directory recursively
- `remove_file(const char* filepath)` - Remove file
- `remove_directory(const char* dirpath)` - Remove directory recursively
- `join_path(char* buffer, size_t buffer_size, const char* dir, const char* filename)` - Safely join paths
- `get_file_size(const char* filepath)` - Get file size in bytes
- `move_directory_contents(const char* src_dir, const char* dest_dir)` - Move all directory contents

### Archive Management (`archive_manager.h/c`)
**Highly Reusable:**
- `generate_archive_name(char* buffer, size_t buffer_size, const char* mac_address, const char* prefix)` 
  - Generates filenames in format: `<MAC>_Logs_<MM-DD-YY-HH-MMAM/PM>.tgz`
  - Removes colons from MAC address automatically
- `create_archive(RuntimeContext* ctx, SessionState* session, const char* source_dir)`
  - Creates tar.gz archives (matches usbLogUpload.sh tar -zcvf requirement)

### Validation (`validation.h/c`) 
**Partially Reusable:**
- `validate_directories(const RuntimeContext* ctx)` - Check required directories exist
- `validate_binaries(void)` - Check required system binaries are available

### System Utilities Pattern
**Adaptable:**
- Timestamp generation in format `MM-DD-YY-HH-MMAM/PM` (matches usbLogUpload.sh)
- MAC address retrieval and formatting
- Configuration file parsing patterns
- Error logging and debugging patterns

## Recommended Integration Strategy:

1. **Direct Reuse:** 
   - File operations functions for directory/file management
   - Archive name generation for consistent filename format
   - Path joining utilities for safe path construction

2. **Adaptation Required:**
   - Archive creation (adapt for USB-specific requirements)
   - MAC address retrieval (may need USB-specific implementation)
   - Validation functions (adapt for USB-specific checks)

3. **USB-Specific Implementation:**
   - USB mount point validation
   - Device compatibility checks (PLATCO-only requirement)
   - syslog-ng service restart logic

## Implementation Benefits:

- **Code Reuse:** ~70% of utility functions can be directly reused
- **Consistency:** Same filename format and archive structure
- **Reliability:** Well-tested functions from existing uploadstblogs module
- **Maintainability:** Single source of truth for common operations