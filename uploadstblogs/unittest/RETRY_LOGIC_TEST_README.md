# Google Test Implementation for retry_logic.c

## Overview

This document describes the comprehensive Google Test implementation for the `retry_logic.c` module in the uploadSTBLogs component. The test suite provides complete coverage of the retry logic functionality including upload retry attempts, failure handling, and telemetry reporting.

## Files Created

### 1. retry_logic_gtest.cpp
**Location**: `unittest/retry_logic_gtest.cpp`  
**Purpose**: Complete Google Test implementation for retry_logic.c

**Key Features**:
- **Real Implementation Testing**: Includes actual `retry_logic.c` source code with `#ifdef GTEST_ENABLE`
- **External Function Mocking**: Mocks `report_upload_attempt()` and `is_terminal_failure()` functions
- **Comprehensive Test Coverage**: 25+ test cases covering all functions and edge cases
- **Mock Upload Functions**: Configurable mock upload functions for testing different scenarios
- **Integration Tests**: End-to-end testing with telemetry and failure handling

### 2. Makefile.am Updates
**Location**: `unittest/Makefile.am`  
**Changes**: Added `retry_logic_gtest` to build configuration

**Added Configuration**:
```makefile
bin_PROGRAMS = ... retry_logic_gtest

retry_logic_gtest_SOURCES = retry_logic_gtest.cpp
retry_logic_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
retry_logic_gtest_LDADD = $(COMMON_LDADD)
retry_logic_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
retry_logic_gtest_CFLAGS = $(COMMON_CXXFLAGS)
```

### 3. Build Script
**Location**: `unittest/run_retry_logic_test.sh`  
**Purpose**: Standalone build and run script for the test

## Test Coverage

### Core Functions Tested

#### 1. retry_upload()
- **Success scenarios**: First-try success, success after retries
- **Failure scenarios**: Max attempts reached, terminal failures, network failures
- **Input validation**: NULL pointers for context, session, and attempt function
- **Path-specific logic**: Direct vs CodeBig retry limits
- **Result handling**: Different return codes (SUCCESS, FAILED, RETRY, ABORTED)

#### 2. should_retry()
- **Success/abort conditions**: No retry on success or abort
- **Network failures**: HTTP 000 code handling (no retry)
- **Terminal failures**: HTTP 404 handling (no retry per script logic)
- **Attempt limits**: Direct and CodeBig path-specific limits
- **Result types**: FAILED and RETRY result handling
- **Invalid inputs**: NULL context/session, invalid paths

#### 3. increment_attempts()
- **Path tracking**: Separate counters for Direct and CodeBig paths
- **Input validation**: NULL session handling
- **Multiple increments**: Counter accumulation testing
- **Invalid paths**: PATH_NONE handling

### Mock Functions

#### External Function Mocks
```cpp
// Telemetry mock - tracks call count
void report_upload_attempt(void);

// Verification mock - configurable terminal failure detection  
bool is_terminal_failure(int http_code);
```

#### Upload Function Mocks
```cpp
// Success/failure scenarios
static UploadResult mock_upload_success(...);
static UploadResult mock_upload_fail(...);
static UploadResult mock_upload_retry(...);
static UploadResult mock_upload_aborted(...);

// Configurable mock for complex scenarios
static UploadResult mock_upload_configurable(...);

// Success on Nth attempt for retry testing
static UploadResult mock_upload_succeed_on_nth_call(...);
```

## Test Scenarios

### Basic Functionality
- **RetryUpload_SuccessOnFirstTry**: Verifies single successful upload
- **RetryUpload_NullContext/Session/Function**: Input validation
- **RetryUpload_InvalidPath**: PATH_NONE handling

### Retry Logic
- **RetryUpload_DirectPath_RetriesUntilMaxAttempts**: Direct path retry limits
- **RetryUpload_CodeBigPath_RetriesUntilMaxAttempts**: CodeBig path retry limits  
- **RetryUpload_SuccessOnSecondTry**: Multi-attempt success scenarios

### Failure Handling
- **RetryUpload_AbortedResult_NoRetry**: ABORTED result handling
- **RetryUpload_RetryResult_RetriesUntilMax**: RETRY result processing

### should_retry() Edge Cases
- **ShouldRetry_NetworkFailure_HTTP000**: Network failure detection
- **ShouldRetry_TerminalFailure_HTTP404**: Terminal failure (404 only)
- **ShouldRetry_NonTerminalHttpCodes**: 500, 503, 408 retry scenarios
- **ShouldRetry_ExceededAttemptLimit**: Max attempt limit enforcement

### Integration Tests
- **Integration_RetryLogicWithTelemetry**: Telemetry call verification
- **Integration_TerminalFailurePreventsRetry**: Terminal failure bypass
- **Integration_NetworkFailurePreventsRetry**: Network failure bypass
- **Integration_MixedPathAttempts**: Cross-path attempt tracking

## Key Design Decisions

### 1. Real Implementation Testing
- Includes actual source code with `#ifdef GTEST_ENABLE` guards
- Tests real function logic rather than mocking internal functions
- Maintains compatibility with production builds

### 2. External Dependency Mocking  
- Mocks only external functions (`report_upload_attempt`, `is_terminal_failure`)
- Allows controlled testing of different failure scenarios
- Tracks telemetry calls for verification

### 3. Comprehensive Scenario Coverage
- Tests all return code paths (SUCCESS, FAILED, RETRY, ABORTED)
- Covers both Direct and CodeBig upload paths
- Includes edge cases and error conditions

### 4. Path-Specific Testing
- Separate attempt counters for Direct vs CodeBig paths
- Different retry limits based on configuration
- Independent failure tracking per path

## Building and Running

### Using Autotools (Recommended)
```bash
cd uploadstblogs/unittest
autoreconf -fiv
./configure
make retry_logic_gtest
./retry_logic_gtest
```

### Using Direct Compilation
```bash
cd uploadstblogs/unittest
./run_retry_logic_test.sh
```

### Integration with Test Suite
```bash
cd uploadstblogs/unittest
make check  # Runs all tests including retry_logic_gtest
```

## Dependencies

### Required Libraries
- **Google Test/GMock**: Test framework
- **pthread**: Threading support
- **curl**: Upload functionality (mocked in tests)
- **cjson**: JSON parsing (mocked in tests)
- **rbus**: RDK bus communication (mocked in tests)
- **rdkloggers**: RDK logging framework

### Header Dependencies
- `uploadstblogs_types.h`: Core type definitions
- `retry_logic.h`: Function declarations
- `rdklogger.h`: Logging macros (locally defined in tests)

## Expected Output

When run successfully, the test should output:
```
[==========] Running 25+ tests from 1 test case.
[----------] Global test environment set-up.
[----------] 25+ tests from RetryLogicTest
[ RUN      ] RetryLogicTest.RetryUpload_SuccessOnFirstTry
[       OK ] RetryLogicTest.RetryUpload_SuccessOnFirstTry
... (additional test results)
[----------] 25+ tests from RetryLogicTest (X ms total)
[==========] 25+ tests from 1 test case ran. (X ms total)
[  PASSED  ] 25+ tests.
```

## Notes

- The test implementation follows the established pattern from other uploadSTBLogs unit tests
- External dependencies are mocked to ensure test isolation
- The retry logic matches the original shell script behavior (immediate retries, 404-only terminal failures)
- Telemetry reporting is verified through mock function call counting
- Path-specific attempt tracking ensures proper separation of Direct and CodeBig upload statistics