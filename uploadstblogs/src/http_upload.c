#include "http_upload.h"
#include "rdk_debug.h"
#include <stdio.h>
#include <string.h>
#include "uploadUtil.h"
#include "mtls_upload.h"

/* All upload functionality has been moved to common_utilities/dwnlutils/ library.
 * This file now serves as a simple pass-through wrapper for backward compatibility.
 * Functions are implemented in:
 * - uploadUtil.c: Core HTTP/S3 upload operations
 * - mtls_upload.c: mTLS-enabled uploads with certificate rotation
 */
