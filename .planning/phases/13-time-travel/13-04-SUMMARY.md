---
phase: 13-time-travel
plan: 04
subsystem: core/tt-rewind
tags: [time-travel, rewind, bsearch, step-back, diff, snapshot, lifecycle]
dependency_graph:
  requires:
    - phase: 13-01
      provides: ev_log_t, tt_periph_t, run_steps_full_g, g_tt, g_replay_mode
    - phase: 13-02
      provides: snap_blob_t, snap_save, snap_restore, g_jit_for_tt, SRAM_BASE_ADDR, SRAM_SIZE
    - phase: 13-03
      provides: ev_log_seek, tt_inject_event, run_until_cycle, tt_replay
  provides:
    - tt_t struct (stride/max_snaps/snaps[]/idx[]/n_snaps/ev_log_t log)
    - tt_create / tt_destroy
    - tt_on_cycle O(1) stride-based snap policy
    - tt_rewind O(log n) bsearch_le + snap_restore + run_until_cycle
    - tt_step_back N cycles backward, whole-instruction granularity
    - tt_diff register + SRAM range-encoded delta printer
    - tt_attach_jit g_jit_for_tt wiring
    - snap_entry_t (cycle/snap_idx parallel index)
    - tt_record_irq / tt_record_uart_rx strong ev_log_append overrides
  affects: [13-05-firmware-integration, user-facing rewind API]
tech_stack:
  added:
    - tests/test_tt_rewind.c
  patterns:
    - bsearch lower-bound O(log n) snap lookup (tt_bsearch_le)
    - stride-based O(1) snap policy in tt_on_cycle
    - rewind = bsearch_le + memcpy snap_restore + run_until_cycle forward replay
    - SRAM range-encoded diff: contiguous changed byte spans reported as [lo..hi]
key_files:
  created:
    - tests/test_tt_rewind.c
  modified:
    - include/core/tt.h
    - src/core/tt.c
    - tests/CMakeLists.txt
decisions:
  - "tt_record_irq/uart_rx bodies replaced (not overridden): no-op stubs from 13-01 removed, new ev_log_append bodies written in-place (MinGW static-lib weak symbol gap)"
  - "snap_entry_t defined alongside tt_t in tt.h: forward decl of snap_blob_t retained for pointer field in tt_t; full snap_blob_t def follows in snapshot section"
  - "targets >= stride in TT-06 test: tt_bsearch_le returns -1 when no snap exists <= target; target=100 with stride=10000 is a correct reject; test adjusted to use targets >= 10001"
  - "tt_diff SRAM loop uses j/k locals (not i/j) to avoid shadow of outer loop var i in tt.h cpp context"
  - "FILE* in tt_diff requires stdio.h in tt.h header; added <stdio.h> include"
metrics:
  duration_minutes: 35
  completed: 2026-04-26
  tasks_completed: 2
  tasks_total: 2
  files_created: 1
  files_modified: 3
  tests_before: 8
  tests_after: 9
---

# Phase 13 Plan 04: TT-06/07/08 tt_t Lifecycle + Bsearch Rewind + step_back + diff Summary

tt_t struct with stride/max_snaps/snap store/parallel cycle index/ev_log: tt_create/destroy/on_cycle/rewind/step_back/diff/attach_jit all exported; bsearch_le O(log n) snap lookup; SRAM range-encoded diff; TT-06 mean rewind 0.3ms at 1M history (budget 100ms).

## Tasks Completed

| Task | Description                                               | Commit  |
|------|-----------------------------------------------------------|---------|
| 1    | tt_t lifecycle + on_cycle + rewind + step_back + diff     | 011be1a |
| 2    | TT-06/07/08 test_tt_rewind                                | c1f582e |

## New Files

- `tests/test_tt_rewind.c` — TT-06 latency (10 rewds <100ms), TT-07 step_back precision (+-1 ARM cycle), TT-08 diff output ("R0:" + "SRAM[0x20000100")

## Key Changes

- `include/core/tt.h`:
  - Added `<stdio.h>` include (for `FILE*` in `tt_diff`)
  - Replaced `typedef struct tt_s tt_t` forward decl with full `tt_t` struct definition (stride, max_snaps, snaps[], idx[], n_snaps, ev_log_t log)
  - Added `snap_entry_t` typedef (u64 cycle, u32 snap_idx)
  - Added declarations: tt_create, tt_destroy, tt_on_cycle, tt_rewind, tt_step_back, tt_diff, tt_attach_jit

- `src/core/tt.c`:
  - `tt_record_irq` / `tt_record_uart_rx`: no-op stubs replaced with `ev_log_append(&g_tt->log, ...)` (strong override of 13-01 stubs)
  - `tt_create`: calloc tt_t + snaps[] + idx[], ev_log_init(16), sets g_tt
  - `tt_destroy`: frees snaps/idx/log, clears g_tt
  - `tt_on_cycle`: O(1) `cycles % stride == 0` check, snap_save to ring slot, idx entry
  - `tt_attach_jit`: assigns g_jit_for_tt
  - `tt_bsearch_le` (static): binary search for largest idx[i].cycle <= target; returns -1 if none
  - `tt_rewind`: bsearch_le -> snap_restore -> ev_log_seek -> run_until_cycle under g_replay_mode
  - `tt_step_back`: tt_rewind(c->cycles - N); inherits whole-instruction granularity
  - `tt_diff`: prints reg[0..15] + APSR deltas; SRAM contiguous range scan -> "SRAM[0xAAAA..0xBBBB]: N bytes differ"

## Exports Verified

```
tt_create     (scl 2, text)
tt_destroy    (scl 2, text)
tt_on_cycle   (scl 2, text)
tt_rewind     (scl 2, text)
tt_step_back  (scl 2, text)
tt_diff       (scl 2, text)
tt_attach_jit (scl 2, text)
```

## TT-06/07/08 Results

- TT-06: 10 random rewds across 1M-cycle history, mean 0.3ms (budget: <100ms). Math: bsearch 7 cmps + 0.14ms memcpy + worst-case 10K cycles replay.
- TT-07: step_back(1) arrives in [anchor-1, anchor]; step_back(100) chain within [anchor2-101, anchor2-100+1]. Whole-instruction granularity: 2-cycle ops can cause +1 cycle overshoot.
- TT-08: tt_diff on two snaps differing in R0 + 4 SRAM bytes produces output containing "R0:" and "SRAM[0x20000100".

## Test Results

8 -> 9 tests, all passing. test_tt_rewind: 36/36 assertions.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Target 100 in TT-06 test below first snap boundary**
- Found during: Task 2 (test run)
- Issue: stride=10000 means first snap at cycle 10000; target=100 has no snap <= it, so tt_bsearch_le returns -1 and tt_rewind correctly returns false; test assertion failed
- Fix: Replaced target `100` with `10001` in the test's targets[] array (smallest valid target above first snap)
- Files modified: tests/test_tt_rewind.c
- Committed in: c1f582e

**2. [Rule 2 - Missing] stdio.h include in tt.h for FILE***
- Found during: Task 1 (build)
- Issue: tt_diff declaration uses FILE* but tt.h had no stdio.h include; including C code that includes tt.h would get an implicit-declaration warning or error on FILE*
- Fix: Added `#include <stdio.h>` at top of include/core/tt.h
- Files modified: include/core/tt.h
- Committed in: 011be1a

## Self-Check: PASSED
