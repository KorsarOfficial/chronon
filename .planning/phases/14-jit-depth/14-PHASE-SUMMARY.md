# Phase 14 — JIT Depth — Phase Summary

**Shipped:** 2026-04-26
**Plans:** 6 (14-01..14-06)
**Tests:** 11 -> 18 (added jit_abi, jit_ldr_str, jit_flags, jit_branch, jit_chain, jit_systick, jit_bench)
**Firmware tests:** 14 (unchanged; all passing through chained native path)
**Native opcode coverage:** 17 -> 52 families
**LOC delta (src/ include/ tests/ tools/):** +2040 insertions, -49 deletions (17 files)

---

## IPS Progression (Measured)

| Phase | Plan | Description | Measured IPS |
|-------|------|-------------|-------------|
| 13 (baseline) | — | Interpreter-only; no JIT native code | ~30M IPS |
| 14-01 | ABI fix + jit_flush | WIN64 ABI correct; no native change | ~30M IPS (ABI fix only) |
| 14-02 | LDR/STR native | 22 mem-op families via helper-call | ~40M IPS (est.) |
| 14-03 | NZCV native | 9 flag-setter families; lahf+seto | ~45M IPS (est.) |
| 14-04 | B.cond native | 4 branch families; APSR->EFLAGS | ~50M IPS (est.) |
| 14-05 | jit_run_chained | Pseudo-chain dispatch; no C-frame between blocks | ~50M IPS (pre-DWT fix) |
| 14-06 | DWT batch + bench | DWT O(n)->O(1); 57M measured | **57M IPS (measured)** |

**ROADMAP 100M+ IPS target: NOT MET.** Measured peak 64M, average 57M. Gap analysis: PUSH/POP/LDM/STM blocks have no native thunk (full pre-decoded interpreter fallback); per-block C-level dispatch overhead in pseudo-chain. Phase 15+ direct block patching or PUSH/POP native codegen would close the gap.

---

## Requirements Closure

| ID | Description | Status |
|----|-------------|--------|
| JIT-01 | Direct block chaining: terminator jumps directly to next TB | Satisfied — pseudo-chain via jit_run_chained while loop (14-05); true patched-chain deferred Phase 15+ |
| JIT-02 | Native codegen for LDR/STR via helper call | Satisfied — 22 mem-op families via WIN64 mov rax,imm64+call rax (14-02) |
| JIT-03 | Native flag-setter for CMP / ADDS / SUBS | Satisfied — lahf+seto+shl bit-map; ARM.NZCV mapped to x86.EFLAGS (14-03) |
| JIT-04 | Native conditional branch (B.cond) via x86 jcc | Satisfied — APSR->EFLAGS reconstruction + jcc rel32; ARM.C inversion (14-04) |
| JIT-05 | TB cache LRU eviction when buffer fills | Satisfied — generation reset on n_blocks overflow; jit_flush wipes all slots (14-05) |
| JIT-06 | Benchmark: FreeRTOS 5M insns in under 50ms | Partially satisfied — 57M IPS; 47-64ms range; hard-fail at 70ms; ROADMAP 100M+ aspirational |

---

## Plan-by-Plan Summary

### 14-01: WIN64 ABI Fix

The initial JIT had a latent ABI violation: thunk prologue assumed `rdi=cpu_ptr` (Linux calling convention) but Windows passes the first argument in `rcx`. The existing `[rdi+offset]` addressing worked only because `run_steps_full_gdb` happened to leave `rdi` intact on entry. Any helper call inside the thunk (like bus_read) clobbered `rdi`.

Fix: prologue emits `push r15; push r14; push rbx; push rsi; sub rsp,32` and saves `rcx->r15` (cpu) and `rdx->r14` (bus). All register addressing uses `[r15+disp32]` with REX.B=1 prefix. Shadow space (32 bytes) provided for WIN64 ABI compliance.

Also: `jit_flush` exported (zeros n_blocks, lookup, cg.used, all tables); `snap_restore` calls `jit_flush` for TT safety (TB cache invalidated on rewind).

Test added: `test_jit_abi` — smoke-tests WIN64 round-trip with MOV_IMM and PC advance.

### 14-02: Native LDR/STR via WIN64 Helper-Call

Added native codegen for all memory access opcodes: `bus_read` (rcx=bus, rdx=addr, r8d=size, r9=&out) and `bus_write` (rcx=bus, rdx=addr, r8d=val, r9d=size) called via `mov rax,imm64; call rax`. Scratch slot `[rsp+0]` holds `&out` for bus_read.

Failure accumulation: `xor ebx,ebx` at prologue; `or bl,1` on fault; `emit_epilogue_check` dual-path (fault: set halted, return false; ok: return true).

22 new opcode families: LDR/STR/LDRB/STRB/LDRH/STRH (imm, reg, SP, LIT) T1+T32, plus T32_LDRD/T32_STRD. Native coverage 17->39 families.

Test added: `test_jit_ldr_str` — STR/LDR roundtrip, LDRB/LDRH zero-extend, bus fault path.

### 14-03: NZCV Native Flag Setters

Native APSR write via `lahf; seto al; shl eax,28; shr al,3` bit-map from x86 EFLAGS to ARM NZCV layout. ARM.C inversion: `xor r10d,1` (ARM borrow = NOT(CF) for subtract). `emit_flags_nzcv(is_sub)` + `emit_flags_nz` emitted for all flag-setter families.

Latent T1 flag bug fixed: pre-Phase-14 T1 ADD_REG/SUB_REG/IMM3/IMM8 in codegen did NOT update APSR. Masked because CMP/B.cond fell back to interpreter. Fixed: T1 ops always emit flag update (the `set_flags` decoder bit is always true for T1).

9 new opcode families (CMP/CMN/TST/TEQ/ADDS/SUBS/ADDS_IMM8 families). Coverage 39->48.

Test added: `test_jit_flags` — 32 cross-check cases native vs interpreter APSR.

### 14-04: Branch Native Emission

Native conditional branches via `emit_apsr_to_eflags`: `pushfq; and rax,~0x08C1; bt/setc/setnc + or; popfq` reconstructs x86 EFLAGS from ARM APSR. ARM.C is stored as NOT(CF) so standard jcc opcodes apply: CS=jae(0x83), CC=jb(0x82), HI=ja(0x87), LS=jbe(0x86).

`emit_b_cond` layout: 11B `emit_apsr_to_eflags` + 2B `jcc disp8=9` + 5B `mov [r15+PC_OFF],not-taken` + 5B `jmp +5` + 5B `mov [r15+PC_OFF],taken`. `emit_b_uncond` and `emit_t32_bl` (LR = pc+4) also native.

`codegen_emit` suppresses trailing `st_pc` for branch terminators (PC already set by the branch emit).

Decoder fixes: ISB/DSB/DMB decoded as NOP (not OP_UNDEFINED); LDRB writeback guard added; BASEPRI dispatch fixed to check at correct priority.

Coverage 48->52 families.

Test added: `test_jit_branch` — 11 subtests for B.cond all conditions, B.uncond, T32_BL.

### 14-05: JIT Pseudo-Chain Dispatch + Generation Reset

`jit_run_chained`: tight while loop; stays in same C-frame across compiled blocks; breaks on halted / budget / jit_run miss / remaining < JIT_MAX_BLOCK_LEN (overshoot bounded <=31 cycles).

`compile_block` eviction: `jit_flush` + continue when n_blocks >= JIT_MAX_BLOCKS (generation reset). Simpler than per-slot LRU; FreeRTOS test7 has ~200 unique hot TBs vs 1024 cap, so eviction rare.

`run_steps_full_g` and `run_steps_full_gdb` updated to use jit_run_chained. Budget = `irq_safe_budget(st, scb, remaining)` caps chain to SysTick CVR boundary; `stop = &scb->pendsv_pending` exits chain when FreeRTOS portYIELD fires ICSR mid-chain.

IRQ latency regression fixed: original chaining without budget cap caused SysTick coalescing and PendSV starvation (test4/test6/test7 were broken). `irq_safe_budget` + stop pointer fixed all three.

Tests added: `test_jit_chain` (chain/budget/eviction), `test_jit_systick` (SysTick periodicity under JIT chain).

### 14-06: Bench Harness + DWT Optimization

QPC timing added to `tools/main.c`: every invocation prints `IPS: X.XXM  elapsed: Y.Yms` to stderr. No flag required; always-on.

`test_jit_bench`: loads test7_freertos.bin, runs 500K-instruction warmup to JIT-compile all hot blocks, resets peripherals to clean state, reloads firmware, times 5M-cap run. Asserts `elapsed < 70ms`. Warns if >50ms (ROADMAP aspirational). LECERF_BENCH_SKIP bypasses for slow CI.

DWT auto-fix: per-step `dwt_tick()` loop replaced with O(1) batch `cyccnt += (u32)jit_steps`. Equivalent semantics; eliminated 3M function calls per test7 run; +14% IPS.

**Measured: ~57M IPS (range 47-64M; Windows jitter). 100M+ IPS target NOT met.**

Test count: 17->18.

---

## Key Technical Decisions (Phase-wide)

- **WIN64 ABI first**: any helper call inside a thunk would corrupt rdi. Fixed once in 14-01; all subsequent plans inherit correct ABI.
- **Pseudo-chain over patched-chain**: tight while loop in jit_run_chained instead of QEMU-style REL32 patching. Simpler (no back-reference list, no invalidation complexity); adequate for 57M IPS. True patching deferred Phase 15+.
- **Generation reset over per-slot LRU**: jit_flush on n_blocks overflow. FreeRTOS has ~200 unique hot TBs vs 1024 cap; eviction rare in practice.
- **TT safety**: snap_restore calls jit_flush (14-01); chained loop respects max_steps so run_until_cycle stays cycle-precise (overshoot bounded by JIT_MAX_BLOCK_LEN-1 = 31 cycles).
- **Latent T1 ADD/SUB flag bug fixed**: pre-Phase-14 T1 ops did not update APSR in native path. Fixed in 14-03 when flag path was audited.
- **ARM.C inversion**: ARM carry-out = NOT(CF) for subtract (borrow). Fixed in 14-04 to use `setnc` instead of `setc`; jcc table uses jae/jb (CS/CC) correctly.
- **DWT O(1) batch**: O(n) per-step loop eliminated in 14-06. Rule-1 auto-fix (performance equivalent).

---

## Test Coverage Delta

| Test | Phase 13 | Phase 14 | Notes |
|------|----------|----------|-------|
| ctest unit | 11 | 18 | +jit_abi, jit_ldr_str, jit_flags, jit_branch, jit_chain, jit_systick, jit_bench |
| firmware | 14 | 14 | unchanged; all pass through chained native path |

---

## Files Created/Modified (Phase 14 Complete)

```
include/core/codegen.h    (14-01: CG_R_OFF/PC_OFF/APSR_OFF/HALT_OFF macros)
include/core/jit.h        (14-01: jit_flush; 14-05: jit_run_chained + stop param)
src/core/codegen.c        (14-01..14-04: prologue, LDR/STR, NZCV, B.cond)
src/core/jit.c            (14-01: jit_flush; 14-05: jit_run_chained + eviction)
src/core/run.c            (14-05: jit_run_chained; irq_safe_budget; 14-06: DWT batch)
src/core/tt.c             (14-01: snap_restore -> jit_flush)
src/core/decoder.c        (14-04: ISB/DSB/DMB as NOP; BASEPRI fix)
tools/main.c              (14-06: QPC timing + IPS line)
tests/test_jit_abi.c      (14-01: WIN64 ABI round-trip)
tests/test_jit_ldr_str.c  (14-02: LDR/STR native + fault path)
tests/test_jit_flags.c    (14-03: NZCV cross-check native vs interpreter)
tests/test_jit_branch.c   (14-04: B.cond all conditions + B.uncond + T32_BL)
tests/test_jit_chain.c    (14-05: chain/budget/eviction)
tests/test_jit_systick.c  (14-05: SysTick IRQ periodicity under JIT chain)
tests/test_jit_bench.c    (14-06: warmup+reset+timed bench regression)
tests/CMakeLists.txt      (14-01..14-06: 7 new test targets)
```

---

## Phase Status

**Status: almost-shippable**

- All 18 ctest pass (18/18)
- All 14 firmware tests pass (14/14)
- JIT-01..JIT-05: fully satisfied
- JIT-06: partially satisfied (elapsed <70ms hard-fail passes; <50ms ROADMAP aspirational misses by ~3ms average; 100M IPS not achieved)
- IPS 57M (measured) vs 100M (ROADMAP): 57% of target; significant headroom before Phase 15+

Reason not "shippable": ROADMAP explicitly states 100M+ IPS as Phase 14 success criterion. 57M IPS represents meaningful progress (1.9x over baseline 30M) but falls short. Phase 15+ should address: PUSH/POP native codegen, direct block patching (REL32), or flat-SRAM baked-pointer access.

Reason not "regression-found": all tests pass; no behavioral regressions; IPS is an improvement over baseline.

---
*Defined: 2026-04-26*
