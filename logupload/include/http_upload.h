#ifndef HTTP_UPLOAD_H
#define HTTP_UPLOAD_H

#include <curl/curl.h>
#include <stdbool.h>
#include <rdk_logger.h>
#include "rdk_logger_types.h"
#include "mtls_cert_selector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UPLOAD_SUCCESS = 0,
    UPLOAD_FAIL    = -1
} UploadStatus;

/* Optional hash headers (e.g., md5, timestamp) */
typedef struct {
    const char *hashvalue;   /* e.g., "x-md5: <value>" or custom header */
    const char *hashtime;    /* e.g., "x-upload-time: <iso8601>" */
} UploadHashData_t;

/* Upload request descriptor (metadata POST stage) */
typedef struct {
    const char *url;          /* Endpoint to POST to */
    const char *pathname;     /* Local file path (used for filename= parameter) */
    const char *pPostFields;  /* Additional POST fields (optional, appended) */
    int         sslverify;    /* 0/1 control for peer verification */
    UploadHashData_t *hashData; /* Optional hash headers */
} FileUpload_t;

int doHttpFileUpload(void *in_curl,
                     FileUpload_t *pfile_upload,
                     const MtlsAuth_t *auth,
                     long *out_httpCode);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_UPLOAD_H */
