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
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "md5_utils.h"
#include "uploadstblogs_types.h"
#include "rdk_debug.h"

/**
 * @brief Base64 encode binary data
 * @param input Binary data to encode
 * @param length Length of input data
 * @param output Buffer to store base64 encoded string
 * @param output_size Size of output buffer
 * @return true on success, false on failure
 */
static bool base64_encode(const unsigned char *input, size_t length, 
                         char *output, size_t output_size)
{
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // No newlines
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);
    
    if (buffer_ptr->length >= output_size) {
        BIO_free_all(bio);
        return false;
    }
    
    memcpy(output, buffer_ptr->data, buffer_ptr->length);
    output[buffer_ptr->length] = '\0';
    
    BIO_free_all(bio);
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
