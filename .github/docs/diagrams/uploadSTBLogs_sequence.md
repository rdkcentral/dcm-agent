# Sequence Diagrams & Text – Strict Diagram Alignment

## 1. Normal Path (Reboot Strategy)
```mermaid
sequenceDiagram
    participant Main
    participant Config
    participant Archive
    participant UploadEngine
    participant Security
    participant Events

    Main->>Config: Context Initialization
    Main->>Main: System Validation
    Main->>Main: Early Return Checks (continue)
    Main->>Main: Strategy Selector -> Reboot Strategy
    Main->>Archive: Prepare archive (.tgz)
    Archive-->>Main: Archive ready
    Main->>UploadEngine: Start upload
    UploadEngine->>Security: MTLS setup (Direct Path)
    Security-->>UploadEngine: TLS ready
    UploadEngine->>UploadEngine: Pre-sign request
    UploadEngine->>UploadEngine: S3 Upload PUT
    UploadEngine-->>Main: Verification success
    Main->>Events: Emit success + cleanup
```

### Text Alternative
1. Initialize context.
2. Validate system.
3. Determine Reboot Strategy.
4. Build archive.
5. Execute upload (Direct path with mTLS).
6. Verify success.
7. Cleanup and emit success event.

## 2. Fallback Scenario
```mermaid
sequenceDiagram
    participant Main
    participant UploadEngine
    participant Security
    participant Events

    Main->>UploadEngine: Execute Direct Path
    UploadEngine->>Security: MTLS setup
    Security-->>UploadEngine: Ready
    UploadEngine->>UploadEngine: Pre-sign (failure non-404)
    UploadEngine->>UploadEngine: Retry attempts exhaust
    UploadEngine->>UploadEngine: Invoke Fallback Handler
    UploadEngine->>Security: OAuth setup (CodeBig)
    Security-->>UploadEngine: Ready
    UploadEngine->>UploadEngine: Pre-sign success (CodeBig)
    UploadEngine->>UploadEngine: S3 Upload success
    UploadEngine-->>Main: Success via fallback
    Main->>Events: Emit success (fallback used), update block markers
```

### Text Alternative
Direct path fails, fallback to CodeBig OAuth succeeds, success emitted, direct block marker may be set.

## 3. Privacy Abort
```mermaid
sequenceDiagram
    participant Main
    participant Config
    participant Events

    Main->>Config: Context Initialization
    Main->>Main: Early Return Checks (Privacy)
    Main->>Main: Truncate logs
    Main->>Events: Emit privacy abort event
```

### Text Alternative
Privacy mode triggers early exit; no archive or upload.

## 4. RRD Strategy
```mermaid
sequenceDiagram
    participant Main
    participant UploadEngine
    participant Security
    participant Events

    Main->>Main: Detect RRD Flag
    Main->>UploadEngine: RRD file upload request
    UploadEngine->>Security: Path auth (Direct or CodeBig)
    Security-->>UploadEngine: Ready
    UploadEngine->>UploadEngine: Pre-sign + Upload
    UploadEngine-->>Main: Result
    Main->>Events: Emit success/failure
```

### Text Alternative
RRD bypasses archive packaging, performs single file upload with same verification path.
