# Security Assessment Summary — dcm-agent-develop

**Assessment Date:** 2026-06-11
**Repository:** `dcm-agent-develop` (RDK Device Configuration Management Agent)
**Repository Type:** Single repository, 4 components (core-dcm, uploadstblogs, backup_logs, usbLogUpload)
**Language:** C
**Privilege Level:** root (uid 0) — `dcmd.service` has no User= directive; daemon calls `umask(0)`
**Production LOC:** 16,356
**Total LOC (incl. tests):** 31,256

---

## Executive Summary

dcm-agent (`dcmd`) is an RDK-V/RDK-B Device Configuration Management daemon that runs as ROOT (dcmd.service has no User= directive, and main() calls `umask(0)` so all created files are world-writable). It opens an RBUS IPC connection as component `T2TODCM`, subscribes to two inbound events (`Device.DCM.Setconfig`, `Device.DCM.Processconfig`) and registers one outbound event (`Device.X_RDKCENTREL-COM.Reloadconfig`). The Telemetry 2.0 daemon fetches a JSON config blob from the operator's XCONF cloud, writes it to a file (typically `/opt/.t2persistentfolder/DCMresponse.txt`), then publishes `Device.DCM.Setconfig` carrying the file PATH (key `dcmSetConfig`) followed by `Device.DCM.Processconfig`. The main loop reads the path via `dcmRbusGetConfPath()`, parses it with cJSON in `dcmSettingParseConf()`, extracts an upload URL / protocol / two cron strings, immediately invokes `uploadstblogs_run()` (libcurl mTLS/CodeBig HTTPS POST + S3 PUT of tar'd `/opt/logs`), and schedules two cron jobs (`DCM_LOG_UPLOAD`, `DCM_FW_UPDATE`). The FW-update job builds a shell command `/bin/sh <RDK_PATH>/swupdate_utility.sh ...` and runs it via `popen()`. Sub-binaries `usbLogUpload` (argv[1] = USB mount path, no canonicalization) and `backup_logs` (driven by `/etc/.../special_files.conf`) also run as root. The dominant attack surfaces are: (1) RBUS — any local process able to publish on rtrouted can set an arbitrary `confPath` and trigger processing; (2) the cloud-supplied JSON, which is copied with unbounded `strcpy()` into tiny fixed stack/heap buffers (`cUploadPrtl[8]`, `cTimeZone[16]`, `logCron[16]`, `difdCron[16]`, `cUploadURL[128]`); (3) `dcmRbusGetT2Version()` does `strcpy(t2_ver[32], rbus_string)` from a remote RBUS provider; (4) world-writable state files in `/tmp` (`/tmp/.dcm-daemon.pid`, `/tmp/DCMSettings.conf`, `/tmp/httpresults.txt`, `/tmp/.log-upload.lock`, `/tmp/.EnableOCSPStapling`); (5) the upload URL from XCONF directly controls where device logs (potentially containing PII / keys) are exfiltrated.

---

## Assessment Methodology

| Phase | Description | Output |
|---|---|---|
| 1. Threat Model | Architecture review, trust-boundary enumeration, privilege analysis | 8 trust boundaries, 14 input sources |
| 2. Input Tracing | Taint analysis from each external input to all sinks | 14 sources traced, 147 sinks mapped |
| 3. Targeted Vuln Hunt | Threat-model-driven review of 8 components | 77 raw findings |
| 4. Line-by-Line Review | Manual review of all 33 production .c files | 100 raw findings |
| 5. Adversarial Verification | Independent skeptical re-review of each deduped finding | 158 reviewed → 49 rejected as FP |
| 6. Synthesis | Severity adjustment, ranking, reporting | 109 confirmed findings |

**Workflow Stats:** 192 agents · 7,526,703 tokens · 166.4 min runtime · 2,166 tool calls
**Failed Jobs:** 0 (none — all agents completed)

---

## Findings Overview

| Severity | Count |
|---|---|
| **Critical** | 0 |
| **High** | 11 |
| **Medium** | 34 |
| **Low** | 42 |
| **Info** | 22 |
| **Total** | 109 |

### By Component

| Component | Critical | High | Medium | Low | Info | Total |
|---|---|---|---|---|---|---|
| backup_logs | 0 | 0 | 3 | 4 | 1 | 8 |
| core-dcm | 0 | 8 | 14 | 16 | 9 | 47 |
| uploadstblogs | 0 | 3 | 14 | 17 | 8 | 42 |
| usbLogUpload | 0 | 0 | 3 | 5 | 4 | 12 |

### Top CWE Categories

| CWE | Count | Description |
|---|---|---|
| CWE-59 | 17 | Link Following (Symlink) |
| CWE-377 | 8 | Insecure Temp File |
| CWE-367 | 6 | TOCTOU |
| CWE-787 | 5 | Out-of-bounds Write |
| CWE-457 | 4 | Use of Uninitialized Variable |
| CWE-918 | 4 | SSRF |
| CWE-22 | 4 | Path Traversal |
| CWE-362 | 4 |  |
| CWE-252 | 4 | Unchecked Return Value |
| CWE-73 | 3 | External Control of File Path |

---

## Findings Summary Table

| S.No | Finding ID | Title | Severity | Vulnerability | Weakness | Comments |
|------|-----------|-------|----------|--------------|----------|----------|
| 1 | DCM-001 | Unauthenticated, unvalidated config-file path accepted from RBUS Setconfig event | High | External Control of File Name or Path | CWE-73 | Any local RBUS publisher can set arbitrary confPath → root parses attacker-chosen file → heap overflow / RCE chain |
| 2 | DCM-002 | Shell command injection via unescaped cRdkPath in popen() | High | OS Command Injection | CWE-78 | Unescaped pRDKPath interpolated into popen(); reachable from XCONF/RBUS via adjacent-field heap overflow into cRdkPath |
| 3 | DCM-003 | Unbounded strcpy() of cloud-supplied JSON string values into tiny fixed heap buffers | High | Heap-based Buffer Overflow | CWE-122 | strcpy(sval, pJsonItem->valuestring) with no length check into cUploadPrtl[8], cTimeZone[16], cUploadURL[128], logCron[16], difdCron[16] |
| 4 | DCM-004 | Heap overflow of cTimeZone[16] corrupts adjacent cRdkPath → root command injection via popen() | High | OS Command Injection | CWE-78 | Overflow of cTimeZone[16] into cRdkPath[80] → shell metacharacters executed as root via popen(). Blocked when ENABLE_MAINTENANCE is set |
| 5 | DCM-005 | 1-byte destination stack buffer passed to strcpy() when UploadOnReboot is sent as a JSON string | High | Stack-based Buffer Overflow | CWE-121 | INT8 temp=0 (1 byte) passed to strcpy(); type mismatch (expected bool, got string) triggers stack smash |
| 6 | DCM-006 | popen() wrapper executes caller-supplied string via /bin/sh with no validation or escaping | High | OS Command Injection | CWE-78 | dcmUtilsCopyCommandOutput() passes cmd verbatim to popen(cmd,"r") with no sanitization; terminal sink for the highest-impact chain |
| 7 | DCM-007 | Static-buffer overflow in attempt_proxy_fallback() while parsing server-supplied S3 URL | High | Out-of-bounds Write | CWE-787 | strncpy into static char clean_path[512] with unbounded path_len (up to ~1013); .bss overflow in root process |
| 8 | DCM-008 | Unbounded strcpy of network-sourced JSON values into 8/16/128-byte struct fields | High | Classic Buffer Overflow | CWE-120 | Duplicate root cause of DCM-003; difdCron[16] is last DCMDHandle field → overflow runs off malloc chunk into heap metadata |
| 9 | DCM-009 | Stack overflow of t2_ver[32] from rbus-supplied string via strcpy | High | Classic Buffer Overflow | CWE-120 | strcpy(t2_ver, rbus_string) into 32-byte stack buffer; any RBUS provider returning ≥32 bytes triggers overflow in root daemon init |
| 10 | DCM-010 | Directory symlink following in add_directory_to_tar() allows arbitrary file exfiltration | High | Improper Link Resolution Before File Access | CWE-59 | stat() follows symlinks on directory entries under world-writable /tmp/DCM/; root-readable files archived and uploaded to cloud |
| 11 | DCM-011 | gzopen() on predictable path under /tmp without O_NOFOLLOW/O_EXCL enables arbitrary file overwrite | High | Insecure Temporary File | CWE-377 | Predictable archive filename in /tmp; symlink at target path → root truncates/overwrites arbitrary file with gzip data |
| 12 | DCM-012 | Stack buffer overflow in dcmRbusGetT2Version() via unbounded strcpy() of RBUS-supplied string | Medium | Stack-based Buffer Overflow | CWE-121 | Same root cause as DCM-009; fires only at daemon init, requires winning RBUS provider registration race |
| 13 | DCM-013 | popen() executed on uninitialised heap buffer when DCM_LOG_UPLOAD scheduler fires | Medium | Use of Uninitialized Variable | CWE-457 | malloc(1024) not zeroed; LOG_UPLOAD branch never writes pExecBuff but popen() called unconditionally |
| 14 | DCM-014 | umask(0) makes every file created by root daemon world-writable | Medium | Incorrect Permission Assignment for Critical Resource | CWE-732 | All created files mode 0666; enables config tampering, upload suppression, PID-file DoS |
| 15 | DCM-015 | Stack buffer overflow in dcmSettingSaveMaintenance() when cron minute/hour token ≥ 4 chars | Medium | Stack-based Buffer Overflow | CWE-121 | buffmin[4]/buffhr[4] written without bounds check; even legal cron fields like '*/15' overflow |

---

## Top 20 Findings by Impact/Severity

### 1. DCM-001 — Unauthenticated, unvalidated config-file path accepted from RBUS Setconfig event (arbitrary file parse as root → heap overflow / RCE chain)

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-73 |
| **Component** | core-dcm |
| **Location** | `dcm_rbus.c:88-94` |
| **Input Source** | RBUS event Device.DCM.Setconfig payload key 'dcmSetConfig' (any local process on rtrouted) |
| **Confidence** | High |

**Description:** The RBUS event handler rbusSetConf() (registered for 'Device.DCM.Setconfig') extracts the string value 'dcmSetConfig' from the event payload and copies it verbatim into pDCMRbusHandle->confPath[128] with no validation whatsoever — no realpath(), no allowed-directory prefix (e.g., '/opt/'), no '..' rejection, no symlink check, and no authentication of the RBUS sender. When the companion event 'Device.DCM.Processconfig' fires (rbusProcConf() sets schedJob=1), the main loop at dcm.c:363-373 reads this attacker-controlled path via dcmRbusGetConfPath() and passes it directly to dcmSettingParseConf(), which fopen()'s it as root and feeds the contents through unbounded strcpy() calls into 8/16/128-byte heap fields (dcm_parseconf.c:224). Any local process able to publish on rtrouted can therefore (a) point dcmd at an attacker-written /tmp/evil.json to fully control the parsed config and trigger heap overflows in the root daemon, (b) point at sensitive root-only files for read, or (c) point at /dev/* or a FIFO to hang the daemon.

**Attack Vector:** Local unprivileged process (or compromised sandboxed app) on the rtrouted bus publishes Device.DCM.Setconfig with payload {dcmSetConfig: "/tmp/evil.json"}, then publishes Device.DCM.Processconfig. dcmd (root) opens /tmp/evil.json and parses attacker JSON; oversized values for urn:settings:TimeZoneMode / uploadProtocol / cron strings overflow heap fields in DCMSettingsHandle/DCMDHandle, including overwriting cRdkPath[80] which is later embedded in a popen() shell command (dcm.c:109-113).

**Impact:** Local-to-root privilege escalation: attacker-chosen file is fopen()'d and parsed by root, feeding directly into unbounded strcpy() heap overflows and into the upload-URL/cron used by the daemon. Also enables arbitrary root file read (info disclosure) and DoS (point at FIFO / huge file).

**Verification Notes:** Code matches exactly: dcm_rbus.c:88-94 copies the RBUS event payload string into confPath[128] with zero validation, and dcm.c:363-373 passes it straight to dcmSettingParseConf() which fopen()'s it as root. The downstream heap-overflow chain is real — dcm_parseconf.c:224 does an unbounded `strcpy(sval, pJsonItem->valuestring)` into cUploadPrtl[8], cTimeZone[16], cUploadURL[128] (DCMSettingsHandle, dcm_parseconf.h:51-60) and into logCron[16]/difdCron[16] (DCMDHandle, dcm.h:41-42); cTimeZone[16] is laid out immediately before cRdkPath[80], so a >16-byte TimeZoneMode value overwrites cRdkPath, which is later interpolated into `snprintf(..., "/bin/sh %s/swupdate_utility.sh ...", pRDKPath)` and executed via popen() (dcm.c:109-113, dcm_utils.c:96 — plain popen, not v_secure_system). No realpath/prefix/symlink check, no sender authentication. The only mitigations found: (a) the popen branch is skipped when ENABLE_MAINTENANCE is set in /etc/device.properties (dcm.c:59-62), and (b) RBUS client-side element ownership means a naive librbus rbusEvent_Publish() from a non-provider fails — but rtrouted itself performs no peer authentication, so an attacker can register the element first (dcmd loops at dcm.c:334-344 waiting for *any* provider) or speak raw rtMessage. These reduce, but do not eliminate, exploitability.

**Exploitability:** Preconditions: attacker is a local process able to connect to the rtrouted unix socket (typically world-accessible on RDK) AND either (i) win the provider-registration race for Device.DCM.Setconfig/Processconfig before telemetry2.0 (or after crashing/restarting T2), or (ii) bypass librbus and inject raw rtMessage frames — RBUS publish-ownership is enforced only client-side, so the reporter's "any local process" is slightly overstated but achievable. Attacker must also be able to drop a JSON file at a path of their choosing (e.g. /tmp). The cleanest root-RCE chain (cTimeZone→cRdkPath→popen) additionally requires ENABLE_MAINTENANCE to be absent from /etc/device.properties; on MM-enabled devices the impact degrades to heap corruption of the root daemon and arbitrary-file-open (DoS via FIFO/blockdev, limited info-leak via dcm logs). Because exploitation is local-only, requires bus-level trickery, and the direct command-injection branch is config-gated, severity is adjusted from Critical to High.

```c
/* dcm_rbus.c — rbusSetConf() */
88     configPath = rbusObject_GetValue(event->data, DCM_SET_CONFIG);
89 
90     if(configPath) {
91         const INT8 *filePath = rbusValue_GetString(configPath, NULL);
92         if(filePath != NULL) {
93             strncpy(pDCMRbusHandle->confPath, filePath, DCM_CONF_SIZE - 1);
94             pDCMRbusHandle->confPath[DCM_CONF_SIZE - 1] = '\0';
95             DCMInfo("configPath: %s\n", filePath);

/* dcm.c — main while(1) */
363            pconfPath = dcmRbusGetConfPath(g_pdcmHandle->pRbusHandle);
...
371            ret = dcmSettingParseConf(g_pdcmHandle->pDcmSetHandle, pconfPath,
372                                      g_pdcmHandle->logCron,
373                                      g_pdcmHandle->difdCron);
```

---

### 2. DCM-002 — Shell command injection via unescaped cRdkPath in popen() (reachable from XCONF/RBUS via adjacent-field heap overflow)

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-78 |
| **Component** | core-dcm |
| **Location** | `dcm.c:109-113` |
| **Input Source** | Cloud XCONF JSON / RBUS-supplied confPath file (TimeZoneMode field overflowing into cRdkPath); secondarily /etc/include.properties RDK_PATH |
| **Confidence** | High |

**Description:** When the DCM_FW_UPDATE scheduler fires, dcmRunJobs() builds a shell command with snprintf(pExecBuff, 1024, "/bin/sh %s/swupdate_utility.sh 0 2 >> ...", pRDKPath) and passes it to dcmUtilsSysCmdExec() → popen(). pRDKPath = pdcmSetHandle->cRdkPath is interpolated with no quoting or shell-metacharacter filtering. cRdkPath is nominally read from /etc/include.properties (root-only), BUT in the DCMSettingsHandle struct (dcm_parseconf.h:51-58) cRdkPath[80] is laid out immediately after cTimeZone[16], which is filled by an unbounded strcpy() of the cloud/RBUS-supplied JSON value 'urn:settings:TimeZoneMode' (dcm_parseconf.c:224). A TimeZoneMode value > 16 bytes overflows directly into cRdkPath, letting an attacker who controls the DCM JSON (XCONF backend, MITM, or local RBUS publisher pointing confPath at /tmp/evil.json) inject shell metacharacters that are then executed as root by popen().

**Attack Vector:** Attacker supplies DCM JSON with {"urn:settings:TimeZoneMode":"AAAAAAAAAAAAAAAA/tmp;id>/tmp/pwn;#","urn:settings:CheckSchedule:cron":"* * * * *"}. dcmSettingJsonGetVal() strcpy()'s this into cTimeZone[16], overflowing into cRdkPath. When the FW_UPDATE cron fires, dcmRunJobs() builds '/bin/sh /tmp;id>/tmp/pwn;#/swupdate_utility.sh ...' and popen()'s it as root. Alternative direct vector: anyone able to write /etc/include.properties (firmware/overlay tamper) sets RDK_PATH='/tmp;cmd;#'.

**Impact:** Arbitrary command execution as root (uid 0). Reachable remotely from the operator XCONF cloud (or MITM of it) and locally from any unprivileged RBUS publisher.

**Verification Notes:** The chain is fully confirmed in source. dcm.c:109 builds `"/bin/sh %s/swupdate_utility.sh ..."` with pRDKPath unescaped and dcm.c:113 → dcm_utils.c:128 → dcm_utils.c:96 passes it to popen() (no v_secure_system). pRDKPath is pdcmSetHandle->cRdkPath (dcm_parseconf.c:687-697), which sits immediately after cTimeZone[16] in DCMSettingsHandle (dcm_parseconf.h:56-57, both INT8 arrays so no padding). dcm_parseconf.c:551 calls dcmSettingJsonGetVal() with sval=cTimeZone, and line 224 does an unbounded `strcpy(sval, pJsonItem->valuestring)` of the cloud-supplied "urn:settings:TimeZoneMode" string — a value ≥16 bytes overwrites cRdkPath with attacker bytes and a clean NUL terminator. This parse happens in main()'s while(1) loop (dcm.c:371) AFTER dcmSettingsInit() seeded cRdkPath from /etc/include.properties (dcm.c:162 → dcm_parseconf.c:759) and BEFORE dcmSchedStartJob(pDifdSchedHandle, difdCron) (dcm.c:376) arms the cron that invokes dcmRunJobs(), so the corrupted value is what reaches popen(). I found one partial mitigation the reporter understated: dcmRunJobs() returns early at dcm.c:59-62 when g_bMMEnable is set (ENABLE_MAINTENANCE present in /etc/device.properties, dcm_parseconf.c:764-770), so devices with Maintenance Manager are not exploitable via this sink — but devices without it are.

**Exploitability:** Preconditions: (a) target device does NOT have ENABLE_MAINTENANCE in /etc/device.properties (else dcmRunJobs() short-circuits before popen); (b) attacker can influence the DCM JSON consumed by dcmSettingParseConf. Attacker positions that satisfy (b): (1) control of / compromise of the operator XCONF backend that T2 fetches and hands to DCM via RBUS — yields fleet-wide root RCE (defense-in-depth failure: a config string becomes shell); (2) any local process able to act as RBUS provider/publisher for Device.DCM.Setconfig / Device.DCM.Processconfig (dcm_rbus.c:88-99) can point confPath at /tmp/evil.json — local non-root → root privesc, constrained by RBUS provider-registration semantics (must beat or replace telemetry2.0 as the event provider). Mitigating factors: production XCONF traffic is normally mTLS-protected so blind network MITM is hard; the secondary /etc/include.properties RDK_PATH vector requires writing a root-owned file on read-only squashfs and is not independently interesting. Net: realistic High (root popen() injection reachable from externally-sourced JSON via an adjacent-field strcpy overflow), not Critical because the strongest remote vector presumes trusted-infra compromise and a per-device config flag gates the sink.

```c
/* dcm.c — dcmRunJobs */
64     INT8 *pRDKPath = dcmSettingsGetRDKPath(pdcmHandle->pDcmSetHandle);
...
107    else if(strcmp(profileName, DCM_DIFD_SCHED) == 0) {
108        DCMInfo("Start FW update Script\n");
109        snprintf(pExecBuff, EXECMD_BUFF_SIZE, "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
110                                               pRDKPath);   /* no shell escaping */
111    }
112
113    dcmUtilsSysCmdExec(pExecBuff);   /* → popen(cmd, "r") at dcm_utils.c:96 */

/* dcm_parseconf.h — adjacent layout */
55     INT8  cUploadPrtl[8];
56     INT8  cTimeZone[16];
57     INT8  cRdkPath[MAX_DEVICE_PROP_BUFF_SIZE];   /* 80 — overwrite target */
```

---

### 3. DCM-003 — Unbounded strcpy() of cloud-supplied JSON string values into tiny fixed heap buffers

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-122 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:222-224` |
| **Input Source** | XCONF cloud JSON (network) or local RBUS event Device.DCM.Setconfig (IPC) -> file at confPath |
| **Confidence** | High |

**Description:** dcmSettingJsonGetVal() copies the JSON string value with an unbounded strcpy() into a caller-supplied buffer whose size is never communicated. The five callers in dcmSettingParseConf() pass: cUploadPrtl[8] (line 532), cUploadURL[128] (line 542), cTimeZone[16] (line 551), g_pdcmHandle->logCron[16] (line 565) and g_pdcmHandle->difdCron[16] (line 573). All five live inside heap-allocated structs (DCMSettingsHandle and DCMDHandle). The JSON is the XCONF DCM response delivered by the cloud, or any file an RBUS publisher points at via Device.DCM.Setconfig. cJSON imposes no length limit on string values, so a >7-byte uploadProtocol, >127-byte URL, >15-byte TimeZoneMode or >15-byte cron string overflows adjacent struct members and, for difdCron[16] (the last member of DCMDHandle), runs off the end of the malloc chunk into heap metadata.

**Attack Vector:** Operator-side / MITM attacker who controls the XCONF DCM JSON, OR any local process on the RBUS bus that publishes Device.DCM.Setconfig with confPath pointing at an attacker-written /tmp/evil.json, then publishes Device.DCM.Processconfig. Crafting any of the urn:settings:* string keys longer than the destination buffer triggers heap corruption inside a uid-0 daemon.

**Impact:** Heap buffer overflow in root daemon -> heap-metadata corruption / adjacent-field corruption -> likely RCE as root.

**Verification Notes:** The unbounded `strcpy(sval, pJsonItem->valuestring)` at dcm_parseconf.c:224 is confirmed, and all five callers pass tiny fixed buffers inside heap-allocated structs: cUploadPrtl[8], cUploadURL[128], cTimeZone[16] in `DCMSettingsHandle` (malloc'd at dcm_parseconf.c:749) and logCron[16]/difdCron[16] in `DCMDHandle` (malloc'd at dcm.c:271), with difdCron being the last member so overflow runs off the chunk. The input path is real: rbusSetConf() (dcm_rbus.c:64-103) stores an RBUS-supplied file path, rbusProcConf() arms the loop, and dcm.c:371 calls dcmSettingParseConf() on that file with no length validation of the JSON values. The only mitigation the reporter missed is that dcmSettingJsonInit() reads the JSON via `fgets(pcJsonStr, DCM_JSON_STRSIZE=2048, fpin)` (dcm_parseconf.c:141), capping the whole document at 2047 bytes — this bounds the overflow to ~1900 bytes per field but does NOT prevent overflowing 8/16/128-byte buffers, and is ample for heap-metadata or adjacent-object corruption. Notably even a legitimate cron expression such as "0 0,6,12,18 * * *" (17 bytes) overflows logCron[16]/difdCron[16].

**Exploitability:** Preconditions: attacker must control the JSON file that dcmd parses. Two realistic positions: (1) control/compromise of the operator XCONF server (or MITM of Telemetry2's fetch if cert validation is weak — outside this repo), since T2 downloads the DCM JSON and publishes its path via Device.DCM.Setconfig; (2) any local process able to reach the rtrouted RBUS socket can publish Device.DCM.Setconfig with a path to an attacker-written /tmp JSON and then Device.DCM.Processconfig, yielding local non-root → root heap corruption. Constraints: payload bytes cannot contain NUL (strcpy) and total JSON line ≤2047 bytes; daemon runs as uid 0 with umask(0). Downgraded from Critical to High because the network vector requires a privileged operator-infrastructure position rather than remote-unauth, and the local vector still requires on-device RBUS access; nonetheless it is a genuine attacker-reachable heap overflow in a root daemon.

```c
    else if(cJSON_IsString(pJsonItem)) {
        *type = DCM_JSONITEM_STR;
        strcpy(sval, pJsonItem->valuestring);
    }
...
// callers (dcm_parseconf.c):
ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_PROTOCOL,
                           pUploadprtl, &confIntVal, &type);   // pUploadprtl = cUploadPrtl[8]
ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_URL,
                           pUploadURL, &confIntVal, &type);    // pUploadURL  = cUploadURL[128]
ret = dcmSettingJsonGetVal(pJsonHandle, DCM_TIMEZONE,
                           pTimezone, &confIntVal, &type);     // pTimezone   = cTimeZone[16]
ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_CRON,
                           pLogCron, &confIntVal, &type);      // pLogCron    = logCron[16]
ret = dcmSettingJsonGetVal(pJsonHandle, DCM_DIFD_CRON,
                           pDifdCron, &confIntVal, &type);     // pDifdCron   = difdCron[16]
```

---

### 4. DCM-004 — Heap overflow of cTimeZone[16] corrupts adjacent cRdkPath -> root command injection via popen()

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-78 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:224 (overflow), dcm_parseconf.h:53-58 (layout), dcm.c:109-113 (sink)` |
| **Input Source** | XCONF cloud JSON 'urn:settings:TimeZoneMode' (network) or local RBUS confPath file |
| **Confidence** | High |

**Description:** In DCMSettingsHandle the field ordering is cUploadURL[128], cUploadPrtl[8], cTimeZone[16], cRdkPath[80]. The unbounded strcpy() at dcm_parseconf.c:224 lets the cloud-supplied 'urn:settings:TimeZoneMode' (or uploadProtocol/URL) overflow directly into cRdkPath. cRdkPath is later returned by dcmSettingsGetRDKPath() and embedded unescaped into '/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1' (dcm.c:109) and executed via popen() (dcmUtilsSysCmdExec -> dcmUtilsCopyCommandOutput, dcm_utils.c:96) when the DCM_FW_UPDATE cron job fires. By supplying e.g. TimeZoneMode = 'AAAAAAAAAAAAAAAA/tmp;sh /tmp/x;#' the attacker controls the shell command line executed as root.

**Attack Vector:** Compromised/malicious XCONF backend, network MITM of the DCM config download, or any local RBUS publisher who plants a crafted JSON and triggers Device.DCM.Processconfig. The DCM_FW_UPDATE schedule (urn:settings:CheckSchedule:cron) is also attacker-controlled in the same JSON, so the popen() can be triggered immediately. Only blocked when ENABLE_MAINTENANCE is set in /etc/device.properties (g_bMMEnable==1 short-circuits dcmRunJobs).

**Impact:** Remote (operator-cloud / MITM) or local-IPC root command execution.

**Verification Notes:** Code matches the claim end-to-end. dcm_parseconf.c:224 does an unbounded `strcpy(sval, pJsonItem->valuestring)` where sval points to cTimeZone[16] (call at dcm_parseconf.c:551), which sits immediately before cRdkPath[80] in DCMSettingsHandle (dcm_parseconf.h:53-58). Nothing re-initialises cRdkPath after the parse (dcmSettingsInit runs only once at boot), so the overflowed bytes persist. dcm.c:64/109 reads cRdkPath via dcmSettingsGetRDKPath() and embeds it unescaped in `snprintf(pExecBuff, 1024, "/bin/sh %s/swupdate_utility.sh ...")`, then dcm_utils.c:96 executes it with `popen(cmd,"r")` as root. The DCM_DIFD_CRON schedule that triggers this callback is taken from the same attacker-controlled JSON (dcm_parseconf.c:573, dcm.c:376). _FORTIFY_SOURCE cannot help because the destination is an `INT8*` parameter (object size unknown at the strcpy site). The only in-code gate is dcm.c:59-62: if /etc/device.properties contains ENABLE_MAINTENANCE the callback returns early — a per-device-model build property, not a universal mitigation.

**Exploitability:** Preconditions: (a) attacker controls the XCONF DCM JSON that T2 downloads and hands to dcmd via RBUS Device.DCM.Setconfig/Processconfig — i.e. compromised/rogue operator XCONF backend or MITM of that fetch; OR (b) any local process able to publish those two RBUS events can point confPath at a file it writes in /tmp (local privesc to root). The 2048-byte fgets cap on the JSON is irrelevant — a ~100-byte payload `{"urn:settings:TimeZoneMode":"AAAAAAAAAAAAAAAA;sh /tmp/x #","urn:settings:CheckSchedule:cron":"* * * * *"}` suffices. Mitigating factors: blocked when ENABLE_MAINTENANCE is set in /etc/device.properties (g_bMMEnable==1); production XCONF is normally HTTPS to operator infrastructure, so the network vector is "trusted-backend compromise" rather than arbitrary-internet, hence High rather than Critical.

```c
// dcm_parseconf.h
typedef struct _dcmSettingsHandle {
    INT8  cJsonStr[DCM_JSON_STRSIZE];
    INT8  cUploadURL[MAX_URL_SIZE];   // 128
    INT8  cUploadPrtl[8];
    INT8  cTimeZone[16];
    INT8  cRdkPath[MAX_DEVICE_PROP_BUFF_SIZE]; // 80  <-- overwritten
    ...
} DCMSettingsHandle;

// dcm.c::dcmRunJobs
INT8 *pRDKPath = dcmSettingsGetRDKPath(pdcmHandle->pDcmSetHandle);
...
snprintf(pExecBuff, EXECMD_BUFF_SIZE, "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
                                       pRDKPath);
...
dcmUtilsSysCmdExec(pExecBuff);   // -> popen(cmd,"r")
```

---

### 5. DCM-005 — 1-byte destination stack buffer passed to strcpy() when UploadOnReboot is sent as a JSON string

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-121 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:508, 560-561, 224` |
| **Input Source** | XCONF cloud JSON (network) or local RBUS confPath file |
| **Confidence** | High |

**Description:** dcmSettingParseConf() declares `INT8 temp = 0;` (a single signed char on the stack) and passes `&temp` as the `sval` output buffer for key DCM_LOGUPLOAD_REBOOT (urn:settings:LogUploadSettings:UploadOnReboot). The code expects this key to be a JSON boolean, but dcmSettingJsonGetVal() does not enforce the expected type: if the cloud (or local RBUS attacker) sends this key as a STRING, the cJSON_IsString branch executes `strcpy(sval, pJsonItem->valuestring)` with sval = &temp. Any string of length >=1 overflows the 1-byte stack slot, corrupting adjacent local variables (type, uploadCheck, pointer locals pUploadURL/pUploadprtl/pTimezone, and ultimately saved registers / canary / return address) of a root process.

**Attack Vector:** Send {"urn:settings:LogUploadSettings:UploadOnReboot":"<long payload>"} in the DCM JSON. Reachable from compromised XCONF cloud, network MITM of the DCM response, or any local process publishing Device.DCM.Setconfig with confPath pointing at an attacker-written file then Device.DCM.Processconfig.

**Impact:** Stack buffer overflow in root daemon -> control-flow hijack / RCE as root, or at minimum corruption of adjacent pointer locals leading to crash.

**Verification Notes:** Code matches exactly: dcm_parseconf.c:508 declares `INT8 temp = 0;` (one byte), line 560-561 passes `&temp` as the `sval` out-buffer for key DCM_LOGUPLOAD_REBOOT, and dcmSettingJsonGetVal() at line 222-224 unconditionally does `strcpy(sval, pJsonItem->valuestring)` whenever the JSON value is a string — there is no type enforcement and no length check anywhere in the chain. The JSON originates from an external file whose path arrives via the RBUS `Device.DCM.Setconfig` event (dcm_rbus.c:88-94, subscribed at dcm_rbus.c:430-445) and is parsed on `Device.DCM.Processconfig` (dcm.c:360-373); that file is the XCONF cloud response written by telemetry2, and per the daemon's umask(0) the on-disk copies (/tmp/DCMSettings.conf, /opt/.DCMSettings.conf) are world-writable, so even a local non-root attacker can inject `"urn:settings:LogUploadSettings:UploadOnReboot":"AAAA…"` into the file before parsing. I found no earlier validation, no #ifdef guard, and no v_secure_* wrapper on this path. I downgrade from Critical to High because the "remote" vector requires controlling or MITM'ing the operator's XCONF backend (not an unauthenticated Internet attacker), and RDK Yocto builds normally inject -fstack-protector-strong (this function takes the address of a local, so a canary will be emitted), which in practice converts the overflow into a root-daemon abort/DoS rather than guaranteed RCE — though adjacent-local corruption (pJsonHandle, type, uploadCheck) still occurs before the canary check.

**Exploitability:** Preconditions: attacker must influence the DCM settings JSON consumed by dcmd. Three realistic positions: (1) compromise/MITM of the XCONF cloud response fetched by telemetry2 (requires breaking TLS or operator-side access); (2) any local process on the device that can write to the world-writable /tmp/DCMSettings.conf or /opt/.DCMSettings.conf (created mode 0666 due to umask(0)) before dcmd re-parses it — yields local-non-root → root memory corruption; (3) a local process able to register/publish the Device.DCM.Setconfig / Device.DCM.Processconfig RBUS events pointing confPath at an attacker-authored file. Mitigating factors: stack canary likely present (address-of-local triggers -fstack-protector-strong), ASLR/NX on RDK targets, and the legitimate XCONF server emits this key as a JSON boolean so the bug is not hit in normal operation. No length or type validation mitigates the strcpy itself.

```c
    INT8   temp          = 0;     // single byte on the stack
    INT32  type          = 0;
    INT32  uploadCheck   = 0;
    ...
    ret = dcmSettingJsonGetVal(pJsonHandle, DCM_LOGUPLOAD_REBOOT,
                               &temp, &uploadCheck, &type);

// dcmSettingJsonGetVal:
    else if(cJSON_IsString(pJsonItem)) {
        *type = DCM_JSONITEM_STR;
        strcpy(sval, pJsonItem->valuestring);   // sval == &temp (1 byte)
    }
```

---

### 6. DCM-006 — popen() wrapper executes caller-supplied string via /bin/sh with no validation or escaping

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-78 |
| **Component** | core-dcm |
| **Location** | `dcm_utils.c:89-110` |
| **Input Source** | /etc/include.properties RDK_PATH key (config file); secondarily XCONF-cloud / RBUS-supplied JSON via heap-adjacent overflow into cRdkPath |
| **Confidence** | High |

**Description:** dcmUtilsCopyCommandOutput() (and its thin wrapper dcmUtilsSysCmdExec() at line 119-131) passes its `cmd` argument verbatim to popen(cmd,"r"), which spawns `/bin/sh -c cmd`. The function performs no whitelisting, no shell-metacharacter escaping, and no NULL/empty-string check on `cmd` (only the wrapper checks for NULL, not for empty or non-printable content). The sole production caller is dcm.c:113 inside dcmRunJobs(), which builds the command as `snprintf(pExecBuff,1024,"/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1", pRDKPath)`. `pRDKPath` is the value of `RDK_PATH` read from /etc/include.properties (dcm_parseconf.c:759) and is interpolated unquoted. Additionally, `pRDKPath` points at DCMSettingsHandle.cRdkPath[80] which sits immediately after cTimeZone[16] in the heap struct; the unbounded strcpy() of the cloud-supplied JSON `urn:settings:TimeZoneMode` (dcm_parseconf.c:224) can overwrite cRdkPath with attacker bytes that then reach this popen() — turning a remote heap overflow into root command execution.

**Attack Vector:** (a) Direct: an attacker who can modify /etc/include.properties (firmware tamper, overlay mount, or another file-write bug) sets `RDK_PATH=/tmp;id>/tmp/pwn;#` → when the DCM_DIFD_SCHED cron fires, popen runs `/bin/sh /tmp;id>/tmp/pwn;#/swupdate_utility.sh ...` as root. (b) Indirect: a malicious XCONF server or local RBUS publisher supplies a >16-byte `urn:settings:TimeZoneMode` JSON value; the strcpy at dcm_parseconf.c:224 overflows cTimeZone[16] into adjacent cRdkPath[80]; the next FW-update cron tick then popen()'s `/bin/sh <attacker-bytes>/swupdate_utility.sh ...` as root.

**Impact:** Root remote/local code execution. The popen() runs with uid 0; any shell metacharacters in the interpolated path execute arbitrary commands. This is the terminal sink for the highest-impact chain in the daemon (cloud JSON → heap overflow → cRdkPath → popen).

**Verification Notes:** Code matches exactly: dcm_utils.c:96 calls popen(cmd,"r") with no sanitization, and the sole production caller dcm.c:109-113 builds the command via snprintf("/bin/sh %s/swupdate_utility.sh ...", pRDKPath) with pRDKPath interpolated unquoted. Vector (a) (/etc/include.properties RDK_PATH) is weak — that file is root-owned/read-only on RDK, so no trust boundary is crossed. Vector (b) however is fully verified: DCMSettingsHandle places cTimeZone[16] immediately before cRdkPath[80] (dcm_parseconf.h:56-57), the struct is malloc'd (dcm_parseconf.c:749), and dcmSettingJsonGetVal() does an unbounded strcpy(sval, pJsonItem->valuestring) at dcm_parseconf.c:224 for the cloud-supplied "urn:settings:TimeZoneMode" key (called at dcm_parseconf.c:551). cRdkPath is populated only once at startup (dcmSettingsInit, dcm.c:162) and never refreshed, so the overflow persists; the attacker also controls the DIFD cron string (urn:settings:CheckSchedule:cron) that triggers dcmRunJobs. No v_secure_system, no quoting, no length check anywhere in the chain.

**Exploitability:** Preconditions: (1) device must NOT have ENABLE_MAINTENANCE set in /etc/device.properties (dcm.c:59 early-returns if MM is enabled); (2) attacker must be able to influence the JSON parsed by dcmSettingParseConf — achievable by (i) any local process that can publish the RBUS event Device.DCM.Setconfig pointing confPath at an attacker-written file (RBUS/rtrouted has no per-event ACLs on RDK-V), or (ii) a malicious/compromised XCONF server or TLS MITM, since T2 fetches DCMSettings from XCONF and forwards the path to dcmd. Payload: TimeZoneMode = 16 filler bytes + ";<cmd>;#" overwrites cRdkPath; set CheckSchedule:cron to fire promptly; resulting popen runs "/bin/sh ;<cmd>;#/swupdate_utility.sh ..." as uid 0. Mitigating factors: XCONF traffic is normally HTTPS to operator infra, so the truly remote variant requires operator-side compromise; vector (a) by itself (editing /etc/include.properties) already requires root and would be Low standalone. Net: local-IPC privesc to root and operator-cloud→fleet root RCE justify High, not Critical.

```c
VOID dcmUtilsCopyCommandOutput (INT8 *cmd, INT8 *out, INT32 len)
{
    FILE *fp;

    if(out != NULL)
        out[0] = 0;

    fp = popen (cmd, "r");
    if (fp) {
        if(out) {
            if (fgets (out, len, fp) != NULL) { ... }
        }
        pclose (fp);
    }
    ...
}
```

---

### 7. DCM-007 — Static-buffer overflow in attempt_proxy_fallback() while parsing server-supplied S3 URL

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-787 |
| **Component** | uploadstblogs |
| **Location** | `uploadstblogs/src/path_handler.c:261-311` |
| **Input Source** | network response (HTTP body of metadata POST written to /tmp/httpresults.txt), or local /tmp file tamper |
| **Confidence** | High |

**Description:** attempt_proxy_fallback() reads up to 1023 bytes of the metadata-server's HTTP response from /tmp/httpresults.txt into char s3_url[1024] via fgets(). It then computes path_len = query_start - path_start and does `strncpy(clean_path, path_start, path_len); clean_path[path_len]='\0';` where clean_path is `static char clean_path[512]`. path_len is never bounded against sizeof(clean_path); for a response of the form `https://h/<~1000 bytes>?x` path_len reaches ~1013, overflowing clean_path by ~500 attacker-controlled bytes plus an out-of-bounds NUL write at clean_path[path_len]. clean_path is in .bss of a root process (uploadstblogs runs inside dcmd which is uid 0). Reachable when device_type=='mediaclient', PROXY_BUCKET is set in /etc/device.properties, and the primary S3 PUT failed — all of which the same attacker can arrange (return a presigned URL whose host rejects the PUT). Also reachable purely locally: any unprivileged user can overwrite /tmp/httpresults.txt between perform_metadata_post() and attempt_proxy_fallback().

**Attack Vector:** (Network) Compromised/malicious log-upload metadata server returns an HTTPS body 'https://h/' + 'A'*1000 + '?x'. (Local) Unprivileged user writes the same string to world-writable /tmp/httpresults.txt during the upload window. perform_s3_put_with_fallback() fails the PUT (HTTP!=200), calls attempt_proxy_fallback(), which fgets() the line and overflows static char clean_path[512] inside the root process.

**Impact:** Memory corruption (BSS overflow) in a uid-0 process from a network response or local /tmp tamper → potential RCE-as-root / DoS.

**Verification Notes:** Code at /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/uploadstblogs/src/path_handler.c:261-311 matches the report exactly: fgets() reads up to 1023 bytes into s3_url[1024], path_len = query_start - path_start is computed with no upper bound, and strncpy(clean_path, path_start, path_len) plus clean_path[path_len]='\0' write into a static char[512]. For input "https://h/" + 'A'*1010 + "?x", path_len ≈ 1011, overflowing .bss by ~500 attacker-controlled bytes. No earlier length check exists; the only later check (lines 314-323) on proxy_url size happens after the corruption. CFLAGS in Makefile.am contain only -Wall (no _FORTIFY_SOURCE), and even if the platform toolchain injects FORTIFY, the direct index write clean_path[path_len]='\0' is not fortified. The function is reached via execute_direct_path → perform_s3_put_with_fallback (line 574) when the S3 PUT fails; the file is the raw HTTP body written by performMetadataPostWithCertRotationEx into world-writable /tmp/httpresults.txt (dcmd runs as root with umask(0)). libuploadstblogs is linked into the root dcmd daemon (Makefile.am:39).

**Exploitability:** Preconditions: device_type=="mediaclient" and PROXY_BUCKET set in /etc/device.properties (fixed, root-owned device config — attacker cannot set these, but they are standard on RDK-V mediaclient/Xi boxes), and the S3 PUT must fail. Network vector: the metadata endpoint is contacted over mTLS (performMetadataPostWithCertRotationEx), so an arbitrary on-path attacker cannot inject the response; the attacker must compromise the operator's log-upload metadata server or control the TR-181 LogUploadEndpoint.URL — this converts trusted-server compromise into root .bss memory corruption across the mediaclient fleet. Local vector: any non-root process can overwrite /tmp/httpresults.txt (and /tmp/logupload_curl_info to force verify_upload() failure) during the wide network-call window between perform_metadata_post/performS3PutWithCert and attempt_proxy_fallback's fgets() re-read — a reliable local-to-root corruption primitive. Mitigating factors: .bss overflow exploitability for RCE depends on adjacent static layout in libuploadstblogs.so (unknown without the compiled artifact); if the platform Yocto build enables _FORTIFY_SOURCE=2, the strncpy becomes an abort (DoS of root dcmd) rather than controlled overwrite. High is appropriate; not Critical because the unauthenticated-network path is blocked by mTLS and the device-type/PROXY_BUCKET gate limits the affected population.

```c
char s3_url[1024] = {0};
...
FILE* result_file = fopen(results_file, "r");
if (!result_file || !fgets(s3_url, sizeof(s3_url), result_file)) { ... }
...
char* path_start = strchr(bucket_start, '/');
char* query_start = strchr(bucket_start, '?');
...
} else if (query_start && path_start && query_start > path_start) {
    // Remove query parameters from path
    size_t path_len = query_start - path_start;          /* up to ~1013 */
    static char clean_path[512];
    strncpy(clean_path, path_start, path_len);            /* OOB write */
    clean_path[path_len] = '\0';                          /* OOB NUL */
    path_part = clean_path;
}
```

---

### 8. DCM-008 — Unbounded strcpy of network-sourced JSON values into 8/16/128-byte struct fields

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-120 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:224` |
| **Input Source** | network response (XCONF JSON) / config file path delivered via rbus |
| **Confidence** | High |

**Description:** dcmSettingJsonGetVal() copies cJSON string values with strcpy(sval, pJsonItem->valuestring) with no length check. Callers in dcmSettingParseConf() pass tiny fixed buffers: cUploadPrtl[8] (line 533), cUploadURL[128] (line 543), cTimeZone[16] (line 552), and — most critically — g_pdcmHandle->logCron[16] (line 566) and g_pdcmHandle->difdCron[16] (line 574), the last field of the heap-allocated DCMDHandle. The JSON file (DCMresponse.txt / conf path delivered over rbus) contains values fetched from the XCONF cloud server. A cron expression such as "0,15,30,45 * * * *" (18 bytes) already overflows the 16-byte cron buffers; an attacker controlling the XCONF response or the on-disk conf file can write arbitrary lengths past the DCMDHandle/DCMSettingsHandle heap allocations.

**Attack Vector:** MITM or compromise of XCONF server response, write access to /opt/.t2persistentfolder/DCMresponse.txt, or any rbus client able to set Device.DCM conf path to an attacker-controlled file. Daemon parses it as root.

**Impact:** Heap buffer overflow in root daemon -> remote code execution / memory corruption.

**Verification Notes:** Confirmed verbatim: dcm_parseconf.c:224 does `strcpy(sval, pJsonItem->valuestring)` with no bound, and dcmSettingParseConf() (lines 522-574) passes cUploadPrtl[8], cUploadURL[128], cTimeZone[16] from the malloc'd DCMSettingsHandle, plus logCron[16] and difdCron[16] which are the trailing fields of the malloc'd DCMDHandle (dcm.c:271, dcm.h:41-42) — overflowing difdCron writes past the heap chunk. The only upstream limiter is fgets(...,2048,...) in dcmSettingJsonInit (line 141), which caps the whole JSON to 2 KB but still allows any single string value to far exceed 8/16/128 bytes; no per-field length check exists anywhere in the chain. Input is genuinely external: the file path arrives via rbus event Device.DCM.Setconfig (dcm_rbus.c:64-103, subscribed at :430) and the file content is the XCONF cloud response written by telemetry2; any rbus client can also publish Setconfig pointing at an attacker-authored file and then Processconfig to trigger parsing in the root daemon.

**Exploitability:** Two realistic vectors: (1) Local — any process able to reach the rbus broker (rtrouted Unix socket, no per-event ACL in stock rbus) can publish Device.DCM.Setconfig with a path to a /tmp JSON containing e.g. a 500-byte "urn:settings:CheckSchedule:cron" value, then publish Device.DCM.Processconfig; dcmd (root) heap-overflows DCMDHandle → LPE/DoS. (2) Remote — controlling the XCONF JSON body; this channel is normally HTTPS+mTLS to operator infrastructure, so it requires server compromise or a separate TLS-validation flaw, hence not unauthenticated-remote. Even benign cron strings like "0,15,30,45 * * * *" (18 B) already corrupt memory, so crashes can occur without an attacker. Downgraded from Critical to High because the network path is gated by TLS to a trusted server and the strongest practical path is local rbus → root heap overflow (LPE), with RCE feasible but requiring heap-grooming on the target libc.

```c
static INT32 dcmSettingJsonGetVal(VOID *jsonHandle, INT8 *item, INT8 *sval, ...)
{
    ...
    else if(cJSON_IsString(pJsonItem)) {
        *type = DCM_JSONITEM_STR;
        strcpy(sval, pJsonItem->valuestring);   // <-- no bound, sval may be 8 or 16 bytes
    }
...
// callers in dcmSettingParseConf:
//   sval = cUploadPrtl[8], cUploadURL[128], cTimeZone[16],
//   sval = pLogCron -> DCMDHandle.logCron[16]
//   sval = pDifdCron -> DCMDHandle.difdCron[16]  (last field -> heap overflow)
```

---

### 9. DCM-009 — Stack overflow of t2_ver[32] from rbus-supplied string via strcpy

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-120 |
| **Component** | core-dcm |
| **Location** | `dcm.c:151-183` |
| **Input Source** | rbus (IPC from another local component) |
| **Confidence** | High |

**Description:** dcmDaemonMainInit() declares `INT8 t2_ver[32];` and passes it to dcmRbusGetT2Version(). That function (dcm_rbus.c:327) performs `strcpy(t2_ver, stringValue)` where stringValue is the result of rbusValue_ToString() on the rbus parameter Device.X_RDKCENTRAL-COM_T2.Version (or similar) obtained over IPC. Any process on the rbus that can register/provide that parameter with a string longer than 31 bytes overflows the daemon's stack frame.

**Attack Vector:** A local process able to publish the T2 Version rbus parameter (or compromise of the telemetry component) returns an oversized string.

**Impact:** Stack buffer overflow -> local privilege escalation / RCE in root daemon.

**Verification Notes:** Confirmed in code: /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/dcm.c:151 declares `INT8 t2_ver[32];` on the stack and at line 182 passes it to dcmRbusGetT2Version(). In /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/dcm_rbus.c:312-327 the function does `rbus_get(handle, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version", &paramValue)`, then `rbusValue_ToString(paramValue, NULL, 0)` (0 = no truncation, full allocation), then an unbounded `strcpy(t2_ver, stringValue)`. There is no length validation anywhere in the chain, no #ifdef guard, and Makefile.am/configure.ac add no -fstack-protector or _FORTIFY_SOURCE (any hardening would have to come from the distro layer). The rbus parameter is an RFC TR-181 string that is supplied over local IPC by another component and is also remotely writable via WebPA/RFC; any peer on the rtrouted Unix socket can provide or set it. This is a genuine attacker-controlled-length stack overflow in a root, unsandboxed daemon.

**Exploitability:** Preconditions: attacker must be able to influence the rbus value of Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version — achievable by (a) any local process that can reach the world-accessible rtrouted Unix socket and either register/provide that element or issue rbus_set on it (RFC strings are persisted), or (b) a compromised/malicious WebPA/XCONF RFC push. Trigger timing: the vulnerable copy runs only during dcmDaemonMainInit(), so the attacker needs dcmd to (re)start after the value is poisoned — but RFC values persist across reboot, so one set yields a recurring trigger on every boot. Impact: dcmd runs as root (uid 0) per dcmd.service with no User=/sandboxing; attacker controls both length and full byte content of the overflow, enabling at minimum reliable DoS and potentially local privilege escalation to root (modulo distro-supplied ASLR/canary). Mitigating factors: requires existing local foothold or operator-side cloud access; init-time-only path narrows the trigger window; distro-level stack-protector (if enabled in Yocto) would convert RCE attempts into a crash but not prevent the overflow itself.

```c
INT32 dcmDaemonMainInit(DCMDHandle *pdcmHandle)
{
    INT32 ret = DCM_SUCCESS;
    INT8  t2_ver[32];
    ...
    ret = dcmRbusGetT2Version(pdcmHandle->pRbusHandle, t2_ver);  // -> strcpy(t2_ver, rbus_string)
    DCMInfo("T2 Version: %s\n", t2_ver);
```

---

### 10. DCM-010 — Directory symlink following in add_directory_to_tar() allows arbitrary file exfiltration via uploaded archive

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-59 |
| **Component** | uploadstblogs |
| **Location** | `uploadstblogs/src/archive_manager.c:595-646` |
| **Input Source** | Local filesystem (world-writable /tmp) — attacker-planted symlink in archive source directory |
| **Confidence** | High |

**Description:** add_directory_to_tar() recursively walks the source directory using stat() (line 619), which follows symbolic links. If an entry is a symlink to a directory, S_ISDIR(st.st_mode) is true and the function recurses into the target via opendir(). While add_file_to_tar() correctly uses O_NOFOLLOW for regular files (line 525), there is no equivalent protection for directory entries. The source directories passed to create_archive() include /tmp/DCM/ (default ctx->dcm_log_path, context_manager.c:344) and /tmp/log_on_demand (ONDEMAND_TEMP_DIR, strategies.c:52) — both children of world-writable /tmp. A local unprivileged attacker who can create or populate these directories can plant a symlink such as `/tmp/DCM/x -> /etc` (or /root, /opt/secure). The privileged daemon will then tar the entire target directory and upload it to the remote log server, disclosing arbitrary root-readable files (private keys, credentials, /etc/shadow).

**Attack Vector:** Local unprivileged user pre-creates (or races to populate) /tmp/DCM or /tmp/log_on_demand and drops `ln -s /etc evil` inside it. When uploadstblogs runs the DCM or ONDEMAND strategy, add_directory_to_tar() follows the symlink, archives the contents of /etc, and the archive is uploaded to the cloud log endpoint.

**Impact:** Information disclosure of arbitrary root-readable files to remote upload endpoint; effectively local-to-remote data exfiltration with root read privileges.

**Verification Notes:** The code at archive_manager.c:619 uses stat() (symlink-following) and recurses via opendir() at :631 with no lstat/O_NOFOLLOW guard on directory entries; O_NOFOLLOW at :525 only protects the final path component, so files reached through an intermediate directory symlink (e.g. /tmp/DCM/evil/passwd where evil→/etc) are still opened and archived. The source directory is attacker-influenceable: ctx->dcm_log_path defaults to /tmp/DCM/ (context_manager.c:344), context_manager.c:350 happily uses a pre-existing /tmp/DCM without ownership checks, and strategies.c:297-302 removes /tmp/DCM after every upload, giving an unprivileged process a non-racy window to recreate and own it before the next run. add_timestamp_to_files() (file_operations.c:406) merely rename()s the symlink, which preserves it. The resulting tarball is written back into source_dir (archive_manager.c:717-721), so an attacker who owns /tmp/DCM can read the archived /etc, /root, or /opt/secure contents locally even without intercepting the cloud upload.

**Exploitability:** Preconditions: attacker has unprivileged local code execution (or any sandboxed component able to mkdir/symlink under /tmp) on the device and can act during the window after dcm_cleanup removes /tmp/DCM and before the next DCM-strategy run; the DCM upload strategy must be selected (upload_flag true in /tmp/DCMSettings.conf). Attacker position: local, uid != 0. Mitigating factors: if /etc/device.properties overrides DCM_LOG_PATH to a non-/tmp location the default vector is removed; the ONDEMAND path (/tmp/log_on_demand) is harder because strategies.c:379-386 wipes and recreates the dir as root immediately before use, leaving only a narrow race that additionally depends on createDir()'s mode bits. RDK STBs are not multi-user shells, but compromised containers/Lightning apps writing to /tmp are an in-scope threat, so High (local root-file disclosure / info-leak privesc) is appropriate rather than Critical.

```c
    while ((entry = readdir(dir)) != NULL) {
        ...
        char fullpath[MAX_PATH_LENGTH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        ...
        struct stat st;
        if (stat(fullpath, &st) != 0) {   /* follows symlinks */
            continue;
        }
        ...
        if (S_ISDIR(st.st_mode)) {
            // Recursively process subdirectory  -> follows dir symlink, no O_NOFOLLOW / lstat
            if (add_directory_to_tar(gz, fullpath, base_path, exclude_file) != 0) {
                closedir(dir);
                return -1;
            }
        }
```

---

### 11. DCM-011 — gzopen() on predictable path under /tmp without O_NOFOLLOW/O_EXCL enables arbitrary file overwrite

| | |
|---|---|
| **Severity** | High |
| **CWE** | CWE-377 |
| **Component** | uploadstblogs |
| **Location** | `uploadstblogs/src/archive_manager.c:717-732` |
| **Input Source** | Local filesystem (world-writable /tmp) — attacker-planted symlink at predictable archive output path |
| **Confidence** | High |

**Description:** create_archive_with_options() builds the output path as `<source_dir>/<MAC>_Logs_<MM-DD-YY-HH-MM><AM|PM>.tgz` and opens it with gzopen(archive_path, "wb9"). gzopen() ultimately calls open() with O_CREAT|O_TRUNC and follows symlinks. When source_dir is /tmp/DCM/ (default dcm_log_path) or /tmp/log_on_demand (ONDEMAND_TEMP_DIR), the output file lives under world-writable /tmp. The filename is fully predictable: the device MAC is constant and the timestamp has 1-minute granularity. A local attacker who controls or pre-creates the /tmp subdirectory can plant a symlink with the predicted name pointing at any file (e.g., /etc/passwd, /lib/systemd/system/foo.service). When the privileged daemon writes the archive, it truncates and overwrites the symlink target with attacker-influenced compressed content.

**Attack Vector:** Attacker reads device MAC, computes the next-minute archive filename, creates `/tmp/DCM/` (or wins the race) and `ln -s /etc/passwd /tmp/DCM/<MAC>_Logs_<ts>.tgz`. Triggering or waiting for a DCM/on-demand log upload causes root to clobber the target file.

**Impact:** Arbitrary file overwrite/corruption as root; can be leveraged for DoS (clobber critical system file) or privilege escalation (overwrite a script/config later executed by root).

**Verification Notes:** Confirmed at /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/uploadstblogs/src/archive_manager.c:717-728: gzopen(archive_path, "wb9") opens with O_CREAT|O_TRUNC and follows symlinks; no unlink/lstat/O_NOFOLLOW/O_EXCL guard exists. The path is fully predictable — generate_archive_name() (lines 401-445) builds "<MAC>_Logs_%m-%d-%y-%I-%M%p.tgz" with 1-minute granularity. target_dir defaults to ctx->dcm_log_path = "/tmp/DCM/" (context_manager.c:344) and the daemon deletes that directory after every run (strategies.c:302 remove_directory), so an attacker can pre-create /tmp/DCM/ before the next scheduled upload; create_directory() (file_operations.c:100) returns success if the dir already exists without verifying ownership. The code runs in-process inside the root dcmd daemon (dcm.c:99 calls uploadstblogs_run()). fs.protected_symlinks does not help because the symlink sits in an attacker-owned non-sticky subdir of /tmp, not in the sticky /tmp itself. The codebase already uses O_NOFOLLOW on adjacent paths (archive_manager.c:525, cleanup_handler.c:158/388) but missed this write sink.

**Exploitability:** Preconditions: a local principal with write access to /tmp (e.g., a de-privileged RDK container/app or non-root service) and knowledge of the device MAC (readable from /sys). Attacker creates /tmp/DCM/ (it is removed after each upload so it normally does not exist between runs), then plants a few symlinks covering the next minutes: ln -s /etc/passwd /tmp/DCM/<MAC>_Logs_<ts>.tgz. When dcmd's scheduled or on-demand upload fires, root truncates and overwrites the symlink target with gzip data. Mitigating factors: written content is a gzip stream (fixed 0x1f8b header) so attacker has only partial content control — direct crafting of valid /etc/passwd is hard, but arbitrary root-file clobber yields integrity loss / DoS / brick and can be chained (e.g., corrupting service unit or config files). Also, RDK STBs are not general multi-user systems; the attack assumes a compromised lower-privilege component, which is a realistic priv-esc chain but not a remote vector. Severity stays High (root arbitrary-file overwrite via CWE-377/CWE-59), not Critical due to limited content control and local-only precondition.

```c
    const char* target_dir = output_dir ? output_dir : source_dir;
    
    // Archive path
    char archive_path[MAX_PATH_LENGTH];
    snprintf(archive_path, sizeof(archive_path), "%s/%s", target_dir, archive_filename);
    ...
    // Create gzip file  -> no O_EXCL, no O_NOFOLLOW, follows symlinks, predictable name in /tmp subdir
    gzFile gz = gzopen(archive_path, "wb9");
    if (!gz) {
        RDK_LOG(RDK_LOG_ERROR, LOG_UPLOADSTB, 
                "[%s:%d] Failed to create gzip file\n", __FUNCTION__, __LINE__);
        return -1;
    }
```

---

### 12. DCM-012 — Stack buffer overflow in dcmRbusGetT2Version() via unbounded strcpy() of RBUS-supplied string

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-121 |
| **Component** | core-dcm |
| **Location** | `dcm_rbus.c:327` |
| **Input Source** | RBUS get() response from another local process (provider of Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version) |
| **Confidence** | High |

**Description:** dcmRbusGetT2Version() fetches the TR-181 parameter Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version via rbus_get() and converts the response to a heap C-string with rbusValue_ToString(paramValue, NULL, 0), which returns a string of whatever length the remote RBUS provider supplied. It then copies it into the caller-supplied buffer with strcpy(t2_ver, stringValue) at line 327. The only caller is dcmDaemonMainInit() (dcm.c:151/182) which passes a 32-byte stack array `INT8 t2_ver[32]`. There is no length check, no strncpy, no truncation. A malicious or compromised local process that registers as the provider of this TR-181 element on rtrouted (e.g., by racing the legitimate RFC agent at boot, or after compromising it) can return a string >= 32 bytes and overwrite saved registers / the return address of dcmDaemonMainInit(), which runs as root immediately after fork()/setsid()/umask(0).

**Attack Vector:** Local attacker connects to rtrouted Unix socket and registers (or front-runs registration of) the data element 'Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version'. When dcmd starts and calls rbus_get() at dcm_rbus.c:312, the attacker's provider returns an RBUS_STRING of arbitrary length (e.g., 512 bytes containing a ROP chain / return-address overwrite). strcpy() at line 327 writes it into the 32-byte stack buffer of dcmDaemonMainInit().

**Impact:** Local privilege escalation to root via stack-based buffer overflow in the root daemon's init path. Even with stack canaries/ASLR, this is at minimum a reliable DoS (canary abort) of dcmd at every boot.

**Verification Notes:** Code matches the report exactly: dcm_rbus.c:327 does `strcpy(t2_ver, stringValue)` where stringValue is the unbounded heap string returned by rbusValue_ToString(paramValue, NULL, 0) for an RBUS_STRING fetched via rbus_get() at line 312, and the sole production caller dcm.c:182 passes a 32-byte stack array `INT8 t2_ver[32]` (dcm.c:151). No length check, truncation, or strncpy exists anywhere in the path; _FORTIFY_SOURCE cannot help because inside dcmRbusGetT2Version the destination is a bare `INT8 *` (line 287) so __builtin_object_size is unknown. The overflow lands in dcmDaemonMainInit's frame in the root dcmd process. This is a genuine CWE-121. I downgrade High→Medium because it fires only once at daemon init, requires the attacker to be the registered RBUS provider for Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version at that moment (boot race vs. the legitimate RFC/tr69hostif provider, or that provider being absent), and RDK Yocto's typical -fstack-protector-strong would place a canary in dcmDaemonMainInit (it has a char[32] local), converting RCE into an abort/DoS on hardened builds.

**Exploitability:** Preconditions: (1) attacker has local write access to the rtrouted Unix socket — common in RDK where the socket is often world-accessible and is mounted into app containers; (2) attacker must call rbus_open()/rbus_regDataElements() for "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.Version" before the legitimate provider does, or on a profile where no provider exists, since RBUS rejects duplicate element registration; (3) dcmd must (re)start after the malicious registration — the vulnerable strcpy runs only in dcmDaemonMainInit, not on any runtime event. Mitigating factors: repo Makefile.am sets only `-fPIC -pthread` (no local hardening), but distro-level security_flags.inc in RDK Yocto usually adds -fstack-protector-strong, which protects dcmDaemonMainInit's frame and reduces impact to denial-of-service of the root daemon; ASLR further complicates one-shot ROP. Without SSP the result is a classic saved-RA overwrite as root.

```c
/* dcm_rbus.c */
312    rc = rbus_get(handle, DCM_RBUS_T2_VERSION, &paramValue);
...
318    rbusValueType = rbusValue_GetType(paramValue);
319    if(rbusValueType == RBUS_STRING) {
320        stringValue   = rbusValue_ToString(paramValue, NULL, 0);
...
327            strcpy(t2_ver, stringValue);   /* <-- unbounded copy */
328            DCMInfo("Telemetry 2 Version: %s\n", stringValue);

/* dcm.c */
151    INT8  t2_ver[32];                         /* <-- 32-byte stack dst */
...
182    ret = dcmRbusGetT2Version(pdcmHandle->pRbusHandle, t2_ver);
```

---

### 13. DCM-013 — popen() executed on uninitialised heap buffer when DCM_LOG_UPLOAD scheduler fires

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-457 |
| **Component** | core-dcm |
| **Location** | `dcm.c:72-113` |
| **Input Source** | RBUS / XCONF JSON (controls cron timing) + heap-spray via RBUS strings; the executed buffer itself is uninitialised heap memory |
| **Confidence** | High |

**Description:** dcmDaemonMainInit() allocates pdcmHandle->pExecBuff = malloc(EXECMD_BUFF_SIZE=1024) at dcm.c:192 without zeroing it. dcmRunJobs() is registered as the callback for BOTH DCM_LOGUPLOAD_SCHED and DCM_DIFD_SCHED. When the LOG_UPLOAD branch is taken (lines 72-106), pExecBuff is never written — the branch only calls uploadstblogs_run(). However line 113 unconditionally calls dcmUtilsSysCmdExec(pExecBuff), which forwards to popen(pExecBuff, "r") (dcm_utils.c:96). The NULL check in dcmUtilsSysCmdExec (line 123) does not help because malloc() returned a valid non-NULL pointer. Result: on the first DCM_LOG_UPLOAD tick, root executes 1024 bytes of uninitialised heap memory as a shell command; on subsequent ticks it re-executes the stale swupdate command. The cron schedule that decides which job fires first is itself attacker-controlled via the JSON at confPath.

**Attack Vector:** Attacker (via RBUS Setconfig→evil.json or via XCONF cloud) sets 'urn:settings:LogUploadSettings:UploadSchedule:cron' to fire immediately and 'urn:settings:CheckSchedule:cron' to fire later, ensuring the LOG_UPLOAD branch runs first with virgin pExecBuff. If the attacker can spray the heap before pExecBuff is allocated (e.g., via large RBUS strings during init, the cJSON parse of the DCM blob, or rbusValue_ToString allocations), a freed 1024-byte chunk containing a shell command can be recycled into pExecBuff and executed by popen() as root.

**Impact:** Root shell-command execution of uninitialised/stale heap data. With heap spraying: arbitrary command execution as root. Without spraying: unintended re-execution of swupdate_utility.sh on every log-upload tick (DoS / unintended firmware-update triggers) and undefined behaviour on first invocation.

**Verification Notes:** The core defect is confirmed: dcm.c:192 allocates pExecBuff via malloc(1024) with no zeroing; the DCM_LOGUPLOAD_SCHED branch (dcm.c:72-106) never writes pExecBuff; and dcm.c:113 unconditionally passes it to dcmUtilsSysCmdExec(), which only NULL-checks (dcm_utils.c:123) before calling popen(cmd,"r") at dcm_utils.c:96. The callback is reachable on devices where ENABLE_MAINTENANCE is absent from /etc/device.properties (g_bMMEnable=0, dcm_parseconf.c:764-770), and the cron ordering that decides which job fires first is taken from urn:settings:LogUploadSettings:UploadSchedule:cron / urn:settings:CheckSchedule:cron in the XCONF/RBUS-supplied JSON (dcm.c:371-376, dcm_parseconf.h:39-40). However, the claimed RCE via heap spray is speculative: every allocation that precedes the malloc(1024) — dcmUtilsCheckDaemonStatus (PID file), dcmSettingsInit (/etc/include.properties, /etc/device.properties), dcmRbusInit/rbus_open, dcmRbusGetT2Version (T2 version string from another root daemon), dcmRbusSubscribeEvents — handles content that is not attacker-controllable; the cJSON parse of the attacker-influenced blob happens AFTER pExecBuff is already allocated. Realistic outcome is popen("") on a fresh zeroed brk page or a shell syntax error on recycled metadata, and on subsequent ticks a stale-but-trusted "/bin/sh /lib/rdk/swupdate_utility.sh ..." re-execution.

**Exploitability:** Preconditions: (1) device built without ENABLE_MAINTENANCE in /etc/device.properties so dcmSettingsGetMMFlag() returns 0; (2) LOG_UPLOAD cron fires before (or without) DIFD cron — this ordering can be influenced by anyone who controls the XCONF response or the RBUS Device.DCM.Setconfig path, but that same position already grants far stronger primitives elsewhere. Attacker position for the claimed RCE additionally requires a heap-grooming primitive that places a NUL-terminated shell string into a freed ~1024-byte chunk before dcm.c:192 executes; no such attacker-content-controlled allocation exists in the init path (dcmSettingsInit/dcmRbusInit/dcmRbusGetT2Version/dcmRbusSubscribeEvents all consume root-owned files or trusted-daemon RBUS values). Mitigating factors: fresh sbrk pages are zero-filled (popen("") is a no-op); on MM-enabled builds the code returns at dcm.c:60 and never reaches popen; after one DIFD tick the buffer permanently holds a fixed trusted swupdate command. Net: genuine CWE-457 root-popen-on-uninitialized-heap that must be fixed (one-line memset/calloc), but the High/RCE rating is not substantiated — downgrade to Medium.

```c
/* dcm.c — dcmDaemonMainInit */
192    pdcmHandle->pExecBuff = malloc(EXECMD_BUFF_SIZE);   /* not zeroed */

/* dcm.c — dcmRunJobs */
65     INT8 *pExecBuff = pdcmHandle->pExecBuff;
...
72     if(strcmp(profileName, DCM_LOGUPLOAD_SCHED) == 0) {
...        /* uploadstblogs_run(...) — pExecBuff NEVER written here */
106    }
107    else if(strcmp(profileName, DCM_DIFD_SCHED) == 0) {
108        DCMInfo("Start FW update Script\n");
109        snprintf(pExecBuff, EXECMD_BUFF_SIZE, "/bin/sh %s/swupdate_utility.sh 0 2 >> /opt/logs/swupdate.log 2>&1",
110                                               pRDKPath);
111    }
112
113    dcmUtilsSysCmdExec(pExecBuff);   /* unconditional → popen() */
```

---

### 14. DCM-014 — umask(0) makes every file created by root daemon world-writable

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-732 |
| **Component** | core-dcm |
| **Location** | `dcm.c:295` |
| **Input Source** | Local filesystem (any unprivileged user with write access to /tmp and, on many RDK builds, /opt) |
| **Confidence** | High |

**Description:** main() calls umask(0) immediately after fork(). Every subsequent file the root daemon (and the in-process uploadstblogs library) creates with default mode bits is therefore mode 0666 (world-writable) or 0777 for directories. Affected files include /tmp/.dcm-daemon.pid, /tmp/DCMSettings.conf, /opt/.DCMSettings.conf, /opt/rdk_maintenance.conf, /opt/loguploadstatus.txt, /tmp/httpresults.txt, and /tmp/.log-upload.lock. Any local user can rewrite the persisted DCM settings (changing upload URL → log exfiltration), tamper the maintenance window, forge upload-success status, or rewrite the PID file. There is no functional reason for a root daemon to clear its umask; this appears to be a copy-paste daemonisation pattern.

**Attack Vector:** Local unprivileged user edits /tmp/DCMSettings.conf (created 0666) to set 'urn:settings:LogUploadSettings:upload=false' → uploads suppressed; or edits /opt/.DCMSettings.conf to redirect upload URL; or rewrites /tmp/.dcm-daemon.pid to '1' so the next dcmd start aborts (DoS).

**Impact:** Local privilege boundary violation: world-writable root-owned configuration/state files enable config tampering, log-upload redirection (PII exfiltration), persistent DoS of dcmd, and tampering of /opt/rdk_maintenance.conf consumed by other root services.

**Verification Notes:** Confirmed verbatim: dcm.c:295 calls umask(0) in the forked child, and dcmd.service has no User=/UMask= directive, so the daemon runs as root with a zero umask. All subsequent fopen(...,"w") calls therefore create files mode 0666: /tmp/.dcm-daemon.pid (dcm_utils.c:163, called from dcmDaemonMainInit after the umask), /tmp/DCMSettings.conf and /opt/.DCMSettings.conf (dcm_parseconf.c:263,270), and /opt/rdk_maintenance.conf (dcm_parseconf.c:459). At least one of these files is read back and trusted: uploadstblogs/src/strategies.c:73-115 reads /tmp/DCMSettings.conf and skips log upload if urn:settings:LogUploadSettings:upload != true, so any uid can suppress diagnostic uploads; the PID-file rewrite-to-"1" DoS is also real (dcm_utils.c:146-159 stats /proc/1 and aborts). One sub-claim is overstated: within this repo /opt/.DCMSettings.conf is only written, never re-read, so the "redirect upload URL via /opt/.DCMSettings.conf" vector is not substantiated in-scope (the URL comes from in-memory parsed config via dcmSettingsGetUploadURL). Net impact is integrity tampering / anti-forensics / restart DoS, not code execution — Medium is the correct rating.

**Exploitability:** Preconditions: attacker must already have arbitrary-uid code execution on the device (e.g., a compromised sandboxed app or non-root daemon) with write access to /tmp (always 1777) and traverse access to /opt (typically 755 on RDK, so writing into an existing 0666 file is allowed even though creating new files is not). No race needed for the upload-suppression vector: dcmd writes /tmp/DCMSettings.conf once per config fetch and the scheduled uploader reads it hours later, giving a wide tamper window; fopen("w") on the pre-existing file does not reset its 0666 mode. Mitigating factors: RDK CPE devices are largely single-purpose with limited interactive non-root access, the /opt URL-redirect claim is not exercised by code in this repo, and the PID-file attack only causes denial-of-service on the next dcmd restart rather than against the running instance.

```c
293    /* unmask the file mode */
294    umask(0);
295
296    /* set new session */
297    sid = setsid();
```

---

### 15. DCM-015 — Stack buffer overflow in dcmSettingSaveMaintenance() when cron minute/hour token >= 4 chars

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-121 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:460-481` |
| **Input Source** | XCONF cloud JSON 'urn:settings:CheckSchedule:cron' (network) or local RBUS confPath file |
| **Confidence** | High |

**Description:** dcmSettingSaveMaintenance() (compiled when HAS_MAINTENANCE_MANAGER is defined and called when ENABLE_MAINTENANCE is set) parses the cloud-supplied CheckSchedule cron string by manually copying bytes into 4-byte stack arrays buffmin[4] and buffhr[4] until a space delimiter is seen. There is no bounds check on `*ptr++ = pCronptr[i]`. A perfectly legal cron field such as '*/15' (4 chars) fills the buffer with no NUL terminator, and any field >=5 chars (e.g. '12345 6789 * * *' or '*/100 0 * * *') writes past the end of the 4-byte stack arrays, smashing the stack of a root process.

**Attack Vector:** Set 'urn:settings:CheckSchedule:cron' in the XCONF DCM JSON to a value whose minute or hour field is >=5 characters. Triggered automatically inside dcmSettingParseConf() when the device has HAS_MAINTENANCE_MANAGER built and ENABLE_MAINTENANCE present in /etc/device.properties.

**Impact:** Stack buffer overflow as root -> potential RCE; minimum DoS (stack-canary abort).

**Verification Notes:** The code at /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/dcm_parseconf.c:460-481 matches the report exactly: `buffhr[4]`/`buffmin[4]` are 4-byte stack arrays and the loop writes `*ptr++ = pCronptr[i]` until a space, with no bound. The input is the value of JSON key "urn:settings:CheckSchedule:cron" (DCM_DIFD_CRON), copied via an unbounded `strcpy(sval, pJsonItem->valuestring)` at line 224 into `g_pdcmHandle->difdCron[16]` and then passed to `dcmSettingSaveMaintenance()` at line 659 — there is no upstream length/format validation, and even a legitimate field such as `*/15` or `0,30` exceeds 3 chars and corrupts the stack. The function is reachable: dcm.c:371 calls dcmSettingParseConf() on the file path delivered via RBUS events Device.DCM.Setconfig/Processconfig (dcm_rbus.c:88-99, 430-451), and HAS_MAINTENANCE_MANAGER is a documented production build option (README.md, cov_build.sh) gated at runtime only by ENABLE_MAINTENANCE in /etc/device.properties. I downgrade from High to Medium because the input originates from the operator-controlled XCONF/T2 pipeline or local RBUS rather than an unauthenticated remote attacker, the overflow length is small, and production RDK toolchains typically enable -fstack-protector-strong, making a root-daemon crash (DoS) the most likely practical impact rather than reliable RCE.

**Exploitability:** Preconditions: binary built with -DHAS_MAINTENANCE_MANAGER and /etc/device.properties contains ENABLE_MAINTENANCE (both common on RDK-V STBs with Maintenance Manager). Attacker position: must control the DCM JSON content — i.e., compromise/spoof the XCONF cloud response that telemetry2.0 fetches, OR be a local process able to publish the RBUS events Device.DCM.Setconfig (pointing confPath at an attacker-written JSON file) and Device.DCM.Processconfig. No authentication or validation is performed on the cron string itself. Mitigating factors: input source is operator-trusted infrastructure (not Internet-anonymous), the intermediate buffer difdCron[16] caps the easily-controlled string to ~15 bytes (still 11 bytes past buffmin[4]), and stack canaries on typical RDK builds convert overwrite of the return address into an abort. Worst case on hardened builds: deterministic crash of the root dcmd daemon; on builds without stack-protector: potential control-flow hijack as root.

```c
    INT8 buffhr[4] = {0};
    INT8 buffmin[4] = {0};
    INT8 *ptr;
    INT32 i = 0, count = 0;
    ...
    ptr = buffmin;
    for(i = 0; i < strlen(pCronptr); i++) {
        if(pCronptr[i] == ' ') {
            count++;
            if(count > 1) {
                break;
            }
            ptr = buffhr;
            continue;
        }
        *ptr++ = pCronptr[i];   // no bound on 4-byte stack array
    }
```

---

### 16. DCM-016 — Negative-index heap write before cJsonStr when 'urn:settings:TelemetryProfile' appears at offset 0/1

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-787 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:148-154` |
| **Input Source** | Local RBUS confPath file contents, or XCONF cloud JSON file |
| **Confidence** | High |

**Description:** After loading the first line of the config file into pdcmSetHandle->cJsonStr[2048], the code searches for the substring 'urn:settings:TelemetryProfile' and writes '}' and NUL at offsets (lc-2) and (lc-1) where lc = match_offset. If the attacker-controlled file begins with this literal at byte 0 (lc=0) or byte 1 (lc=1), the writes occur at cJsonStr[-2]/cJsonStr[-1]. cJsonStr is the FIRST member of the malloc'd DCMSettingsHandle, so these writes land in the glibc heap chunk header (size/prev_size), corrupting allocator metadata.

**Attack Vector:** Local RBUS attacker publishes Device.DCM.Setconfig with confPath = '/tmp/x' where /tmp/x contains 'urn:settings:TelemetryProfile...' starting at byte 0, then publishes Device.DCM.Processconfig. A compromised XCONF backend could in theory deliver such a malformed blob too.

**Impact:** Heap-metadata corruption (2-byte underflow before allocation) in root daemon -> potential RCE or crash on next malloc/free.

**Verification Notes:** The code at /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/dcm_parseconf.c:148-154 is exactly as described, with no bounds check on `lc` before `pcJsonStr[lc-2]='}'` / `pcJsonStr[lc-1]=0`. `cJsonStr` is verifiably the first member of the malloc'd `DCMSettingsHandle` (dcm_parseconf.h:51-53, allocated at dcm_parseconf.c:749), so indices -2/-1 land in the glibc chunk size header. The input path flows unchecked from the RBUS `Device.DCM.Setconfig` event (dcm_rbus.c:88-94) through `dcmRbusGetConfPath()` into `dcmSettingParseConf()` → `dcmSettingJsonInit()` (dcm.c:363-371, dcm_parseconf.c:526) with zero content validation before the strstr. However, the primitive is extremely constrained: it writes two FIXED bytes (0x7D, 0x00) at FIXED offsets into the high-order bytes of the chunk-size field of a long-lived allocation that is only freed at daemon shutdown — modern glibc will simply abort on the size-mismatch check. There is no attacker control over the written value or offset, so this yields at best a deterministic DoS of dcmd, not the memory-corruption-to-RCE that a High rating implies.

**Exploitability:** Preconditions: attacker must control the first 1-2 bytes of the file whose path arrives via the Device.DCM.Setconfig RBUS event. Realistic positions: (a) compromised/MITMed XCONF backend delivering a blob that telemetry2.0 writes verbatim and forwards, (b) local process that can either impersonate the RBUS provider of Device.DCM.Setconfig (rtrouted socket is typically world-accessible and RBUS lacks auth) and point confPath at an attacker-owned /tmp file, or (c) local user overwriting telemetry2.0's 0666 /tmp response file before dcmd reads it. Mitigating factors: only 2 fixed bytes are written into the size field's MSBs; the corrupted chunk is allocated once at init and never freed in the `while(1)` main loop, so corruption stays dormant until shutdown/adjacent-consolidation, where glibc integrity checks abort cleanly. No path to code execution — downgrade from High to Medium (root-daemon DoS via IPC/cloud input).

```c
    /* Remvoing the telemetry entries */
    tmp = strstr(pcJsonStr, DCM_T2_JSONSTR);

    if(tmp) {
        lc = tmp - pcJsonStr;
        pcJsonStr[lc - 2] = '}';   // OOB if lc <= 1
        pcJsonStr[lc - 1] = 0;     // OOB if lc == 0
    }
```

---

### 17. DCM-017 — Arbitrary file read as root via attacker-controlled confPath (no path validation)

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-73 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:134, 257` |
| **Input Source** | RBUS event Device.DCM.Setconfig payload key 'dcmSetConfig' |
| **Confidence** | High |

**Description:** Both dcmSettingJsonInit() (line 134) and dcmSettingStoreTempConf() (line 257) call fopen(pConffile, "r") where pConffile == confPath received verbatim from the RBUS event Device.DCM.Setconfig. There is no realpath(), no allowed-directory prefix check, no '..' rejection and no symlink check. Any local process on the rtrouted bus can make the root daemon open and parse an arbitrary path. This is the enabling primitive for every overflow above (point at /tmp/evil.json), and also allows pointing at FIFOs/devices (DoS hang) or huge files (memory exhaustion via calloc(file_len+1)). If the chosen file happens to be valid JSON, its keys/values are re-serialized to world-readable /tmp/DCMSettings.conf (info leak).

**Attack Vector:** Local unprivileged process publishes Device.DCM.Setconfig {dcmSetConfig:'/path/of/choice'} then Device.DCM.Processconfig on the RBUS bus.

**Impact:** Local privilege boundary bypass: arbitrary-file-open-as-root, DoS (FIFO/huge file), and pivot to all heap/stack overflows in this module by parsing attacker-authored JSON.

**Verification Notes:** The data flow is confirmed end-to-end. In /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/dcm_rbus.c:88-94, the RBUS subscriber callback rbusSetConf() extracts key 'dcmSetConfig' from the Device.DCM.Setconfig event payload and strncpy()s it verbatim into confPath[128] with only length bounding and zero path sanitisation. /home/bow/Desktop/RDK/Phase2/RDK_Repos/RDK_Repos/dcm-agent-develop/dcm.c:363-371 then hands that buffer straight to dcmSettingParseConf(), which calls fopen(infile,"r") at dcm_parseconf.c:134 and again at :257 — no realpath, prefix check, '..' filter, or O_NOFOLLOW. The FIFO-hang, calloc(file_len+1) memory-exhaustion, and JSON-only re-serialisation to world-readable /tmp/DCMSettings.conf consequences are all accurate. However, I downgrade from High to Medium as a standalone CWE-73: (a) the 'arbitrary file read' leak is constrained to root-only files that happen to be parseable JSON, (b) the overflow chain it enables is tracked as separate findings, and (c) delivering the event is not free — dcm-agent is a *subscriber*; the legitimate *provider* of Device.DCM.Setconfig is the T2/telemetry component, so an attacker on rtrouted must either win the element-registration race at boot or craft raw rtmessage frames, not merely 'publish on the bus'.

**Exploitability:** Preconditions: local code execution with connectivity to the rtrouted Unix socket, plus the ability to act as publisher of Device.DCM.Setconfig (register the element before telemetry2 does, or speak raw rtmessage to spoof the provider). No authentication is checked in rbusSetConf() itself. Mitigating factors: rtrouted access is typically limited to host-side RDK middleware (containerised apps usually cannot reach it); only one provider may register a given RBUS element, so a running T2 blocks naive re-registration; the info-leak path requires the target file to be valid JSON; and the threat model for RDK STBs generally assumes no untrusted interactive local users. Primary practical value of this bug is as the input-delivery primitive for the unbounded strcpy at dcm_parseconf.c:224 and for redirecting upload_http_link in uploadstblogs_run().

```c
// dcmSettingJsonInit
    fpin = fopen(infile, "r");          // infile == RBUS-supplied confPath, no validation
    if(fpin == NULL) {
        DCMError("Failed to open input file:%s\n", infile);
        return DCM_FAILURE;
    }
...
// dcmSettingStoreTempConf
    FILE *fp_in = fopen(pConffile, "r");   // same tainted path opened again
```

---

### 18. DCM-018 — Root writes /tmp/DCMSettings.conf via fopen("w") without O_NOFOLLOW/O_EXCL - symlink clobber + world-writable file

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-59 |
| **Component** | core-dcm |
| **Location** | `dcm_parseconf.c:263-270` |
| **Input Source** | Local filesystem (/tmp) - attacker-controlled symlink / world-writable file |
| **Confidence** | High |

**Description:** dcmSettingStoreTempConf() opens DCM_TMP_CONF (/tmp/DCMSettings.conf) and DCM_OPT_CONF (/opt/.DCMSettings.conf) with fopen(path, "w"). fopen follows symlinks and there is no O_NOFOLLOW / O_EXCL / lstat pre-check. /tmp is world-writable, so any local user can pre-create /tmp/DCMSettings.conf as a symlink to an arbitrary root-owned file; the daemon (uid 0) then truncates that target and fills it with attacker-controlled JSON-derived key=value lines. Additionally, because main() previously called umask(0), if the file is created fresh it gets mode 0666, allowing any local user to later rewrite the persisted DCM settings (URL, cron, upload flag) consumed by other RDK components.

**Attack Vector:** Local unprivileged user runs `ln -s /etc/passwd /tmp/DCMSettings.conf` (or any target) before the daemon's first Processconfig event, OR rewrites the 0666 file afterwards to disable/redirect log upload. Effective only if kernel sysctl fs.protected_symlinks=0 (common on older RDK BSPs).

**Impact:** Local arbitrary-file-truncate/overwrite as root (symlink follow); persistent tampering of DCM settings (world-writable 0666 file).

**Verification Notes:** The code is exactly as described: dcm_parseconf.c:263 calls fopen("/tmp/DCMSettings.conf", "w") with no O_NOFOLLOW/O_EXCL/lstat, the daemon runs as root (dcmd.service has no User=), and dcm.c:295 calls umask(0) so a freshly-created file is mode 0666. Both issues are genuine: (a) a pre-planted symlink in /tmp lets root truncate/overwrite an arbitrary file, and (b) the resulting 0666 file lets any local uid rewrite settings later consumed by uploadstblogs/src/strategies.c:75-115 (upload_flag) and other RDK scripts. However, the High rating is overstated. The symlink vector is blocked on any kernel with fs.protected_symlinks=1 (default since Linux 3.6 and standard in Yocto/RDK builds; the reporter admits this). The bytes written via the symlink are NOT attacker-controlled — they come from the XCONF-downloaded JSON — so the attacker gets destructive truncate, not content-controlled write. The 0666-tamper vector only lets a local user flip the log-upload boolean / URL (anti-forensics / redirect telemetry), not achieve code execution or privilege escalation. On an embedded STB with no untrusted interactive users, this is a defense-in-depth weakness rather than a directly exploitable High.

**Exploitability:** Preconditions: attacker must already have local non-root code execution on the STB with write access to /tmp; for the symlink-clobber variant the kernel must have fs.protected_symlinks=0 AND the attacker must create the link before dcmd's first Processconfig (dcmd starts after network-online.target so a post-boot race window exists). Mitigating factors: protected_symlinks is enabled by default on modern RDK/Yocto kernels; /tmp is tmpfs cleared at boot; written content is server-derived, not attacker-chosen; /opt/.DCMSettings.conf is in a non-world-writable directory so only the /tmp path is reachable. Realistic impact: arbitrary root-file truncation (only if protected_symlinks=0) or tampering with telemetry/log-upload configuration via the 0666 file — no direct RCE or privesc.

```c
    FILE *fp_out = fopen(pTempConf, "w");        // pTempConf = "/tmp/DCMSettings.conf"
    if (fp_out == NULL) {
        ret = DCM_FAILURE;
        DCMError("Unable to open tmp file: %s\n", pTempConf);
        goto exit1;
    }

    fp_out_opt = fopen(pOptConf, "w");           // pOptConf = "/opt/.DCMSettings.conf"
```

---

### 19. DCM-019 — Unchecked CRON_INVALID_INSTANT (-1) return from dcmCronParseGetNext() causes infinite tight loop firing root popen()

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-835 |
| **Component** | core-dcm |
| **Location** | `dcm_schedjob.c:83-113` |
| **Input Source** | XCONF cloud JSON (urn:settings:CheckSchedule:cron / urn:settings:LogUploadSettings:UploadSchedule:cron) delivered via RBUS Device.DCM.Setconfig path; or local RBUS publisher pointing confPath at attacker-written JSON |
| **Confidence** | High |

**Description:** dcmSchedulerThread() calls dcmCronParseGetNext() and uses the result to compute the absolute deadline for pthread_cond_timedwait(), but never checks whether the function returned CRON_INVALID_INSTANT ((time_t)-1). dcmCronParseGetNext() returns -1 whenever dcmCronParseDoNext() cannot find a matching date within 4 years (dcm_cronparse.c:815-817), which is reachable with a *valid-parse* but *impossible-date* cron expression such as "0 0 31 2 *" (Feb 31) or "0 0 30 2 *" — both fit in the 16-byte logCron[]/difdCron[] buffers without overflow. When -1 is returned, `_now.tv_sec += (-1 - currentTime)` sets the absolute timeout to tv_sec=-1; glibc pthread_cond_timedwait() returns ETIMEDOUT immediately for a past timestamp. The thread then invokes the callback (dcmRunJobs → snprintf+popen("/bin/sh …/swupdate_utility.sh …") for DCM_FW_UPDATE, or uploadstblogs_run()+popen(uninitialised-pExecBuff) for DCM_LOG_UPLOAD) and loops back, recomputing -1 again, firing the callback in a tight loop with no delay. The result is an unbounded fork-storm of root /bin/sh processes and curl uploads.

**Attack Vector:** Operator-side or MITM attacker (XCONF DCM JSON), OR any local process on the rtrouted RBUS bus, sets `urn:settings:CheckSchedule:cron` (or `urn:settings:LogUploadSettings:UploadSchedule:cron`) to "0 0 31 2 *" in the DCM response JSON. dcmCronParseExp() parses it successfully (5 fields, day 31 < CRON_MAX_DAYS_OF_MONTH=32, month 2 < 13), dcmSchedStartJob() sets startSched=1, and the scheduler thread enters the runaway loop described above as soon as it is woken.

**Impact:** Remote/local DoS — uncontrolled fork loop of root /bin/sh + swupdate_utility.sh and/or repeated full log-upload curl cycles; CPU, PID-table and memory exhaustion; on the LOG_UPLOAD scheduler each iteration also calls popen() on the uninitialised/stale pExecBuff (see dcm.c:113), so this also massively amplifies that separate uninitialised-command-execution bug.

**Verification Notes:** The defect is real and the snippet matches the source. dcm_schedjob.c:86-87 uses dcmCronParseGetNext()'s return value directly in `_now.tv_sec += (timeOffset - currentTime)` with no check for CRON_INVALID_INSTANT, and dcm_cronparse.c:815-817/856 does return (time_t)-1 when the 4-year search is exhausted. "0 0 31 2 *" (10 bytes, fits in difdCron[16]) is accepted by dcmCronParseExp() (day 31 < CRON_MAX_DAYS_OF_MONTH=32, month 2 < 13) so dcmSchedStartJob() sets startSched=1, and the impossible Feb-31 date makes every subsequent dcmCronParseGetNext() return -1, yielding tv_sec=-1; on glibc >= 2.33 __futex_abstimed_wait_common short-circuits negative tv_sec to ETIMEDOUT, so the outer while(1) re-fires dcmRunJobs() forever. The input path is confirmed: dcm_rbus.c:88-94 copies an arbitrary file path from any Device.DCM.Setconfig RBUS publisher, dcm.c:371-376 parses it, and dcm_parseconf.c:224 strcpy()s the JSON cron string verbatim with no validation. However the "unbounded fork-storm" claim is overstated: dcmUtilsSysCmdExec()->popen()/pclose() is synchronous, so each swupdate_utility.sh / uploadstblogs_run() invocation completes before the next — this is continuous serialized root-script execution and CPU spin (plus log-file growth), i.e. an availability/DoS issue, not RCE or privilege escalation.

**Exploitability:** Preconditions: attacker must either (a) control or MITM the operator XCONF DCM JSON (a trusted, normally TLS-protected channel) so that urn:settings:CheckSchedule:cron / urn:settings:LogUploadSettings:UploadSchedule:cron is set to an impossible-date expression such as "0 0 31 2 *", or (b) already have local code execution sufficient to publish a Device.DCM.Setconfig event on the rtrouted RBUS bus pointing confPath at an attacker-written JSON file. No unauthenticated remote vector exists. Mitigating factors: when HAS_MAINTENANCE_MANAGER is enabled and g_bMMEnable is set, dcmRunJobs() returns early so no popen() occurs, though the scheduler thread still busy-spins at 100% CPU repeatedly recomputing the failing cron; popen()/pclose() is blocking so child processes are serialized rather than accumulating; on glibc < 2.33 the negative tv_sec may instead yield EINVAL (thread exits via goto thread_exit) or abort, which is still a DoS but not a runaway loop. Impact is limited to availability (CPU exhaustion, continuous swupdate/log-upload traffic, /opt/logs growth, flash wear) — therefore Medium, not High.

```c
clock_gettime(CLOCK_REALTIME, &_now);
currentTime = _now.tv_sec;

timeOffset = dcmCronParseGetNext(&pDCMSched->parseData, currentTime);
_now.tv_sec += (timeOffset - currentTime);   /* NO check for timeOffset == (time_t)-1 */

while(pDCMSched->startSched && !pDCMSched->terminated) {
    n = pthread_cond_timedwait(&pDCMSched->tCond, &pDCMSched->tMutex, &_now);
    if(n == ETIMEDOUT) { break; }
    ...
}
...
if(n == ETIMEDOUT) {
    DCMInfo("Scheduling %s Job handle: %p\n", pDCMSched->name, pDCMSched->pUserData);
    if(pDCMSched->pDcmCB) {
        pDCMSched->pDcmCB(pDCMSched->name, pDCMSched->pUserData);  /* dcmRunJobs -> popen() as root */
    }
}
```

---

### 20. DCM-020 — Symlink-follow on /tmp/.dcm-daemon.pid fopen("w") — arbitrary file truncate/create as root

| | |
|---|---|
| **Severity** | Medium |
| **CWE** | CWE-59 |
| **Component** | core-dcm |
| **Location** | `dcm_utils.c:163-170` |
| **Input Source** | Local filesystem (world-writable /tmp directory entry / symlink planted by unprivileged user) |
| **Confidence** | High |

**Description:** dcmUtilsCheckDaemonStatus() opens the hard-coded path DCM_PID_FILE ("/tmp/.dcm-daemon.pid", defined in dcm_utils.h:46) for writing with `fopen(path,"w")`. fopen("w") translates to open(O_WRONLY|O_CREAT|O_TRUNC) WITHOUT O_NOFOLLOW or O_EXCL, so symlinks are followed. /tmp is world-writable, and the daemon runs as uid 0 immediately after main() has called umask(0). A local unprivileged user can pre-create `/tmp/.dcm-daemon.pid` as a symlink to any path (e.g. /etc/shadow, /etc/nologin, a systemd unit, an SUID binary). On the next daemon start, root truncates the target and writes the decimal PID into it via fprintf at line 169. If the target does not yet exist it is CREATED with mode 0666 (because of umask(0)), giving the attacker a world-writable root-owned file at an arbitrary location. There is no lstat()/realpath() pre-check and no use of mkstemp/O_TMPFILE.

**Attack Vector:** Local unprivileged user runs `ln -s /etc/passwd /tmp/.dcm-daemon.pid` (or any target) before dcmd starts (boot, crash-restart, or after killing the existing PID file). systemd starts /usr/bin/dcmd as root → dcmDaemonMainInit() → dcmUtilsCheckDaemonStatus() → fopen("/tmp/.dcm-daemon.pid","w") follows the symlink and zeroes /etc/passwd, then writes "<pid>" into it. Pointing the link at a non-existent path under /etc/cron.d, /etc/ld.so.preload, etc. yields a root-owned mode-0666 file the attacker can then freely edit.

**Impact:** Local privilege escalation primitive: arbitrary file truncation as root, and creation of arbitrary world-writable root-owned files (mode 0666 due to umask(0)). Can be leveraged to clobber /etc/shadow (DoS / auth bypass), create writable /etc/ld.so.preload, or destroy security-critical configuration.

**Verification Notes:** Code is confirmed exactly as described: dcm_utils.c:163 calls fopen("/tmp/.dcm-daemon.pid","w") with no O_NOFOLLOW/O_EXCL/lstat guard, and dcm.c:295 sets umask(0) BEFORE dcm.c:317 calls dcmDaemonMainInit()→dcmUtilsCheckDaemonStatus(), so a followed-symlink to a non-existent path would indeed yield a root-owned mode-0666 file. The preceding fopen("r") check at line 146 does not block the attack (a dangling symlink returns NULL and falls through; a link to an existing file yields a non-numeric first line so /proc/<garbage> fails the presence check and still falls through). dcmd.service runs as root with no hardening directives. Notably, sibling components in the same repo (uploadstblogs/src/cleanup_handler.c:388, backup_logs/src/backup_engine.c:411, usbLogUpload/src/usb_log_file_manager.c:126) deliberately use O_NOFOLLOW, confirming this instance is an oversight rather than an intentional design.

**Exploitability:** Preconditions: attacker must already have local unprivileged code execution on the STB and must place the symlink in /tmp before dcmd (re)starts; /tmp is tmpfs so this is a boot-time race, though the unit's After=network-online.target/tr69hostif.service gives a usable window, and a daemon crash/restart reopens it. Major mitigating factor the reporter omitted: the Linux kernel sysctl fs.protected_symlinks=1 — default-on in mainline since 3.6 and shipped enabled by systemd's 50-default.conf — causes root's open() of an unprivileged-owned symlink in sticky /tmp to fail with EACCES, neutralising the attack on typical RDK builds. Because the repo itself does not guarantee that sysctl and the code is objectively wrong, the finding stands, but the combination of (a) likely kernel-level symlink protection, (b) local-only attacker on an embedded device with few non-root principals, and (c) timing requirement warrants downgrading High → Medium rather than the claimed direct root privesc.

```c
    DCMInfo("Opening new pid file\n");
    fp = fopen(DCM_PID_FILE, "w");
    if(fp == NULL) {
        DCMWarn("Failed to open PID file %s\n", DCM_PID_FILE);
        return DCM_FAILURE;
    }

    fprintf(fp, "%d", getpid());
    fclose(fp);

/* dcm_utils.h:46 */
#define DCM_PID_FILE                 "/tmp/.dcm-daemon.pid"
```

---


## Key Risk Themes

1. **Cloud-to-root memory corruption** (DCM-003, DCM-004, DCM-005, DCM-008): The XCONF cloud JSON is parsed with unbounded `strcpy()` into 8/16/128-byte heap fields. A malicious or compromised XCONF server (or MITM) can overflow `cTimeZone[16]` directly into adjacent `cRdkPath`, which is then interpolated into a `popen()` shell command — yielding **remote root command execution**.

2. **RBUS as an unauthenticated local-to-root pivot** (DCM-001, DCM-009, DCM-012, DCM-017): Any local process able to publish on rtrouted can set the config-file path (`Device.DCM.Setconfig`) to an attacker-controlled file, triggering the same overflow chain. `dcmRbusGetT2Version()` does `strcpy(t2_ver[32], <rbus-string>)` — a separate stack overflow primitive.

3. **`umask(0)` + `/tmp` symlink races** (DCM-011, DCM-014, DCM-018, DCM-020, DCM-024): The daemon creates ~12 files in `/tmp` and `/opt` with mode 0666 and follows symlinks. Any local user can pre-create symlinks to clobber/truncate arbitrary root-owned files, or hijack `/tmp/DCMSettings.conf` to suppress/redirect log uploads.

4. **SSRF / log exfiltration** (DCM-022, DCM-023): The upload URL is taken verbatim from RBUS (`LogUploadEndpoint.URL`) or XCONF JSON with no allowlist — device logs (potentially containing PII, MAC addresses, tokens) can be redirected to an attacker server.

5. **Server-response parsing overflows** (DCM-007): `attempt_proxy_fallback()` parses the upload server's S3 response into fixed stack buffers with no bounds checks.

## Recommendations (Priority Order)

1. Replace ALL `strcpy()`/`strcat()`/`sprintf()` with bounded `snprintf()`/`strlcpy()` — particularly `dcm_parseconf.c:224` and `dcm_rbus.c:327`.
2. Remove `umask(0)` from `dcm.c:main()`; use explicit restrictive modes (0600/0700) on all created files.
3. Validate/canonicalize the RBUS-supplied `confPath` against an allowlist directory; reject paths outside `/opt/.t2persistentfolder/`.
4. Use `open(..., O_CREAT|O_EXCL|O_NOFOLLOW)` + `fdopen()` for all `/tmp` file creation; or move state to a daemon-owned `/run/dcmd/` directory.
5. Allowlist upload URL hosts; reject `file://`, internal IPs, and non-operator domains.
6. Replace `popen()`/`system()` with `posix_spawn()`/`execve()` with argv arrays (no shell).
7. Add `User=dcm` + sandboxing directives to `dcmd.service`.

---

*Full findings detail: `Findings.md` · Threat model: `ThreatModel.md` · Input traces: `InputTracing.md` · Rejected FPs: `RejectedFindings.md` · PoCs: `PoC/`*
