#include "mtls_cert_selector.h"
#include "downloadUtil.h"
#ifdef LIBRDKCERTSELECTOR
#include "rdkcertselector.h"
#include <string.h>
#include <rdk_logger.h>


#define FILESCHEME "file://"
#define LOG_UPLOAD "LOG.RDK.UPLOAD"

MtlsAuthStatus loguploadGetCert(MtlsAuth_t *sec, rdkcertselector_h* pthisCertSel) {
    char *certUri = NULL;
    char *certPass = NULL;
    char *engine = NULL;
    char *certFile = NULL;

    rdkcertselectorStatus_t certStat = rdkcertselector_getCert(*pthisCertSel, &certUri, &certPass);

    if (certStat != certselectorOk || certUri == NULL || certPass == NULL) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Failed to retrieve certificate for MTLS\n", __FUNCTION__);
        rdkcertselector_free(pthisCertSel);
        if(*pthisCertSel == NULL){
            RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: Cert selector memory freed\n", __FUNCTION__);
        }else{
            RDK_LOG(RDK_LOG_ERROR, LOG_UPLOAD, "%s: Cert selector memory free failed\n", __FUNCTION__);
        }
        return MTLS_CERT_FETCH_FAILURE; // Return error
    }

    certFile = certUri;
    if (strncmp(certFile, FILESCHEME, sizeof(FILESCHEME)-1) == 0) {
        certFile += (sizeof(FILESCHEME)-1); // Remove file scheme prefix
    }

    strncpy(sec->cert_name, certFile, sizeof(sec->cert_name) - 1);
    sec->cert_name[sizeof(sec->cert_name) - 1] = '\0';

    strncpy(sec->key_pas, certPass, sizeof(sec->key_pas) - 1);
    sec->key_pas[sizeof(sec->key_pas) - 1] = '\0';

    engine = rdkcertselector_getEngine(*pthisCertSel);
    if (engine == NULL) {
        sec->engine[0] = '\0';
    } else {
        strncpy(sec->engine, engine, sizeof(sec->engine) - 1);
        sec->engine[sizeof(sec->engine) - 1] = '\0';
    }

    strncpy(sec->cert_type, "P12", sizeof(sec->cert_type) - 1);
    sec->cert_type[sizeof(sec->cert_type) - 1] = '\0';

    RDK_LOG(RDK_LOG_INFO, LOG_UPLOAD, "%s: MTLS dynamic/static cert success. cert=%s, type=%s, engine=%s\n",
            __FUNCTION__, sec->cert_name, sec->cert_type, sec->engine);
    return MTLS_CERT_FETCH_SUCCESS; // Return success
}
#endif
