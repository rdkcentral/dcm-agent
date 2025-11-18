#ifndef PRIVACY_MODE_H
#define PRIVACY_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Thunder JSON-RPC endpoint (override at compile time if needed) */
#ifndef JSONRPC_URL
#define JSONRPC_URL "http://127.0.0.1:9998/jsonrpc"
#endif

/* Result codes */
#ifndef OK
#define OK   0
#endif
#ifndef NOK
#define NOK -1
#endif

/* Privacy mode enumeration */
typedef enum {
    PRIVACY_MODE_UNKNOWN = 0,
    PRIVACY_MODE_SHARE,
    PRIVACY_MODE_DO_NOT_SHARE
} privacy_mode_t;

/* Retrieve security token from /usr/bin/WPEFrameworkSecurityUtility.
 * Returns heap string (caller must free) or NULL on failure.
 */
char* get_security_token(void);

/* Perform JSON-RPC POST to Thunder using Authorization: Bearer <token>.
 * postData: JSON string to send.
 * Returns heap string with full response body or NULL on failure.
 * Caller must free().
 */
char* getJsonRPCData(const char *postData);

/* Return heap string containing privacyMode (e.g. "SHARE", "DO_NOT_SHARE").
 * Returns NULL on error. Caller must free().
 */
char* get_privacy_mode_string(void);

/* Convenience: obtain privacy mode enum. Returns OK/NOK.
 * On success sets *outMode.
 */
int get_privacy_mode(privacy_mode_t *outMode);

/* Returns 1 if privacy mode is DO_NOT_SHARE, 0 otherwise (includes errors). */
int is_privacy_mode_do_not_share(void);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_MODE_H */
