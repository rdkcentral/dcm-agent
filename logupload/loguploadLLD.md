# Low Level Design (LLD): Migration of `uploadSTBLogs.sh` to C++

## 1. Detailed Design Specifications

### 1.1. Entry Point
- **Class:** `UploadSTBLogsApp`
- **Method:** `int main(int argc, char* argv[])`
- **Responsibility:** Parses command-line arguments, initializes subsystems, selects upload logic flow, handles exit codes.

### 1.2. Configuration Management
- **Class:** `ConfigLoader`
- **Responsibilities:**
  - Loads configuration files: `/etc/include.properties`, `/etc/device.properties`
  - Provides access to parsed properties via `getProperty(key)`

### 1.3. Log File Management
- **Class:** `LogManager`
- **Responsibilities:**
  - Scans log directories (paths are loaded from config).
  - Timestamp and backup log files.
  - Compresses logs into `.tgz` using lightweight tar/gzip library.
  - Applies retention policies (delete files older than N days).
  - Handles copying/backup of special log types (e.g., DRI logs, .pcap).

### 1.4. Upload Manager
- **Class:** `UploadManager`
- **Responsibilities:**
  - Uploads logs via HTTP/S3 (using libcurl or minimal HTTP client).
  - Implements retry logic, failover between direct and Codebig.
  - Handles TLS/MTLS options, OCSP stapling flags.
  - Manages upload state flags, error codes, and protocol switching.

### 1.5. Event Sender
- **Class:** `EventSender`
- **Responsibilities:**
  - Sends event notifications (maintenance, success/failure) via IPC or file signals.
  - Encapsulates platform-specific event logic.

### 1.6. Lock Manager
- **Class:** `LockManager`
- **Responsibilities:**
  - Ensures single instance execution using OS-based mutex or file lock (`flock` equivalent).
  - Handles lock acquisition/release, timeout, and error scenarios.

### 1.7. RFC/Feature Handler
- **Class:** `RfcManager`
- **Responsibilities:**
  - Reads feature flags (e.g., privacy mode, encrypt upload, unscheduled reboot upload disable) from config or service calls.
  - Provides queries for dynamic feature control.

### 1.8. Logger
- **Class:** `Logger`
- **Responsibilities:**
  - Writes timestamped log entries to the appropriate log files.
  - Provides error, info, and telemetry logging methods.

---

## 2. Data Structures

### 2.1. Configuration Properties
```cpp
struct ConfigProperties {
    std::unordered_map<std::string, std::string> values;
};
```

### 2.2. Log File Metadata
```cpp
struct LogFileInfo {
    std::string filename;
    std::string fullPath;
    std::time_t mtime;
    size_t size;
    bool isDRILog;
    bool isPcap;
};
```

### 2.3. Upload State
```cpp
enum class UploadProtocol { HTTP, TFTP, Unknown };
enum class UploadResult { Success, Failed, Aborted };

struct UploadStatus {
    UploadResult result;
    int httpCode;
    int curlRetCode;
    std::string serverUrl;
    std::string errorMsg;
};
```

### 2.4. Lock State
```cpp
struct LockState {
    bool acquired;
    std::string lockFilePath;
    int lockFd;
};
```

---

## 3. Pseudocode & Logic

### 3.1. Main Flow

```cpp
int main(int argc, char* argv[]) {
    // Parse arguments
    Args args = Args::parse(argc, argv);

    // Load configuration
    ConfigLoader config;
    config.load("/etc/include.properties");
    config.load("/etc/device.properties");

    // Acquire lock
    LockManager lockMgr;
    if (!lockMgr.acquire("/tmp/.log-upload.lock")) {
        Logger::log("Another instance is running, exiting.");
        EventSender::send(Event::MaintenanceInProgress);
        return 1;
    }

    // Initialize feature flags
    RfcManager rfc(config);

    // Select upload mode
    UploadMode mode = selectMode(args, rfc);

    // Prepare logs
    LogManager logMgr(config, rfc);
    std::vector<LogFileInfo> logs = logMgr.prepareLogs(mode);

    // Upload logs
    UploadManager uploadMgr(config, rfc);
    UploadStatus status = uploadMgr.upload(logs, args);

    // Post-upload event and cleanup
    EventSender::send(status.result == UploadResult::Success ? Event::LogUploadSuccess : Event::LogUploadFailure);
    logMgr.cleanup();

    lockMgr.release();
    return status.result == UploadResult::Success ? 0 : 1;
}
```

### 3.2. Retry and Failover Logic

```cpp
// HTTP Upload with direct and Codebig failover
UploadStatus UploadManager::upload(const std::vector<LogFileInfo>& logs, Args args) {
    int attempt = 0;
    int maxAttempts = (args.triggerType == 1) ? 1 : 3;
    UploadStatus status;

    for (; attempt < maxAttempts; ++attempt) {
        status = tryDirectUpload(logs, args);
        if (status.result == UploadResult::Success) break;
        if (status.httpCode == 404 || status.httpCode == 200) break;
        Logger::log("Direct upload failed, retrying...");
        sleep(60);
    }

    if (status.result != UploadResult::Success && canUseCodebig(args, rfc)) {
        for (int cbAttempt = 0; cbAttempt < 1; ++cbAttempt) {
            status = tryCodebigUpload(logs, args);
            if (status.result == UploadResult::Success || status.httpCode == 404) break;
            Logger::log("Codebig upload failed, retrying...");
            sleep(10);
        }
    }
    return status;
}
```

---

## 4. Interface Definitions

### 4.1. ConfigLoader

```cpp
class ConfigLoader {
public:
    void load(const std::string& filePath);
    std::string getProperty(const std::string& key) const;
};
```

### 4.2. LogManager

```cpp
class LogManager {
public:
    LogManager(const ConfigLoader& config, const RfcManager& rfc);
    std::vector<LogFileInfo> prepareLogs(UploadMode mode);
    void cleanup();
};
```

### 4.3. UploadManager

```cpp
class UploadManager {
public:
    UploadManager(const ConfigLoader& config, const RfcManager& rfc);
    UploadStatus upload(const std::vector<LogFileInfo>& logs, Args args);
};
```

### 4.4. LockManager

```cpp
class LockManager {
public:
    bool acquire(const std::string& lockFilePath);
    void release();
};
```

### 4.5. EventSender

```cpp
class EventSender {
public:
    static void send(Event event);
};
```

---

## 5. Error Handling & Edge Cases

- All file operations checked for existence, permissions, and errors.
- Network operations implement timeout, retry, and fallback.
- Upload failures log error codes and messages.
- RFC flags (e.g., privacy mode) trigger log deletion or skip upload.
- Lock acquisition failure triggers event and clean exit.
- Upload protocol fallback: Direct → Codebig → Proxy (where applicable).
- Edge cases for empty log directories, missing config files, and unknown device types handled gracefully.

---

## 6. Code Snippets

### 6.1. Log Preparation Example

```cpp
std::vector<LogFileInfo> LogManager::prepareLogs(UploadMode mode) {
    std::vector<LogFileInfo> logs;
    // Scan directory, filter by extension, apply timestamp
    for (const auto& entry : std::filesystem::directory_iterator(logPath)) {
        if (isLogFile(entry)) {
            LogFileInfo info = extractLogInfo(entry);
            if (shouldBackup(info)) backupLog(info);
            logs.push_back(info);
        }
    }
    // Compress logs into tarball
    compressLogs(logs, archiveName);
    return logs;
}
```

### 6.2. HTTP Upload Example

```cpp
UploadStatus UploadManager::tryDirectUpload(const std::vector<LogFileInfo>& logs, Args args) {
    UploadStatus status;
    // Use libcurl to POST log archive
    CURL *curl = curl_easy_init();
    if (!curl) {
        status.result = UploadResult::Failed;
        status.errorMsg = "curl init failed";
        return status;
    }
    curl_easy_setopt(curl, CURLOPT_URL, args.serverUrl.c_str());
    // Configure SSL/TLS, timeouts, etc.
    // ...
    CURLcode res = curl_easy_perform(curl);
    status.curlRetCode = res;
    // Parse response, set status.httpCode, etc.
    curl_easy_cleanup(curl);
    return status;
}
```

---

## 7. Notes

- All external shell utilities (e.g., tar, curl) replaced by C++ libraries.
- Sensitive operations (deletion, event signaling) must be atomic and robust.
- All file and network buffers sized for low-memory operation.
- All paths, timeouts, and protocol details must be configurable for target environments.
