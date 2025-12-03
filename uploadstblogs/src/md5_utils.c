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
 * @file md5_utils.c
 * @brief MD5 hash calculation utilities for file integrity
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/evp.h>
#include "md5_utils.h"
#include "uploadstblogs_types.h"
#include "rdk_debug.h"

/**
 * @brief Base64 encode binary data using simple implementation
 * @param input Binary data to encode
 * @param length Length of input data
 * @param output Buffer to store base64 encoded string
 * @param output_size Size of output buffer
 * @return true on success, false on failure
 */
static bool base64_encode(const unsigned char *input, size_t length, 
                         char *output, size_t output_size)
{
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = ((length + 2) / 3) * 4;
    
    if (output_length >= output_size) {
        return false;
    }
    
    size_t i, j;
    for (i = 0, j = 0; i < length; i += 3, j += 4) {
        uint32_t a = i < length ? input[i] : 0;
        uint32_t b = (i + 1) < length ? input[i + 1] : 0;
        uint32_t c = (i + 2) < length ? input[i + 2] : 0;
        
        uint32_t triple = (a << 16) | (b << 8) | c;
        
        output[j] = base64_chars[(triple >> 18) & 0x3F];
        output[j + 1] = base64_chars[(triple >> 12) & 0x3F];
        output[j + 2] = (i + 1) < length ? base64_chars[(triple >> 6) & 0x3F] : '=';
        output[j + 3] = (i + 2) < length ? base64_chars[triple & 0x3F] : '=';
    }
    
    output[output_length] = '\0';
    return true;
}

bool calculate_file_md5(const char *filepath, char *md5_base64, size_t output_size)
{
    if (!filepath || !md5_base64 || output_size < 25) { // MD5 base64 = 24 chars + null
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to open file: %s\n", __FUNCTION__, __LINE__, filepath);
        return false;
    }
    
    // Use modern EVP API instead of deprecated MD5 functions
    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to create MD5 context\n", __FUNCTION__, __LINE__);
        fclose(file);
        return false;
    }
    
    if (EVP_DigestInit_ex(md_ctx, EVP_md5(), NULL) != 1) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to initialize MD5 digest\n", __FUNCTION__, __LINE__);
        EVP_MD_CTX_free(md_ctx);
        fclose(file);
        return false;
    }
    
    unsigned char buffer[8192];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(md_ctx, buffer, bytes_read) != 1) {
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                    "[%s:%d] Failed to update MD5 digest\n", __FUNCTION__, __LINE__);
            EVP_MD_CTX_free(md_ctx);
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    
    unsigned char md5_binary[EVP_MAX_MD_SIZE];
    unsigned int md5_len;
    if (EVP_DigestFinal_ex(md_ctx, md5_binary, &md5_len) != 1) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Failed to finalize MD5 digest\n", __FUNCTION__, __LINE__);
        EVP_MD_CTX_free(md_ctx);
        return false;
    }
    
    EVP_MD_CTX_free(md_ctx);
    
    // Encode to base64 (matches script: openssl md5 -binary < file | openssl enc -base64)
    if (!base64_encode(md5_binary, md5_len, md5_base64, output_size)) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB,
                "[%s:%d] Base64 encoding failed\n", __FUNCTION__, __LINE__);
        return false;
    }
    
    RDK_LOG(RDK_LOG_DEBUG, LOG_UPLOADSTB,
            "[%s:%d] Calculated MD5 for %s: %s\n", 
            __FUNCTION__, __LINE__, filepath, md5_base64);
    
    return true;
}
