---
phase: 13-time-travel
plan: 01
subsystem: core/tt
tags: [determinism, event-log, jit, uart, nvic, tdd]
dependency_graph:
  requires: []
  provides: [ev_log_t, tt_periph_t, run_steps_full_g_jit, jit_reset_counters, bus_find_flat, nvic_set_pending_ext, uart_inject_rx]
  affects: [13-02-snapshot, 13-03-replay, 13-04-tt-core]
tech_stack:
  added: [src/core/tt.c, include/core/tt.h, include/core/run.h, tests/test_tt_determinism.c]
  patterns: [event-log bsearch lower-bound, weak-hook pattern (tt_record_irq/uart_rx), replay-mode gate on uart TX]
key_files:
  created:
    - include/core/tt.h
    - include/core/run.h
    - src/core/tt.c
    - tests/test_tt_determinism.c
  modified:
    - include/core/jit.h
    - src/core/jit.c
    - include/core/bus.h
    - src/core/bus.c
    - include/core/nvic.h
    - src/core/nvic.c
    - include/periph/uart.h
    - src/periph/uart.c
    - src/core/run.c
    - tools/main.c
    - CMakeLists.txt
    - tests/CMakeLists.txt
decisions:
  - "tt_record_irq/uart_rx: weak attribute dropped (MinGW static-lib weak symbol linker gap); strong no-ops in tt.c; 13-04 replaces bodies"
  - "run_steps_full_g renamed to run_steps_full_gdb; new run_steps_full_g(jit_t*) added for TT determinism path"
  - "jit_t instances in test must be static (size ~2MB each; stack overflow on Windows 1MB default stack)"
metrics:
  duration_minutes: 65
  completed: 2026-04-26
  tasks_completed: 3
  tasks_total: 3
  files_created: 4
  files_modified: 12
  tests_before: 5
  tests_after: 6
---

# Phase 13 Plan 01: TT-01 Determinism Kernel Summary

ev_t=16B event log with bsearch seek, jit_t.counters[], bus_find_flat, uart replay-mode gate, nvic_set_pending_ext event hook, and 2-run byte-equal cpu_t regression test.

## Tasks Completed

| Task | Description                                      | Commit  |
|------|--------------------------------------------------|---------|
| 1    | tt.h public types + ev_log_t + run.h             | 46182dc |
| 2    | jit counters into jit_t + bus_find_flat          | 6491c72 |
| 3    | uart replay-mode + nvic ext + determinism test   | 8a75a31 |

## New Files

- `include/core/tt.h` — ev_t (16B, static_assert), ev_log_t, tt_periph_t (uart_t* last field), EVENT_* enum, ev_log_*/tt_record_* decls, g_tt/g_replay_mode externs
- `include/core/run.h` — run_steps_full_g(jit_t*) and run_steps_full/st/plain decls
- `src/core/tt.c` — ev_log_init/free/append/seek (canonical, not weak); g_tt=NULL; g_replay_mode=false; tt_record_irq/uart_rx no-op stubs
- `tests/test_tt_determinism.c` — 2-run interp-only cpu_t+SRAM byte-equal + jit_reset_counters post-reset equality check

## Key Changes

- `jit.h/jit.c`: static u32 counters[JIT_MAX_BLOCKS] removed from jit_run local; moved to jit_t.counters[]; jit_reset_counters() exported
- `bus.h/bus.c`: bus_find_flat(bus_t*, addr_t base) added; linear O(n) scan on REGION_FLAT+base
- `nvic.h/nvic.c`: nvic_set_pending_ext calls tt_record_irq(cycle, irq) when g_tt&&!g_replay_mode
- `uart.h/uart.c`: replay_mode bool + rx_q[64]/rx_head/rx_tail; DR pop from queue in replay; SR RXNE bit gated; TX fputc suppressed in replay; uart_inject_rx push; uart_record_rx stub
- `run.c`: run_steps_full_gdb (gdb path, uses g_jit static); run_steps_full_g (jit_t* param, determinism path); run_steps_full calls _gdb(NULL)

## Exports Verified

```
T ev_log_init
T ev_log_append
T ev_log_seek
T jit_reset_counters
T bus_find_flat
T nvic_set_pending_ext
T uart_inject_rx
```

## Test Results

5 -> 6 tests, all passing. tt_determinism: two identical interp-only runs produce byte-equal cpu_t and SRAM at 10k cycles.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] MinGW weak symbol linker gap in static library**
- Found during: Task 3
- Issue: `__attribute__((weak))` in static lib produces `w` nm entries that don't satisfy undefined references in same archive under MinGW ld
- Fix: tt_record_irq/tt_record_uart_rx changed to strong no-op stubs; 13-04 plan will replace bodies (not override via weak)
- Files modified: src/core/tt.c
- Commit: 8a75a31

**2. [Rule 1 - Bug] run_steps_full_g name collision with gdb path**
- Found during: Task 1
- Issue: existing run_steps_full_g(gdb_t*) conflicts with plan's run_steps_full_g(jit_t*)
- Fix: renamed gdb path to run_steps_full_gdb; added new run_steps_full_g(jit_t*); main.c updated
- Files modified: src/core/run.c, tools/main.c
- Commit: 46182dc

**3. [Rule 1 - Bug] Stack overflow: jit_t ~2MB as local in test**
- Found during: Task 3 (test segfault debug)
- Issue: jit_t has blocks[1024] * ~2KB each = ~2MB; two stack-allocated jit_t exceed Windows 1MB default stack
- Fix: jit_t instances declared static at file scope in test_tt_determinism.c
- Files modified: tests/test_tt_determinism.c
- Commit: 8a75a31

## Self-Check: PASSED

All 4 created files found on disk. All 3 task commits verified in git log. 6/6 tests pass.
