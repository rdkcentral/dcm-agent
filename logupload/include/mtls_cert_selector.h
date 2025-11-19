#ifndef _MTLS_CERT_SELECTOR_H_
#define _MTLS_CERT_SELECTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CURL_MTLS_LOCAL_CERTPROBLEM 58

typedef struct {
    char cert_name[256];
    char key_pas[128];
    char cert_type[16];
    char engine[64];
} MtlsAuth_t;

typedef enum {
    MTLS_CERT_FETCH_FAILURE = -1,      // Indicates general MTLS failure
    MTLS_CERT_FETCH_SUCCESS = 0        // Indicates success
} MtlsAuthStatus;

#ifdef LIBRDKCERTSELECTOR
#include "rdkcertselector.h"
MtlsAuthStatus loguploadGetCert(MtlsAuth_t *sec, rdkcertselector_h* pthisCertSel);
#endif

#ifdef __cplusplus
}
#endif

#endif // _MTLS_CERT_SELECTOR_H_
