---
phase: 13
status: passed
must_haves_total: 8
must_haves_verified: 8
date: 2026-04-27
re_verification:
  previous_status: gaps_found
  previous_score: 7/8
  gaps_closed:
    - "TT-02: EVENT_ETH_RX now recorded by tt_record_eth_rx (tt.c:61-74) and replayed by tt_inject_event (tt.c:176-184); eth_inject_rx exported from libcortex_m_core.a; test_tt_eth_replay passes"
  gaps_remaining: []
  regressions: []
---

# Phase 13: Time-Travel Kernel Verification Report

**Phase Goal:** Turn `f(state, time, events)` into a fully deterministic, snapshotable, reversible function.
**Verified:** 2026-04-27T00:00:00Z
**Status:** passed — 8/8 must-haves verified
**Re-verification:** Yes — after gap closure plan 13-06. TT-02 closed; no regressions.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | TT-01: Same firmware twice produces byte-equal final state | VERIFIED | test_tt_determinism.c `tt_two_run_equal` memcmp cpu_t + SRAM; ctest pass |
| 2 | TT-02: All I/O events (UART RX, IRQ, ETH frame) recorded with cycle stamp | VERIFIED | tt.c:61-74 `tt_record_eth_rx` calls `ev_log_append(EVENT_ETH_RX, id)`; tt.c:176-184 dispatch calls `eth_inject_rx`; test_tt_eth_replay byte-eq assertion passes |
| 3 | TT-03: snapshot(state) -> blob; restore(blob) reproduces registers/memory | VERIFIED | snap_save/snap_restore with XOR32 checksum; test_tt_snapshot 5 subtests pass |
| 4 | TT-04: Sub-100ms restore for 256 KB SRAM | VERIFIED | test_tt_snapshot `tt_snap_restore_latency` 100 restores, asserts mean < 100 ms |
| 5 | TT-05: replay byte-equal across independent runs | VERIFIED | test_tt_replay `tt_replay_byte_equal` two tt_replay calls memcmp cpu_t + SRAM |
| 6 | TT-06: rewind seeks correctly across 1M+ cycle history under 100 ms | VERIFIED | test_tt_rewind 10 random targets in 1M history, asserts ms_mean < 100.0 |
| 7 | TT-07: step_back(1) whole-instruction granularity ±1 ARM cycle | VERIFIED | test_tt_rewind:108-110 asserts c.cycles in [anchor-1, anchor] |
| 8 | TT-08: All 14 v1.0 firmware tests still pass | VERIFIED | Empirical: firmware/run_all.sh 14/14 pass; ctest 11/11 pass |

**Score:** 8/8 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/core/tt.h` | ev_t 16 B, ev_log_t, tt_periph_t, snap_blob_t, tt_t with frames/n_frames, full API | VERIFIED | `_Static_assert(sizeof(ev_t)==16)` line 32; `eth_frame_t` lines 38-41; `frames/n_frames` in tt_t lines 78-79; `tt_record_eth_rx` declared line 96 |
| `src/core/tt.c` | tt_record_eth_rx + EVENT_ETH_RX dispatch | VERIFIED | tt.c:61-74 records frame into side store, appends ev_log; tt.c:176-184 dispatch calls `eth_inject_rx(p->eth, fr->buf, fr->len)` |
| `src/periph/eth.c` | eth_inject_rx exported | VERIFIED | eth.h:38 declares `void eth_inject_rx(eth_t*, const u8*, u32)`; eth.c:82 implements it; linked into libcortex_m_core.a |
| `tests/test_tt_eth_replay.c` | byte-equal replay test for ETH RX | VERIFIED | Two-replay byte-eq test `tt_eth_replay_byte_equal` (lines 68-136): verifies frame bytes in SRAM, cpu_t memcmp, SRAM memcmp; plus capacity-guard test |
| `src/core/run.c` | run_until_cycle + run_steps_full_g | VERIFIED | Unchanged from initial verification |
| `src/core/jit.c` | jit_reset_counters | VERIFIED | Unchanged |
| `src/periph/uart.c` | uart_inject_rx + replay_mode guard | VERIFIED | Unchanged |
| `src/core/nvic.c` | tt_record_irq wired | VERIFIED | Unchanged |
| `firmware/test_tt/test_tt.bin` | exists, loaded by tt_firmware test | VERIFIED | Unchanged |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| nvic_set_pending_ext | tt.log | tt_record_irq -> ev_log_append | WIRED | nvic.c:30 -> tt.c:53-55 |
| uart receive path | tt.log | tt_record_uart_rx -> ev_log_append | WIRED | uart.c -> tt.c:57-59 |
| eth receive path | tt.log | tt_record_eth_rx -> ev_log_append(EVENT_ETH_RX, id) | WIRED | tt.c:69; side-blob id stored in ev_t.payload |
| tt_inject_event EVENT_ETH_RX | eth_inject_rx | g_tt->frames[e->payload] lookup | WIRED | tt.c:176-183; bounds-checked, calls eth_inject_rx |
| tt_replay / tt_rewind | run_steps_full_g | run_until_cycle | WIRED | tt.c:177, tt.c:251 |
| snap_restore | jit cache flush | jit_reset_counters | WIRED | tt.c:119-120 |

### Requirements Coverage

| Requirement | Status | Notes |
|-------------|--------|-------|
| TT-01: no hidden non-determinism | SATISFIED | No rand/time calls in src/; test passes |
| TT-02: all I/O events recorded | SATISFIED | UART RX, IRQ, ETH frame — all three wired; closed by 13-06 |
| TT-03: snapshot byte-equal restore | SATISFIED | XOR32 checksum; 5 subtests pass |
| TT-04: sub-100ms restore | SATISFIED | memcpy-based; latency criterion met; test asserts < 100 ms |
| TT-05: replay byte-equal across runs | SATISFIED | tt_replay + g_replay_mode; test passes |
| TT-06: rewind O(log n) | SATISFIED | tt_bsearch_le; 1M history < 100 ms |
| TT-07: step_back N ARM cycles | SATISFIED | tt_step_back delegates to tt_rewind; precision test passes |
| TT-08: diff + v1.0 regression | SATISFIED | tt_diff present; 14/14 firmware tests pass |

### Anti-Patterns Found

None blocking. The `_Static_assert(sizeof(ev_t)==16)` guard remains intact after reuse of `ev_t.payload` as `u32 frame_id`. TT-04 implementation uses memcpy rather than mmap/COW as spec text suggested, but the observable sub-100 ms criterion is satisfied; deviation noted, not blocking.

### Human Verification Required

None. All phase success criteria are verifiable programmatically and covered by the passing test suite.

### Gaps Summary

TT-02 closed by gap-closure plan 13-06. `tt_record_eth_rx` now copies inbound frames into the `tt_t.frames[]` side-blob store and appends an `EVENT_ETH_RX` entry to the event log. `tt_inject_event` dispatches that event to `eth_inject_rx` during replay. `test_tt_eth_replay` demonstrates byte-equal state across two independent replays of the same ETH RX event. No regressions in any of the 7 previously verified must-haves.

---

_Verified: 2026-04-27_
_Verifier: Claude (gsd-verifier)_
