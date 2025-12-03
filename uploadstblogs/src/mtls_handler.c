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
 * @file mtls_handler.c
 * @brief mTLS authentication implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mtls_handler.h"
#include "cert_manager.h"
#include "ocsp_validator.h"
#include "rdk_debug.h"

bool configure_mtls(const RuntimeContext* ctx, char* curl_args, size_t args_size)
{
    if (!ctx || !curl_args || args_size == 0) {
        return false;
    }
    
    // Load certificate paths if not already loaded
    if (!load_cert_paths((RuntimeContext*)ctx)) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] Failed to load certificate paths\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    // Build curl arguments for mTLS
    int ret = snprintf(curl_args, args_size,
                      "--cert %s --key %s --cacert %s --tlsv1.2",
                      ctx->certificates.cert_path,
                      ctx->certificates.key_path,
                      ctx->certificates.ca_cert_path);
    
    if (ret < 0 || ret >= (int)args_size) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.UPLOADSTB",
                "[%s:%d] curl arguments too long\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.UPLOADSTB",
            "[%s:%d] mTLS curl args: %s\n", __FUNCTION__, __LINE__, curl_args);
    
    return true;
}

bool validate_mtls_certs(const RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    
    // Use cert_manager validation functions
    bool valid = validate_certificates(ctx->certificates.cert_path,
                                     ctx->certificates.key_path,
                                     ctx->certificates.ca_cert_path);
    
    if (valid) {
        // Also check expiration
        if (!check_cert_expiration(ctx->certificates.cert_path)) {
            RDK_LOG(RDK_LOG_WARN, "LOG.RDK.UPLOADSTB",
                    "[%s:%d] Certificate expiration warning\n", __FUNCTION__, __LINE__);
            // Don't fail - just warn
        }
    }
    
    return valid;
}

bool is_ocsp_enabled(const RuntimeContext* ctx)
{
    if (!ctx) {
        return false;
    }
    
    // Use the OCSP validator module
    return is_ocsp_validation_enabled(ctx);
}
