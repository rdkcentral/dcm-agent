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
 * @file mtls_cert_selector.h
 * @brief MTLS certificate selector interface
 *
 * Provides dynamic and static certificate selection for MTLS authentication
 * using the rdkcertselector library.
 */

#ifndef _MTLS_CERT_SELECTOR_H_
#define _MTLS_CERT_SELECTOR_H_

#include "downloadUtil.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CURL_MTLS_LOCAL_CERTPROBLEM 58

/**
 * @enum MtlsAuthStatus
 * @brief MTLS certificate fetch status codes
 */
typedef enum {
    MTLS_CERT_FETCH_FAILURE = -1,      /**< Indicates general MTLS failure */
    MTLS_CERT_FETCH_SUCCESS = 0        /**< Indicates success */
} MtlsAuthStatus;

#ifdef LIBRDKCERTSELECTOR
#include "rdkcertselector.h"

/**
 * @brief Retrieve MTLS certificate for log upload
 * @param sec MTLS authentication structure to populate
 * @param pthisCertSel Certificate selector handle
 * @return MTLS_CERT_FETCH_SUCCESS on success, MTLS_CERT_FETCH_FAILURE on failure
 *
 * Retrieves dynamic or static certificate, password, engine information
 * for MTLS authentication during log upload.
 */
MtlsAuthStatus loguploadGetCert(MtlsAuth_t *sec, rdkcertselector_h* pthisCertSel);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _MTLS_CERT_SELECTOR_H_ */
