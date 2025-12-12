# Flowcharts – Strict Diagram Alignment

## 1. Core Flow (Original Diagram Reflected)

```mermaid
graph TB
    A[Main Entry Point] --> B[Context Initialization]
    B --> C[System Validation]
    C --> D{Early Return Checks}
    
    D -->|RRD Flag| E[RRD Strategy]
    D -->|Privacy Mode| F[Privacy Strategy]
    D -->|No Previous Logs| G[No Logs Strategy]
    D -->|Continue| H[Strategy Selector]
    
    H --> I[Selected Strategy]
    
    I --> J[Non-DCM Strategy]
    I --> K[OnDemand Strategy]
    I --> L[Reboot Strategy]
    I --> M[DCM Strategy]
    
    J --> AM[Archive Manager]
    K --> AM
    L --> AM
    M --> AM

    %% RRD strategy feeds directly to upload path
    E --> N[Upload Execution Engine]
    AM --> N[Upload Execution Engine]
    
    N --> O[Direct Upload Path]
    N --> P[CodeBig Upload Path]
    N --> Q[Fallback Handler]
    
    O --> R[MTLS Authentication]
    P --> S[OAuth Authentication]
    Q --> T[Retry Logic]
    
    R --> U[HTTP/HTTPS Transfer]
    S --> U
    T --> U
    
    U --> V[Upload Verification]
    V --> W[Cleanup & Notification]
    
    subgraph "Support Modules"
        X[Configuration Manager]
        Y[Log Collector]
        Z[File Operations]
        BB[Event Manager]
    end
    
    subgraph "Security Layer"
        CC[Certificate Management]
        DD[TLS/MTLS Handler]
        EE[OCSP Validation]
    end
    
    A -.-> X
    B -.-> Y
    I -.-> Z
    W -.-> BB
    
    R -.-> CC
    R -.-> DD
    R -.-> EE
```

## 2. Text-Based Flow (Simplified)
1. Main → Initialize → Validate.
2. Early Return:
   - RRD → Upload Engine.
   - Privacy → Abort (no archive).
   - No Logs → Abort.
   - Continue → Strategy selector → Selected strategy → Archive Manager → Upload Engine.
3. Upload Engine:
   - Decide path (Direct/CodeBig).
   - Retry Logic engages fallback if needed.
   - Authentication (mTLS/OAuth).
   - Transfer.
   - Verification.
4. Cleanup & Notification.

## 3. Fallback Handling (Extracted Sub-flow)
```mermaid
graph TD
    A[Start Upload Attempt] --> B[Primary Path Request]
    B --> C{HTTP Code}
    C -->|200| D[Upload to S3]
    C -->|404| E[Terminal Fail]
    C -->|Other| F{Fallback Allowed?}
    F -->|Yes| G[Switch to Alternate Path]
    F -->|No| E
    D --> H{Upload Success?}
    H -->|Yes| I[Success -> Cleanup]
    H -->|No| F
```

## 4. Strategy Selection (Decision Only)
```mermaid
graph LR
    A[Continue Case] --> B{RRD?}
    B -->|Yes| S1[RRD Strategy]
    B -->|No| C{Privacy Mode?}
    C -->|Yes| S2[Privacy Strategy]
    C -->|No| D{Logs Exist?}
    D -->|No| S3[No Logs Strategy]
    D -->|Yes| E{TriggerType==5?}
    E -->|Yes| S4[OnDemand Strategy]
    E -->|No| F{DCM_FLAG==0?}
    F -->|Yes| S5[Non-DCM Strategy]
    F -->|No| G{UploadOnReboot==1 && FLAG==1?}
    G -->|Yes| S6[Reboot Strategy]
    G -->|No| S7[DCM Strategy]
```

## 5. Upload Verification Terminal States
| Condition | Result |
|-----------|--------|
| HTTP 200 + curl success | Success |
| HTTP 404 | Terminal failure (no fallback) |
| Other HTTP + attempts left | Retry/fallback |
| Other HTTP + no attempts/fallback | Failure |
