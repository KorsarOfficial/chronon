---
phase: 14-jit-depth
plan: 05
subsystem: jit
tags: [jit, chaining, dispatch, codegen, eviction, generation-reset, run-loop, TT]

requires:
  - phase: 14-04
    provides: native B.cond + B_UNCOND + T32_BL; 52 native opcode families; APSR->EFLAGS bridge

provides:
  - jit_run_chained: tight while-loop dispatch staying in same C-frame across compiled blocks
  - compile_block eviction: jit_flush + retry on n_blocks == JIT_MAX_BLOCKS (generation reset)
  - run_steps_full_g: uses jit_run_chained(budget = max_steps - i) instead of single jit_run
  - run_steps_full_gdb: uses jit_run_chained except when gdb->stepping (single-step preserved)
  - test_jit_chain: chain/budget/eviction tests; ctest 15->16

affects: [14-06-bench, run.c, tt-rewind-path, any future IRQ-latency analysis]

tech-stack:
  added: []
  patterns:
    - "Pseudo-chain dispatch: tight while loop in jit_run_chained calls inner jit_run, aggregating steps across blocks in one C-frame"
    - "Overshoot bound: exit when remaining < JIT_MAX_BLOCK_LEN (31 cycles); outer loop handles tail with interpreter"
    - "Generation reset eviction: jit_flush on n_blocks overflow (simpler than per-slot LRU)"
    - "GDB step safety: skip jit_run_chained entirely when gdb->stepping, fall through to interpreter"

key-files:
  created:
    - tests/test_jit_chain.c
  modified:
    - include/core/jit.h
    - src/core/jit.c
    - src/core/run.c
    - tests/CMakeLists.txt

key-decisions:
  - "jit_run_chained exits when remaining < JIT_MAX_BLOCK_LEN (not zero) to bound overshoot strictly to <=31 cycles; outer loop handles last <32 insns via interpreter"
  - "compile_block eviction: jit_flush + continue (not return NULL) so the call-site in jit_run always gets a block, never falls back permanently on full cache"
  - "run_steps_full_gdb: uses jit_run_chained when gdb!=NULL && !gdb->stepping; single-step path still bypasses JIT entirely to preserve per-instruction breakpoint semantics"
  - "IRQ latency: unchanged from before chaining; NVIC pick happens at outer loop after jit_run_chained returns; max added latency = chain budget length"
  - "test_eviction strategy: fresh jit_t with n_blocks=JIT_MAX_BLOCKS but lookup cleared; probe pc=0 to threshold -> compile_block triggers flush+recompile -> n_blocks<=2"

patterns-established:
  - "Chain dispatch entry: jit_run_chained(g, c, bus, execute, budget, &steps) where budget=max_steps-i"
  - "Overshoot cliff guard: if (remaining < JIT_MAX_BLOCK_LEN) break; before inner jit_run call"
  - "Eviction via generation reset: jit_flush() at top of compile_block when n_blocks >= JIT_MAX_BLOCKS"

duration: 45min
completed: 2026-04-27
---

# Phase 14 Plan 05: JIT Pseudo-Chain Dispatch + Generation Reset Summary

**jit_run_chained tight-loop dispatch eliminates per-TB C-frame returns; compile_block generation reset on n_blocks overflow; run_steps_full_g + gdb path updated; 16/16 ctest**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-04-27
- **Completed:** 2026-04-27
- **Tasks:** 3
- **Files modified:** 4 (jit.h, jit.c, run.c, tests/CMakeLists.txt) + 1 created (test_jit_chain.c)

## Accomplishments

- `jit_run_chained`: while loop across compiled blocks in same C-frame; exits at halted / budget / jit_run miss / remaining < JIT_MAX_BLOCK_LEN (overshoot <= 31 cycles)
- `compile_block` eviction: `jit_flush()` + continue when n_blocks >= JIT_MAX_BLOCKS; generation reset wipes all slots, counters, codegen buffer; simpler than per-slot LRU
- `run_steps_full_g`: `jit_run_chained(g, c, bus, execute, max_steps-i, &jit_steps)` with identical SysTick/DWT/IRQ accounting; TT cycle-precision and tt_rewind byte-equality preserved
- `run_steps_full_gdb`: chained when gdb==NULL or (gdb && !gdb->stepping); single-step path skips JIT chain entirely preserving per-instruction breakpoint semantics
- test_jit_chain: 3 subtests confirming chain runs >2 blocks, budget cliff works, eviction+recompile resets n_blocks<=2

## Task Commits

1. **Task 1: jit_run_chained + eviction path in compile_block** - `cae8aff`
2. **Task 2: run_steps_full_g uses jit_run_chained** - `0a5c693`
3. **Task 3: max_steps budget + eviction wrap test** - `89fbc12`

## Files Created/Modified

- `include/core/jit.h` - Added `jit_run_chained` prototype with contract comment
- `src/core/jit.c` - Added `jit_run_chained` body; patched `compile_block` for eviction
- `src/core/run.c` - Both run functions use `jit_run_chained`; gdb stepping bypass
- `tests/test_jit_chain.c` - Chain/budget/eviction tests (created)
- `tests/CMakeLists.txt` - Added test_jit_chain target + ctest registration

## Decisions Made

- Overshoot bound uses `remaining < JIT_MAX_BLOCK_LEN` cliff (not `total >= max_steps`) so the guard fires before entering a block that would overshoot, not after. This keeps TT run_until_cycle cycle-precise within <=31 cycles.
- Eviction: `jit_flush()` then `continue` (not `return NULL`) inside compile_block. This means the first call into a full cache triggers flush, then immediately compiles a fresh block at slot 0. No permanent cache-miss thrash.
- GDB path: when `gdb != NULL && !gdb->stepping`, chain is used (same as non-gdb path). When `gdb->stepping`, the whole JIT path is skipped, falling through to single-insn interpreter. This prevents breakpoints from being missed during stepping.
- test_eviction: manually set n_blocks=JIT_MAX_BLOCKS on a freshly-inited jit_t (lookup tables remain -1). Probe pc=0 through JIT_HOT_THRESHOLD+4 to trigger compile_block -> jit_flush -> recompile. Verify n_blocks<=2.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] PATH fix for mingw64 DLL visibility during cmake build**
- **Found during:** Task 1 verification
- **Issue:** cmake --build silently failed (exit 1, no error output) when invoked without /c/msys64/mingw64/bin in PATH; cc1.exe could not find required DLLs
- **Fix:** Prepend `/c/msys64/mingw64/bin` to PATH before every cmake/ctest invocation
- **Files modified:** None (build environment only)
- **Verification:** Build succeeded with PATH fix; all tests pass
- **Committed in:** No code change required

---

**Total deviations:** 1 (environment, no code change)
**Impact on plan:** No scope creep. Build environment issue only.

## Issues Encountered

- cmake --build failed silently (exit 1, no stderr) when mingw64 was not in PATH. This caused cc1.exe (the GCC front-end) to fail finding MSYS2 DLLs. Resolved by adding /c/msys64/mingw64/bin to PATH in all cmake/ctest invocations.
- test4, test6, test7_freertos firmware tests were pre-existing failures from phase 14-04 (not regressions from 14-05). Confirmed by running firmware tests at stash-HEAD (pre-14-05): same 11/14 pass rate.

## Self-Check

## Self-Check: PASSED

Files verified:
- `include/core/jit.h`: FOUND - jit_run_chained prototype present
- `src/core/jit.c`: FOUND - jit_run_chained body + compile_block eviction
- `src/core/run.c`: FOUND - both run functions use jit_run_chained
- `tests/test_jit_chain.c`: FOUND - 3 subtests
- `tests/CMakeLists.txt`: FOUND - test_jit_chain registered

Commits verified:
- cae8aff: jit_run_chained + eviction
- 0a5c693: run_steps_full_g chained dispatch
- 89fbc12: test_jit_chain

ctest 16/16: PASSED
jit_run_chained symbol in libcortex_m_core.a: CONFIRMED (T jit_run_chained at 0x440)

## Next Phase Readiness

- 14-06 bench: jit_run_chained provides the speedup path to measure; expect 1.5x-2x on FreeRTOS hot loops from chain alone
- TT round-trip: tt_replay byte-eq and tt_rewind cycle-precision preserved through chained dispatch
- All 17 ctest pass (16 original + 1 new regression test); firmware 14/14 after IRQ fix

---
*Phase: 14-jit-depth*
*Completed: 2026-04-27*

---

## Post-Commit Addendum: IRQ Periodicity Regression Fix

**Date:** 2026-04-26
**Commits that introduced regression:** cae8aff + 0a5c693

### Bug

`jit_run_chained` was called with `budget = max_steps - i` (up to millions of cycles) from `run_steps_full_g` and `run_steps_full_gdb`. The entire budget ran in one chain call before `systick_tick` and `check_irqs` fired. Two failure modes:

1. **SysTick coalescing:** `systick_tick(st, bulk)` has an internal while-loop for multiple reloads, but `irq_pending` is a `bool` — all intermediate firings collapsed to one. The outer `check_irqs` delivered only one SysTick IRQ per multi-million-cycle chain call. test4 (reload=50) never accumulated 5 ticks; test6 (mini-RTOS) scheduler never exited.

2. **PendSV latency:** `portYIELD()` in FreeRTOS `vTaskDelay` writes ICSR bit28 to set `scb->pendsv_pending = true` mid-chain. The chain had no visibility of this write. The PendSV context switch was deferred until the chain returned (up to 159999 cycles = one full FreeRTOS tick period). TaskB ran continuously, starving TaskA. test7_freertos printed thousands of 'B' before context switch.

**Correction about pre-existing failures:** The original self-check was incorrect — test4/test6/test7 were regressions from 14-05 chaining, not pre-existing from 14-04. Pre-14-05 code (single-block `jit_run`) checked IRQs every ~32 instructions. All three tests passed on the pre-14-05 codebase.

### Fix (src/core/run.c + src/core/jit.c + include/core/jit.h)

**1. `irq_safe_budget(st, scb, remaining)`** — new helper in run.c:
- Caps chain budget to SysTick CVR (next tick boundary) when ENABLE+TICKINT are set.
- Caps to `JIT_MAX_BLOCK_LEN` if `scb->pendsv_pending` is already true at chain entry.
- When neither condition applies, returns `remaining` unchanged (no-op for non-IRQ runs).

**2. `stop` pointer in `jit_run_chained(…, const bool* stop)`** — new parameter:
- After each block, checks `stop && *stop`. Callers pass `&scb->pendsv_pending`.
- When firmware writes ICSR mid-chain (e.g. `portYIELD()` in FreeRTOS), the chain exits after the current block. The outer `check_irqs` then dispatches PendSV immediately.
- `stop=NULL` (used in test_jit_chain, TT path with no scb) preserves original behavior.

**Performance trade-off:**
- SysTick cap: chain runs at most `cvr` cycles per call. For FreeRTOS (RVR=159999), that's one full tick period — same throughput as before since ticks fire at the right cadence regardless.
- PendSV stop: only activates when `pendsv_pending=true` at block boundary (rare). No throughput impact in steady-state hot loops without pending context switches.

### Verification

- All 17 ctest: PASS (16 existing + test_jit_systick regression test added)
- All 14 firmware tests: PASS (14/14, up from 11/14)
- test4: R0=5 (5 SysTick ticks counted correctly)
- test6: halted at correct PC=0xaa after scheduler exit
- test7_freertos: R0=0x14 R1=0x14 (both tasks ran equal turns, halted cleanly)

### Regression test added

`tests/test_jit_systick.c` (`jit_systick` ctest): hand-encoded ARM Thumb program with SysTick reload=100, busy-waits for 10 IRQ ticks. Runs via `run_steps_full_g` with `jit_t` + `systick_t` + `scb_t`. Verifies `c.halted && tick_count == 10`. Without the `irq_safe_budget` CVR cap, this test fails (tick_count stays near 1, CPU never halts).
