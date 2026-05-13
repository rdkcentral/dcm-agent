---
marp: true
theme: default
paginate: true
size: 16:9
style: |
  section {
    font-size: 24px;
  }
  section.title {
    text-align: center;
    font-size: 32px;
  }
  section.title h1 {
    font-size: 52px;
    color: #2563eb;
  }
  section.title h3 {
    color: #64748b;
    font-weight: 400;
  }
  section.split {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 2em;
  }
  table {
    font-size: 20px;
  }
  blockquote {
    border-left: 4px solid #2563eb;
    padding-left: 1em;
    color: #334155;
    background: #f1f5f9;
    border-radius: 4px;
    padding: 0.5em 1em;
  }
  pre {
    font-size: 18px;
    background: #f8fafc;
    border: 1px solid #e2e8f0;
    border-radius: 6px;
  }
  .risk { color: #dc2626; font-weight: bold; }
  .ok { color: #16a34a; font-weight: bold; }
header: "reboot-sequence-sync | dcm-agent"
footer: "Confidential — Internal Review"
---

<!-- _class: title -->
<!-- _paginate: false -->
<!-- _header: "" -->
<!-- _footer: "" -->

# Reboot Sequence Synchronization

### Enforcing Strict Boot-Time Execution Order
### Across `dcm-agent` and `reboot-manager`

&nbsp;

**Change**: `reboot-sequence-sync`
**Date**: 2026-05-06  |  **Status**: Proposal — Team Review

---

## Agenda

&nbsp;

| # | Topic |
|---|-------|
| 1 | Problem Statement & User Story |
| 2 | Current Boot Flow — The Race |
| 3 | Three Synchronization Gaps |
| 4 | The 330s Sleep Problem |
| 5 | Proposed Solution — Sentinel Chain |
| 6 | Sentinel Flow — Happy Path |
| 7 | NTP Guarantee — Explicit Check |
| 8 | Key Design Decisions |
| 9 | Component Impact Map |
| 10 | Error Propagation & Timeout Behavior |
| 11 | Polling Approach Comparison |
| 12 | Testing Strategy |
| 13 | Task Breakdown |
| 14 | Scope Boundary |
| 15 | Before vs. After Summary |

---

## 1. Problem Statement

&nbsp;

> *As a device operator, I want log uploads after reboot to contain the **correct reboot
> reason** and **NTP-accurate archive timestamps**, so that support teams can reliably
> correlate uploaded logs with the actual reboot event.*

&nbsp;

### Three Boot-Time Subsystems — No Ordering Guarantee

| Subsystem | Repository | Responsibility |
|-----------|-----------|----------------|
| **backup_logs** | dcm-agent | Moves live logs → `PreviousLogs/` |
| **update-prev-reboot-info** | reboot-manager | Derives reboot reason → writes `previousreboot.info` |
| **uploadstblogs** (REBOOT) | dcm-agent | Archives `PreviousLogs/` → uploads to log server |

All three start independently at boot. **They race — and the race has real consequences.**

---

## 2. Current Boot Flow — The Race

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              DEVICE BOOT EVENT                                  │
└───────────────┬─────────────────────┬──────────────────────┬────────────────────┘
                │                     │                      │
                ▼                     ▼                      ▼
     ┌──────────────────┐  ┌─────────────────────┐  ┌───────────────────────┐
     │   backup_logs    │  │ update-prev-reboot-  │  │  uploadstblogs        │
     │                  │  │ info                 │  │  (REBOOT trigger)     │
     │  Moving logs...  │  │                      │  │                       │
     │  /opt/logs/ ──►  │  │  Reads PreviousLogs/ │  │  ┌─────────────────┐  │
     │  PreviousLogs/   │  │  ⚠ MAY BE INCOMPLETE │  │  │  sleep(330s)    │  │
     │                  │  │                      │  │  │  (crude guard)  │  │
     │  (still running) │  │  Writes              │  │  └─────────────────┘  │
     │                  │  │  previousreboot.info  │  │                       │
     │                  │  │  ⚠ MAY BE WRONG      │  │  Reads                │
     └──────────────────┘  └─────────────────────┘  │  previousreboot.info   │
                                                     │  ⚠ MAY NOT EXIST      │
                                                     │                       │
                                                     │  time(NULL)           │
                                                     │  ⚠ MAY BE PRE-NTP    │
                                                     └───────────────────────┘
```

**No explicit ordering exists between any of these three processes.**

---

## 3. Three Synchronization Gaps

```
  ┌───────────────┐       ┌──────────────────────┐       ┌───────────────────┐
  │  backup_logs  │       │ update-prev-reboot-  │       │  uploadstblogs    │
  │               │       │ info                 │       │  (REBOOT)         │
  └───────┬───────┘       └──────────┬───────────┘       └─────────┬─────────┘
          │                          │                              │
          │      ╔═══════════╗       │                              │
          ├─────►║  GAP  1   ║──────►│                              │
          │      ║ Backup vs ║       │        ╔═══════════╗         │
          │      ║ Reboot    ║       ├───────►║  GAP  2   ║────────►│
          │      ║ Reason    ║       │        ║ Reboot    ║         │
          │      ╚═══════════╝       │        ║ Reason vs ║         │
          │                          │        ║ Upload    ║         │
          │                          │        ╚═══════════╝         │
          │                          │                              │
          │                          │        ╔═══════════╗         │
          │                          │        ║  GAP  3   ║────────►│
          │                          │        ║ NTP Sync  ║         │
          │                          │        ║ vs Archiv ║         │
          │                          │        ║ Timestamp ║         │
          │                          │        ╚═══════════╝         │
          ▼                          ▼                              ▼
```

| Gap | Between | Risk |
|-----|---------|------|
| **Gap 1** | backup_logs → reboot-reason | Incomplete `PreviousLogs/` → wrong reboot reason |
| **Gap 2** | reboot-reason → upload | `previousreboot.info` absent → wrong upload classification |
| **Gap 3** | NTP sync → archive naming | `time(NULL)` returns pre-NTP epoch → corrupt timestamp in filename |

---

## 4. The 330-Second Sleep Problem

The only existing "guard" is a crude sleep in `reboot_setup()`:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    sleep(330) Analysis                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌─── Fast/Modern Device ──────────────────────────────────┐       │
│   │  backup_logs done in 2s                                 │       │
│   │  reboot-reason done in 5s                               │       │
│   │  NTP sync done in 10s                                   │       │
│   │  ════════════════════════════════════════════            │       │
│   │  uploadstblogs WAITS 330s anyway (5.5 min wasted!)      │       │
│   └─────────────────────────────────────────────────────────┘       │
│                                                                     │
│   ┌─── Slow/Loaded Device ──────────────────────────────────┐       │
│   │  backup_logs done in 120s                               │       │
│   │  reboot-reason done in 200s                             │       │
│   │  NTP sync done in 400s (slow network)                   │       │
│   │  ════════════════════════════════════════════            │       │
│   │  uploadstblogs STARTS at 330s — NTP NOT DONE!           │       │
│   └─────────────────────────────────────────────────────────┘       │
│                                                                     │
│   Problem: sleep is NOT event-driven — fundamentally unreliable     │
│   • Too long on fast devices  →  needlessly delays uploads          │
│   • Too short on slow devices →  race conditions still happen       │
│   • No feedback mechanism     →  can't adapt to actual readiness    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Proposed Solution — Sentinel Chain

Replace the sleep with **sentinel files** — lightweight, volatile, event-driven.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SENTINEL CHAIN (Target State)                        │
│                                                                             │
│                                                                             │
│   ┌──────────────┐   writes    ┌─────────────────────────┐                  │
│   │ backup_logs  │────────────►│  /tmp/.backup_logs_done  │  ◄── NEW        │
│   │  (dcm-agent) │  on success │  (sentinel file)         │                 │
│   └──────────────┘             └───────────┬─────────────┘                  │
│                                            │                                │
│                               polls (1s interval, 60s timeout)              │
│                                            │                                │
│                                            ▼                                │
│   ┌──────────────────────────────────────────────────┐                      │
│   │  update-prev-reboot-info  (reboot-manager)       │                      │
│   │                                                  │                      │
│   │  Also gates on: /tmp/stt_received (NTP - existing)│                     │
│   │  PreviousLogs/ is now STABLE and COMPLETE  ✓     │                      │
│   └──────────────────────┬───────────────────────────┘                      │
│                          │ writes                                           │
│                          ▼                                                  │
│   ┌──────────────────────────────────────────────────┐                      │
│   │  /tmp/Update_rebootInfo_invoked                  │  ◄── EXISTING       │
│   │  (completion sentinel — already in reboot-manager)│     (now leveraged)  │
│   └──────────────────────┬───────────────────────────┘                      │
│                          │                                                  │
│                polls (1s interval, 120s timeout)                             │
│                REPLACES sleep(330s)                                          │
│                          │                                                  │
│                          ▼                                                  │
│   ┌──────────────────────────────────────────────────┐                      │
│   │  uploadstblogs — REBOOT strategy  (dcm-agent)    │                      │
│   │                                                  │                      │
│   │  previousreboot.info is present and correct  ✓   │                      │
│   │  time(NULL) is NTP-accurate                  ✓   │                      │
│   └──────────────────────────────────────────────────┘                      │
│                                                                             │
│   All sentinels in /tmp/ → volatile → auto-cleared on every reboot          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Happy Path — Timing Diagram

```
  TIME ──────────────────────────────────────────────────────────────────────►

  ┌─ backup_logs ─────────────────────┐
  │  init → execute → SUCCESS         │
  │  (~2–30s depending on log volume) │
  └───────────────────────────────────┤
                                      │
                              writes .backup_logs_done
                                      │
                                      ▼
              ┌─ update-prev-reboot-info ───────────────────┐
              │  (was already waiting — poll detects in <1s) │
              │  stt_received: ✓ (NTP done)                  │
              │  .backup_logs_done: ✓ (just appeared)        │
              │  reads PreviousLogs/ → derives reboot reason │
              │  writes previousreboot.info                  │
              └──────────────────────────────────────────────┤
                                                             │
                                               writes Update_rebootInfo_invoked
                                                             │
                                                             ▼
                          ┌─ uploadstblogs REBOOT ─────────────────────────┐
                          │  (was already waiting — poll detects in <1s)    │
                          │  stt_received: ✓ present (explicit check)      │
                          │  Update_rebootInfo_invoked: ✓ present          │
                          │  BOTH met → proceed                            │
                          │                                                │
                          │  archive → upload → cleanup                    │
                          └────────────────────────────────────────────────┘

  TOTAL WAIT: seconds (not 330s!)
  ════════════════════════════════
  Fast devices: upload starts within ~10s of boot
  Slow devices: upload waits exactly as long as needed, no more
```

---

## 7. NTP Guarantee — Explicit Check

**Key insight**: `reboot_setup()` explicitly checks `/tmp/stt_received` alongside
`/tmp/Update_rebootInfo_invoked` — belt-and-suspenders, not just transitive inference.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        NTP GUARANTEE — EXPLICIT CHECK                       │
│                                                                             │
│   ┌──────────────────┐                                                      │
│   │   NTP Daemon      │                                                     │
│   │   syncs clock     │                                                     │
│   └────────┬─────────┘                                                      │
│            │  writes                                                        │
│            ▼                                                                │
│   ┌──────────────────────┐                                                  │
│   │ /tmp/stt_received    │ ← NTP sync complete signal                       │
│   └────────┬─────────────┘                                                  │
│            │                                                                │
│            ├── systemd .path unit triggers update-prev-reboot-info           │
│            │   (transitive path — existing)                                  │
│            │                                                                │
│            └── uploadstblogs checks DIRECTLY  ◄── NEW explicit check        │
│                                                                             │
│   ┌──────────────────────────────────────────┐                              │
│   │  reboot_setup() polls BOTH:              │                              │
│   │                                          │                              │
│   │  ✓ /tmp/stt_received        (NTP sync)   │                              │
│   │  ✓ /tmp/Update_rebootInfo_invoked        │                              │
│   │    (reboot reason ready)                 │                              │
│   │                                          │                              │
│   │  BOTH present → proceed                  │                              │
│   │  EITHER missing at 120s → skip upload    │                              │
│   └──────────────────────────────────────────┘                              │
│                                                                             │
│   WHY EXPLICIT?                                                             │
│   If update-prev-reboot-info pipeline ever stops requiring stt_received,    │
│   the transitive guarantee breaks silently. The explicit check makes NTP    │
│   correctness directly verifiable and self-documenting.                     │
│                                                                             │
│   NOTE: backup_logs runs BEFORE NTP — and that's fine.                      │
│   It doesn't generate timestamps. Only uploadstblogs does.                  │
│                                                                             │
│   NOTE: File mtime inside TAR reflects backup-time (pre-NTP).              │
│   This is expected. Only the archive NAME needs NTP-accurate time.          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Key Design Decisions

&nbsp;

| # | Question | Decision | Rationale |
|:-:|----------|:--------:|-----------|
| 1 | Poll timeout for uploadstblogs? | **120 s** | Shorter than 330s; event-driven so fast devices proceed immediately |
| 2 | Poll interval? | **1 s** | Matches existing codebase pattern; low CPU overhead on embedded |
| 3 | Timeout action — uploadstblogs? | **Skip upload** | Safe degradation — no partial or mis-classified upload |
| 4 | Sentinel file location? | **`/tmp/`** | Volatile — auto-cleared on reboot; zero stale-sentinel risk |
| 5 | Timeout action — reboot-manager? | **Exit with error** | Prevents deriving wrong reboot reason from partial data |
| 6 | Specification scope? | **Single spec** | Both repos serve one boot pipeline; one spec = one truth |
| 7 | NTP for backup_logs? | **Not required** | Must run ASAP at boot; NTP only needed for archive naming (downstream) |

---

## 9. Component Impact Map

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         REPOSITORIES & FILES AFFECTED                       │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────┐       │
│   │  dcm-agent                                                      │       │
│   │                                                                 │       │
│   │   backup_logs/                                                  │       │
│   │   ├── include/backup_logs.h ··········· Add sentinel constant   │       │
│   │   └── src/backup_logs.c ··············· Write sentinel on       │       │
│   │                                         success                 │       │
│   │                                                                 │       │
│   │   uploadstblogs/                                                │       │
│   │   ├── include/uploadstblogs_types.h ·· Add poll constants       │       │
│   │   └── src/strategies.c ··············· Replace sleep(330) with  │       │
│   │                                        sentinel poll            │       │
│   └─────────────────────────────────────────────────────────────────┘       │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────┐       │
│   │  reboot-manager                                                 │       │
│   │                                                                 │       │
│   │   reboot-reason-fetcher/                                        │       │
│   │   ├── include/update-reboot-info.h ··· Add sentinel constant    │       │
│   │   └── src/rebootreason_main.c ········ Poll .backup_logs_done   │       │
│   │                                        before reading           │       │
│   │                                        PreviousLogs/            │       │
│   └─────────────────────────────────────────────────────────────────┘       │
│                                                                             │
│   Change surface: 3 source files, 3 headers, 0 service files, 0 scripts    │
│   1 new sentinel path, 1 explicit NTP check added                           │
│                                                                             │
│   ┌────────────────────────────────────────────────────┐                    │
│   │  CROSS-REPO INTERFACE CONTRACT                     │                    │
│   │  /tmp/.backup_logs_done                            │                    │
│   │  Written by: dcm-agent (backup_logs)               │                    │
│   │  Read by:    reboot-manager (update-prev-reboot)   │                    │
│   │  ⚠ Path changes MUST be released together          │                    │
│   └────────────────────────────────────────────────────┘                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Error Propagation & Timeout Behavior

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FAILURE CASCADES CLEANLY                             │
│                                                                             │
│   SCENARIO: backup_logs FAILS                                               │
│   ═════════════════════════                                                 │
│                                                                             │
│   backup_logs                                                               │
│   ├── execute → FAILURE                                                     │
│   └── .backup_logs_done: ✗ NOT written                                      │
│              │                                                              │
│              ▼                                                              │
│   update-prev-reboot-info                                                   │
│   ├── polls .backup_logs_done for 60s                                       │
│   ├── TIMEOUT → logs error                                                  │
│   ├── exits ERROR_GENERAL                                                   │
│   └── Update_rebootInfo_invoked: ✗ NOT written                              │
│              │                                                              │
│              ▼                                                              │
│   uploadstblogs                                                             │
│   ├── polls Update_rebootInfo_invoked for 120s                              │
│   ├── TIMEOUT → logs error                                                  │
│   ├── returns -1 → strategy handler SKIPS upload                            │
│   └── NO partial upload, NO wrong data sent to server                       │
│              │                                                              │
│              ▼                                                              │
│   ┌──────────────────────────────────────────┐                              │
│   │  Device continues boot normally          │                              │
│   │  Next reboot: /tmp/ cleared → fresh start│                              │
│   └──────────────────────────────────────────┘                              │
│                                                                             │
│   KEY PROPERTY: Failure cascades are SAFE — no corrupt data, no partial     │
│   uploads. The device simply skips the upload for this boot cycle.          │
└─────────────────────────────────────────────────────────────────────────────┘
```

&nbsp;

| Subsystem | Polls for | Interval | Timeout | On Timeout |
|-----------|-----------|:--------:|:-------:|------------|
| update-prev-reboot-info | `/tmp/.backup_logs_done` | 1 s | 60 s | Exit with error |
| uploadstblogs `reboot_setup()` | `/tmp/stt_received` AND `/tmp/Update_rebootInfo_invoked` | 1 s | 120 s | Skip reboot upload |

**Worst-case boot penalty**: 180 s (only when backup or reboot-info fails completely)

---

## 11. Polling Approach — Why `stat()` ?

&nbsp;

| Criterion | `inotify` | `stat()` polling |
|-----------|:---------:|:----------------:|
| POSIX portability | Linux-only | **All target platforms** |
| Embedded resource cost | Higher (kernel watch descriptors) | **Minimal** |
| Already in codebase? | No | **Yes** (`dcm_utils.c` pattern) |
| Code complexity | Higher (event loop, fd mgmt) | **Simple loop** |
| Timing accuracy | Event-exact | **±1 s** (acceptable) |
| Signal interruption risk | fd-based — complex | **Benign** (re-checks next iteration) |

&nbsp;

**Decision**: `stat()` polling — consistent with existing codebase, portable, minimal.

---

## 12. Testing Strategy

### Unit Tests (per module)

&nbsp;

| ID | Verifies | Module | Type |
|----|----------|--------|------|
| TEST-SYNC-001 | Sentinel written on success, absent on failure | backup_logs | GTest |
| TEST-SYNC-002 | Proceeds when sentinel present | reboot-manager | GTest |
| TEST-SYNC-003 | Exits error after timeout (no sentinel) | reboot-manager | GTest |
| TEST-SYNC-004 | `reboot_setup()` proceeds when both sentinels present | uploadstblogs | GTest |
| TEST-SYNC-005 | `reboot_setup()` skips upload if either sentinel missing at timeout | uploadstblogs | GTest |
| TEST-SYNC-006 | Archive timestamp is post-NTP (mocked `time()`) | uploadstblogs | GTest |

### L2 Integration (Docker CI)

| Step | Verification |
|------|-------------|
| Run backup_logs | `.backup_logs_done` created |
| Run update-prev-reboot-info | Reads PreviousLogs/ correctly; writes `Update_rebootInfo_invoked` |
| Run uploadstblogs REBOOT | Proceeds immediately (no 330 s sleep) |
| Inject failure: no `.backup_logs_done` | update-prev-reboot-info exits error |
| Inject timeout: no `Update_rebootInfo_invoked` | Upload skipped cleanly |

---

## 13. Task Breakdown

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        IMPLEMENTATION GROUPS                                │
│                                                                             │
│   GROUP A — Constants (both repos, parallel)                                │
│   ┌──────────────────────────────────┬──────────────────────────────────┐   │
│   │ A1: backup_logs.h                │ A2: update-reboot-info.h        │   │
│   │     sentinel constant            │     sentinel + poll constants   │   │
│   │     [dcm-agent]                  │     [reboot-manager]            │   │
│   └──────────────┬───────────────────┴──────────────┬───────────────────┘  │
│                  │                                   │                      │
│                  ▼                                   ▼                      │
│   GROUP B                            GROUP C                               │
│   ┌──────────────────────┐           ┌──────────────────────────────┐      │
│   │ B1: Write sentinel   │           │ C1: Add poll helper          │      │
│   │     in backup_logs   │           │ C2: Gate PreviousLogs/ read  │      │
│   │     [dcm-agent]      │           │     [reboot-manager]         │      │
│   └──────────────────────┘           └──────────────────────────────┘      │
│                                                                             │
│   GROUP D                                                                   │
│   ┌──────────────────────────────────────────────────┐                      │
│   │ D1: Add poll constants + sentinel paths         │                      │
│   │ D2: Replace sleep(330) with dual-sentinel poll   │                      │
│   │     [dcm-agent]                                  │                      │
│   └──────────────────────────────────────────────────┘                      │
│                                                                             │
│   GROUP E — Documentation (parallel, any time)                              │
│   ┌──────────────────────────────────┬──────────────────────────────────┐   │
│   │ E1: dcm-agent DEPENDENCIES.md   │ E2: reboot-mgr DEPENDENCIES.md  │   │
│   └──────────────────────────────────┴──────────────────────────────────┘   │
│                                                                             │
│   GROUP F — Tests (after implementation)                                    │
│   ┌──────────┬──────────┬──────────┬──────────────┐                         │
│   │ F1: Unit │ F2: Unit │ F3: Unit │ F4: L2 E2E   │                         │
│   │ backup   │ reboot-  │ upload-  │ full chain   │                         │
│   │ _logs    │ manager  │ stblogs  │ integration  │                         │
│   └──────────┴──────────┴──────────┴──────────────┘                         │
│                                                                             │
│   TOTAL: 14 tasks  •  2 repositories  •  6 groups                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 14. What We Are NOT Changing

&nbsp;

| Area | In scope? | Reason |
|------|:---------:|--------|
| `TRIGGER_DCM` upload path | No | Only REBOOT trigger has the race |
| `TRIGGER_ONDEMAND` path | No | User-triggered; not a boot-sequence concern |
| Scheduled log upload | No | Different code path entirely |
| NTP subsystem | No | We consume its signal, don't change it |
| Systemd service/path units | No | Existing `.service` / `.path` files unchanged |
| `backup_logs` copy logic | No | Only sentinel write added at end |
| Shell scripts (legacy) | No | C binary paths only (future task for script parity) |

---

## 15. Before vs. After

&nbsp;

| Dimension | Before | After |
|-----------|--------|-------|
| **Ordering** | None — three concurrent processes | Strict chain via sentinel files |
| **Guard mechanism** | `sleep(330s)` — fixed, blind | Event-driven poll — reacts to actual completion |
| **Reboot reason** | May be wrong (partial PreviousLogs/) | Always correct (backup confirmed done first) |
| **Archive timestamp** | May be pre-NTP | Guaranteed NTP-accurate (explicit `/tmp/stt_received` check) |
| **Fast device boot** | Wastes ~5 min sleeping | Upload starts within seconds of readiness |
| **Slow device boot** | May still race (330s not enough) | Waits exactly as long as needed |
| **Failure mode** | Undefined — may upload corrupt data | Clean: timeout → skip → log error → continue boot |
| **backup_logs NTP dep** | None | None (unchanged — NTP-free by design) |
| **Change surface** | — | 3 source files, 3 headers, 1 new sentinel path, 1 explicit NTP check |

&nbsp;

> **One new sentinel. Two existing sentinels leveraged (one explicitly). Zero new IPC mechanisms.**
> Pattern identical to sentinels already in both codebases.

---

<!-- _class: title -->
<!-- _paginate: false -->
<!-- _header: "" -->

# Questions & Discussion

```
                              SENTINEL CHAIN RECAP
   ═══════════════════════════════════════════════════════════════════

     backup_logs
       └──► /tmp/.backup_logs_done             NEW — dcm-agent
                 │
     update-prev-reboot-info
       ├── polls .backup_logs_done             NEW — reboot-manager
       └──► /tmp/Update_rebootInfo_invoked     EXISTING — now leveraged
                 │
     uploadstblogs reboot_setup()
       ├── polls stt_received              NEW — explicit NTP check
       └── polls Update_rebootInfo_invoked     REPLACES sleep(330)
       BOTH present → proceed
```

&nbsp;

**Artifacts**: `openspec/changes/reboot-sequence-sync/`
`proposal.md` — What and why  |  `spec.md` — Requirements (REQ-SYNC-001 – 007)
`design.md` — Technical design  |  `tasks.md` — 14 tasks with dependencies
