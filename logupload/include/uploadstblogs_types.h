/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file uploadstblogs_types.h
 * @brief Common data structures and type definitions for uploadSTBLogs
 *
 * This file contains all core data structures, enumerations, and constants
 * used throughout the uploadSTBLogs application as defined in the HLD.
 */

#ifndef UPLOADSTBLOGS_TYPES_H
#define UPLOADSTBLOGS_TYPES_H

#include <stdbool.h>


/* ==========================
   Constants
   ========================== */
#define MAX_PATH_LENGTH      512
#define MAX_URL_LENGTH       1024
#define MAX_MAC_LENGTH       32
#define MAX_IP_LENGTH        64
#define MAX_FILENAME_LENGTH  256
#define MAX_CERT_PATH_LENGTH 256
#define LOG_UPLOADSTB "LOG.RDK.UPLOADSTB"

/* ==========================
   Enumerations
   ========================== */

/**
 * @enum Strategy
 * @brief Upload strategies based on trigger conditions
 */
typedef enum {
    STRAT_RRD,             /**< RRD (Remote Debug) single file upload */
    STRAT_PRIVACY_ABORT,   /**< Privacy mode - abort upload */
    STRAT_NO_LOGS,         /**< No previous logs found */
    STRAT_NON_DCM,         /**< Non-DCM upload strategy */
    STRAT_ONDEMAND,        /**< On-demand immediate upload */
    STRAT_REBOOT,          /**< Reboot-triggered upload */
    STRAT_DCM              /**< DCM batching strategy */
} Strategy;

/**
 * @enum UploadPath
 * @brief Upload path selection (Direct vs CodeBig)
 */
typedef enum {
    PATH_DIRECT,           /**< Direct upload using mTLS */
    PATH_CODEBIG,          /**< CodeBig upload using OAuth */
    PATH_NONE              /**< No path available */
} UploadPath;

/**
 * @enum TriggerType
 * @brief Upload trigger types
 */
typedef enum {
    TRIGGER_SCHEDULED = 0,
    TRIGGER_MANUAL = 1,
    TRIGGER_REBOOT = 2,
    TRIGGER_CRASH = 3,
    TRIGGER_DEBUG = 4,
    TRIGGER_ONDEMAND = 5
} TriggerType;

/**
 * @enum UploadResult
 * @brief Upload operation result codes
 */
typedef enum {
    UPLOAD_SUCCESS = 0,
    UPLOAD_FAILED = 1,
    UPLOAD_ABORTED = 2,
    UPLOAD_RETRY = 3
} UploadResult;

/* ==========================
   Configuration & Context Structures
   ========================== */

/**
 * @struct UploadFlags
 * @brief Upload control flags and triggers
 */
typedef struct {
    int rrd_flag;                   /**< RRD mode flag */
    int dcm_flag;                   /**< DCM mode flag */
    int flag;                       /**< General upload flag */
    int upload_on_reboot;           /**< Upload on reboot flag */
    int trigger_type;               /**< Type of upload trigger */
} UploadFlags;

/**
 * @struct UploadSettings
 * @brief Boolean settings for upload behavior
 */
typedef struct {
    bool privacy_do_not_share;      /**< Privacy mode enabled */
    bool ocsp_enabled;              /**< OCSP validation enabled */
    bool encryption_enable;         /**< Encryption enabled */
    bool direct_blocked;            /**< Direct path blocked */
    bool codebig_blocked;           /**< CodeBig path blocked */
    bool include_pcap;              /**< Include PCAP files */
    bool include_dri;               /**< Include DRI logs */
    bool tls_enabled;               /**< TLS 1.2 support enabled */
    bool maintenance_enabled;       /**< Maintenance mode enabled */

} UploadSettings;

/**
 * @struct PathConfig
 * @brief File system paths and directories
 */
typedef struct {
    char log_path[MAX_PATH_LENGTH];           /**< Main log directory */
    char prev_log_path[MAX_PATH_LENGTH];      /**< Previous logs directory */
    char archive_path[MAX_PATH_LENGTH];       /**< Archive output directory */
    char rrd_file[MAX_PATH_LENGTH];           /**< RRD log file path */
    char dri_log_path[MAX_PATH_LENGTH];       /**< DRI logs directory */
    char temp_dir[MAX_PATH_LENGTH];           /**< Temporary directory */
    char telemetry_path[MAX_PATH_LENGTH];     /**< Telemetry directory */
    char dcm_log_file[MAX_PATH_LENGTH];       /**< DCM log file path */
    char dcm_log_path[MAX_PATH_LENGTH];       /**< DCM log directory */
    char iarm_event_binary[MAX_PATH_LENGTH];  /**< IARM event sender location */
} PathConfig;

/**
 * @struct EndpointConfig
 * @brief Upload endpoint URLs and links
 */
typedef struct {
    char endpoint_url[MAX_URL_LENGTH];        /**< Upload endpoint URL */
    char upload_http_link[MAX_URL_LENGTH];    /**< HTTP upload link */
    char presign_url[MAX_URL_LENGTH];         /**< Pre-signed URL */
    char proxy_bucket[MAX_URL_LENGTH];        /**< Proxy bucket for fallback uploads */
} EndpointConfig;

/**
 * @struct DeviceInfo
 * @brief Device identification information
 */
typedef struct {
    char mac_address[MAX_MAC_LENGTH];         /**< Device MAC address */
    char device_type[32];                     /**< Device type (mediaclient, etc.) */
    char build_type[32];                      /**< Build type */                 /**< Device name */
} DeviceInfo;

/**
 * @struct CertificateConfig
 * @brief TLS/mTLS certificate paths
 */
typedef struct {
    char cert_path[MAX_CERT_PATH_LENGTH];     /**< Client certificate path */
    char key_path[MAX_CERT_PATH_LENGTH];      /**< Private key path */
    char ca_cert_path[MAX_CERT_PATH_LENGTH];  /**< CA certificate path */
} CertificateConfig;

/**
 * @struct RetryConfig
 * @brief Retry and timeout configuration
 */
typedef struct {
    int direct_max_attempts;        /**< Max attempts for direct path */
    int codebig_max_attempts;       /**< Max attempts for CodeBig path */
    int direct_retry_delay;         /**< Retry delay for direct (seconds) */
    int codebig_retry_delay;        /**< Retry delay for CodeBig (seconds) */
    int curl_timeout;               /**< Curl operation timeout */
    int curl_tls_timeout;           /**< TLS handshake timeout */
} RetryConfig;

/**
 * @struct RuntimeContext
 * @brief Complete runtime context containing all configuration
 */
typedef struct {
    UploadFlags flags;              /**< Upload control flags */
    UploadSettings settings;        /**< Upload behavior settings */
    PathConfig paths;               /**< File system paths */
    EndpointConfig endpoints;       /**< Upload endpoints */
    DeviceInfo device;              /**< Device information */
    CertificateConfig certificates; /**< Certificate paths */
    RetryConfig retry;              /**< Retry configuration */
} RuntimeContext;

/* ==========================
   Session State Structures
   ========================== */

/**
 * @struct SessionState
 * @brief Tracks the state of an upload session
 */
typedef struct {
    Strategy strategy;              /**< Selected upload strategy */
    UploadPath primary;             /**< Primary upload path */
    UploadPath fallback;            /**< Fallback upload path */
    int direct_attempts;            /**< Number of direct path attempts */
    int codebig_attempts;           /**< Number of CodeBig path attempts */
    int http_code;                  /**< Last HTTP response code */
    int curl_code;                  /**< Last curl return code */
    bool used_fallback;             /**< Whether fallback was used */
    bool success;                   /**< Overall success status */
    char archive_file[MAX_FILENAME_LENGTH];  /**< Generated archive filename */
} SessionState;

/* ==========================
   Metrics & Telemetry Structures
   ========================== */

/**
 * @struct UploadMetrics
 * @brief Metrics and telemetry data for upload operation
 */
typedef struct {
    int total_attempts;             /**< Total upload attempts */
    int fallback_count;             /**< Number of fallback switches */
    long upload_duration_ms;        /**< Total upload duration */
    long archive_size_bytes;        /**< Archive file size */
    int files_collected;            /**< Number of files in archive */
    char last_error[256];           /**< Last error message */
} UploadMetrics;

#endif /* UPLOADSTBLOGS_TYPES_H */
