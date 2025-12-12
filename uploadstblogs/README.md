# uploadSTBLogs C Migration - Project Structure

This directory contains the C implementation of uploadSTBLogs based on the High Level Design (HLD).

## Project Structure

### Core Components

- **uploadstblogs.c/h** - Main entry point and orchestration
- **uploadstblogs_types.h** - Common data structures and type definitions

### Modules

#### Context & Initialization
- **context_manager.c/h** - Runtime context initialization, environment loading
- **validation.c/h** - System validation and prerequisite checks
- **config_manager.c/h** - Configuration file loading and parsing

#### Strategy & Decision Logic
- **strategy_selector.c/h** - Upload strategy selection decision tree

#### Upload Engine
- **upload_engine.c/h** - Upload execution orchestration
- **path_handler.c/h** - Direct (mTLS) and CodeBig (OAuth) path handlers
- **retry_logic.c/h** - Retry loops and delay handling

#### Archive Management
- **archive_manager.c/h** - Log collection and archive creation
- **log_collector.c/h** - Log file collection and filtering

#### Authentication
- **mtls_handler.c/h** - mTLS authentication for Direct path
- **oauth_handler.c/h** - OAuth authentication for CodeBig path

#### Security Layer
- **cert_manager.c/h** - Certificate management
- **tls_handler.c/h** - TLS configuration
- **ocsp_validator.c/h** - OCSP validation support

#### Support Modules
- **file_operations.c/h** - Common file system operations
- **event_manager.c/h** - Event emission and notifications
- **telemetry.c/h** - Telemetry and metrics reporting

#### Cleanup & Verification
- **cleanup_handler.c/h** - Cleanup and finalization
- **verification.c/h** - Upload verification

## Build System

- **uploadstblogs.am** - Automake configuration (to be included in main Makefile.am)

## Design Alignment

This implementation strictly follows the HLD design:

### Strategy Flow
1. **Main Entry** → Parse args, acquire lock
2. **Context Initialization** → Load environment, TR-181, RFC values
3. **System Validation** → Verify directories, binaries, config
4. **Early Return Checks** → RRD, Privacy, No Logs, or Continue
5. **Strategy Selector** → Non-DCM, OnDemand, Reboot, or DCM
6. **Archive Manager** → Collect logs, create archive
7. **Upload Engine** → Execute with retry and fallback
8. **Cleanup & Notification** → Finalize, emit events

### Upload Paths
- **Direct Path**: mTLS authentication, xPKI certificates
- **CodeBig Path**: OAuth authentication, signed URLs
- **Fallback**: Single fallback cycle on non-terminal failures

### Retry Logic
- **Direct**: Configurable attempts with 60s delay
- **CodeBig**: Configurable attempts with 10s delay

## Implementation Status

**Status**: Skeleton structure created (no implementation)

All modules contain:
- Complete header file with function declarations
- Stub implementation files with TODO comments
- No actual functionality implemented

## Next Steps

To implement the full functionality:
1. Implement context_manager: Load environment variables and TR-181 parameters
2. Implement validation: Check system prerequisites
3. Implement strategy_selector: Decision tree logic
4. Implement archive_manager: Log collection and tar.gz creation
5. Implement path_handler: curl-based upload with authentication
6. Implement retry_logic: Retry loops with delays
7. Implement cleanup_handler: Block markers and cleanup
8. Implement event_manager: IARM events
9. Implement telemetry: T2 markers
10. Integration testing with actual RDK environment

## Dependencies

- libcurl (HTTP/HTTPS transfers)
- OpenSSL (TLS/mTLS, certificates)
- pthread (threading support)
- IARM Bus (event notifications)
- RBus (TR-181 parameters)
- Telemetry 2.0 API (optional)

## Build Instructions

To integrate into dcm-agent:

1. Update main `Makefile.am` to include `uploadstblogs.am`
2. Run autotools: `autoreconf -fi`
3. Configure: `./configure`
4. Build: `make`
5. Install: `make install`

## License

Apache License 2.0 - See LICENSE file for details.
