# Upload Architecture Documentation

## Overview
The upload system is organized into three distinct layers with clear separation of concerns:

1. **HTTP Layer** (`http_upload.c`) - Low-level HTTP/CURL primitives
2. **mTLS Layer** (`mtls_cert_selector.c`) - Certificate management and mTLS orchestration
3. **Orchestration Layer** (`upload_engine.c`) - High-level retry/fallback logic

## Layer Responsibilities

### 1. HTTP Layer (`http_upload.c`)
**Purpose**: Pure HTTP/CURL utility functions without business logic

**Functions**:
- `extract_s3_url()` - Parse S3 presigned URL from response file
- `doHttpFileUpload()` - Metadata POST with optional mTLS
- `doS3PutUpload()` - S3 PUT with optional mTLS
- `runFileUpload()` - Thin wrapper (delegates to mTLS layer if enabled)

**Key Characteristics**:
- No retry logic
- No certificate management
- No path selection
- Simple success/failure returns
- Can accept optional `MtlsAuth_t*` parameter

**Dependencies**: CURL, downloadUtil

---

### 2. mTLS Layer (`mtls_cert_selector.c`)
**Purpose**: All certificate selection and mTLS-aware upload logic

**Functions**:
- `loguploadGetCert()` - Fetch certificate/key from rdkcertselector
- `http_upload_with_mtls()` - Complete mTLS upload workflow

**Key Characteristics**:
- Owns all `rdkcertselector` operations
- Implements certificate rotation (TRY_ANOTHER loop)
- Two-stage upload workflow:
  1. Metadata POST to get presigned URL
  2. S3 PUT with file content
- Automatic cleanup of cert selector resources
- No path selection or fallback logic

**Certificate Rotation Flow**:
```
1. Initialize rdkcertselector
2. Loop:
   a. Fetch cert/key via loguploadGetCert()
   b. Attempt metadata POST with cert
   c. If POST succeeds → break and do S3 PUT
   d. If cert expired → rdkcertselector_setCurlStatus(TRY_ANOTHER)
   e. Retry with next certificate
3. Cleanup cert selector
```

**Dependencies**: http_upload.c, rdkcertselector library

---

### 3. Orchestration Layer (`upload_engine.c`)
**Purpose**: High-level upload orchestration with retry and fallback

**Functions**:
- `execute_upload_cycle()` - Main orchestration entry point
- `attempt_upload()` - Single upload attempt wrapper
- `should_fallback()` - Determines if fallback is needed
- `switch_to_fallback()` - Switches from Direct → CodeBig
- `upload_archive()` - Public API for archive uploads

**Key Characteristics**:
- Path-aware retry logic (Direct vs CodeBig)
- Automatic fallback on Direct failure
- Delegates actual uploads to lower layers
- Transient error handling
- Max attempt tracking per path

**Orchestration Flow**:
```
1. Start with Direct path
2. Retry Direct up to max attempts (retry_logic.c)
   - Use path-specific delays (60s for Direct)
   - Check transient errors
3. If Direct exhausted → fallback to CodeBig
4. Retry CodeBig up to max attempts
   - Use path-specific delays (10s for CodeBig)
5. Return final status
```

**Integration Points**:
- Uses `retry_logic.c` for retry loops
- Calls `http_upload_with_mtls()` for Direct path (mTLS)
- Calls `runFileUpload()` for CodeBig path (non-mTLS)

**Dependencies**: retry_logic.c, mtls_cert_selector.c, http_upload.c

---

## Upload Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                      upload_engine.c                             │
│                                                                   │
│  execute_upload_cycle()                                          │
│    ├─→ Retry Direct (max 3 attempts, 60s delay)                 │
│    │    └─→ attempt_upload() → http_upload_with_mtls()          │
│    │                                                              │
│    └─→ Fallback to CodeBig (max 5 attempts, 10s delay)          │
│         └─→ attempt_upload() → runFileUpload()                   │
└─────────────────────────────────────────────────────────────────┘
                        ↓                    ↓
┌──────────────────────────────────┐  ┌────────────────────────┐
│     mtls_cert_selector.c         │  │    http_upload.c       │
│                                   │  │                        │
│  http_upload_with_mtls()         │  │  runFileUpload()       │
│    ├─→ Init rdkcertselector      │  │    (delegates to       │
│    ├─→ Loop: TRY_ANOTHER         │  │     mTLS layer)        │
│    │    └─→ loguploadGetCert()   │  │                        │
│    ├─→ Metadata POST             │  └────────────────────────┘
│    └─→ S3 PUT                     │              ↓
│                                   │  ┌────────────────────────┐
└───────────────────────────────────┘  │    http_upload.c       │
                    ↓                   │                        │
        ┌───────────────────────────┐  │  extract_s3_url()      │
        │     http_upload.c         │  │  doHttpFileUpload()    │
        │                           │  │  doS3PutUpload()       │
        │  doHttpFileUpload()       │  │                        │
        │  doS3PutUpload()          │  └────────────────────────┘
        └───────────────────────────┘
```

---

## Path Configuration

### Direct Path (mTLS enabled)
- **Max Attempts**: 3
- **Retry Delay**: 60 seconds
- **Upload Function**: `http_upload_with_mtls()`
- **Certificate**: Automatic rotation via rdkcertselector
- **Workflow**: Metadata POST → S3 PUT (both with mTLS)

### CodeBig Path (Fallback, no mTLS)
- **Max Attempts**: 5
- **Retry Delay**: 10 seconds
- **Upload Function**: `runFileUpload()`
- **Certificate**: None
- **Workflow**: Metadata POST → S3 PUT (plain HTTPS)

---

## Error Handling

### Transient Errors (Retry-able)
Defined in `retry_logic.c`:
- `CURLE_OPERATION_TIMEDOUT`
- `CURLE_COULDNT_CONNECT`
- `CURLE_COULDNT_RESOLVE_HOST`
- HTTP 429 (Too Many Requests)
- HTTP 503 (Service Unavailable)
- HTTP 504 (Gateway Timeout)

### Permanent Errors (No Retry)
- HTTP 4xx (except 429)
- Certificate fetch failure (`MTLS_CERT_FETCH_FAILURE`)
- Invalid parameters

### Fallback Triggers
- Direct path exhausted all attempts
- Metadata POST succeeded but S3 PUT failed
- Certificate validation failures

---

## Design Principles

### 1. Single Responsibility
Each layer has one clear purpose:
- HTTP layer: CURL operations
- mTLS layer: Certificate management
- Orchestration layer: Retry/fallback logic

### 2. Dependency Direction
```
upload_engine.c → mtls_cert_selector.c → http_upload.c
                ↘                      ↗
                  retry_logic.c
```
No circular dependencies, clear hierarchy.

### 3. Testability
- Each layer can be unit tested independently
- Mock boundaries at layer interfaces
- No global state (except cert selector handle)

### 4. Maintainability
- Certificate logic consolidated in one file
- Retry configuration in one place
- HTTP primitives reusable across paths

---

## Future Enhancements

### Potential Improvements
1. **Async Uploads**: Background thread pool for parallel uploads
2. **Metrics**: Upload success rates, retry counts, latency tracking
3. **Dynamic Config**: Runtime adjustment of retry delays/attempts
4. **Certificate Caching**: Reduce rdkcertselector calls
5. **Circuit Breaker**: Temporarily skip Direct if repeated failures

### Migration Notes
- If adding new upload paths, extend `upload_engine.c`
- If adding new mTLS providers, extend `mtls_cert_selector.c`
- HTTP primitives (`http_upload.c`) should remain stable

---

## API Usage Examples

### Example 1: Simple Upload (Orchestration Layer)
```c
#include "upload_engine.h"

int result = upload_archive("archive.tar.gz", "http://direct.example.com/upload");
if (result == 0) {
    printf("Upload successful\n");
} else {
    printf("Upload failed after retry/fallback\n");
}
```

### Example 2: mTLS Upload (mTLS Layer)
```c
#include "mtls_cert_selector.h"

int result = http_upload_with_mtls("http://example.com/upload", "file.tar.gz");
if (result == 0) {
    printf("mTLS upload successful\n");
}
```

### Example 3: Direct HTTP Primitives (HTTP Layer)
```c
#include "http_upload.h"

void *curl = doCurlInit();
FileUpload_t upload = {
    .url = "http://example.com/upload",
    .pathname = "file.tar.gz",
    .sslverify = 1,
    .pPostFields = NULL,
    .hashData = NULL
};
long http_code;

int result = doHttpFileUpload(curl, &upload, NULL, &http_code);
doStopDownload(curl);
```

---

## Build Configuration

### Required Libraries
- libcurl
- librdkcertselector (for mTLS)
- librbus (for configuration)
- librdk (for logging)

### Compiler Flags
```makefile
# Enable mTLS support
CFLAGS += -DLIBRDKCERTSELECTOR

# Include paths
CFLAGS += -I$(SRCDIR)/include
CFLAGS += -I$(SRCDIR)/../common_utilities/utils

# Link flags
LDFLAGS += -lcurl -lrdkcertselector
```

---

## Troubleshooting

### Common Issues

**Issue**: Upload fails with "Cert selector init failed"
- **Solution**: Verify rdkcertselector library is installed and accessible

**Issue**: All Direct attempts fail, no fallback
- **Solution**: Check `should_fallback()` logic in upload_engine.c

**Issue**: Infinite certificate rotation loop
- **Solution**: Verify rdkcertselector_setCurlStatus() receives correct curl error code

**Issue**: S3 PUT fails after successful POST
- **Solution**: Check S3 URL extraction, verify presigned URL validity

---

## Version History
- **v1.0** (2025-01-XX): Initial architecture documentation
  - Three-layer separation
  - mTLS consolidation
  - Retry/fallback orchestration
