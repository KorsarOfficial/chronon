---
phase: 14
plan: 07
subsystem: jit
tags: [codegen, push-pop, ldm-stm, branch, fast-path, flat-sram]
dependency_graph:
  requires: [14-06]
  provides: [jit-gap-closure, native-push-pop, native-ldm-stm, b-cond-fast]
  affects: [jit, bench, test-suite]
tech_stack:
  added: [flat-sram-baked-pointer, b-cond-direct-bit-test, near-jmp-e9-rel32]
  patterns: [emit-precompute-size-before-jcc, codegen-bus-threading]
key_files:
  created: [tests/test_jit_push_pop.c]
  modified:
    - include/core/codegen.h
    - src/core/codegen.c
    - src/core/jit.c
    - tests/test_jit_abi.c
    - tests/test_jit_branch.c
    - tests/test_jit_flags.c
    - tests/test_jit_ldr_str.c
    - tests/test_jit_bench.c
    - tests/CMakeLists.txt
    - .planning/phases/14-jit-depth/14-PHASE-SUMMARY.md
decisions:
  - "bus_t* added to codegen_emit signature — enables flat-SRAM baked-pointer at codegen time; propagated to jit.c and all test call sites"
  - "E9 rel32 near jmp when slow_body > 127 bytes — prevents jmp_short truncation for 3+ register PUSH/POP"
  - "jb/ja target computation restructured to pre-compute slow_body and jmp_sz before emitting bounds-check jcc"
  - "emit_apsr_to_eflags + cond_to_jcc deleted — replaced by emit_b_cond_fast direct bit-test; removes ~30 cycle pushfq/popfq round-trip"
  - "bench assertion tightened from <70ms to <50ms; best-of-3 trials for scheduler jitter stability"
metrics:
  duration_minutes: 120
  completed: "2026-04-26"
  tasks_completed: 5
  files_modified: 10
  files_created: 1
---

# Phase 14 Plan 07: JIT Gap Closure Summary

Native PUSH/POP/LDM/STM codegen + B.cond direct APSR bit-test; 173M IPS (was 57M); 100M+ ROADMAP target met.

## What Was Built

### Task 1: codegen_emit signature + emit_push_v + emit_pop

`codegen_emit(codegen_t*, bus_t*, const insn_t*, u8)` — `bus_t*` param threads the flat-SRAM reference through to emit_push_v/emit_pop/emit_ldm_stm.

`emit_push_v`: SP -= cnt*4 first (ARM semantics); `bus_find_flat(b, 0x20000000)` at codegen time; if found: bake `sram->buf` as `mov r10, imm64`; runtime bounds check `cmp eax,fbase; jb slow; cmp eax,hi; ja slow`; fast path: `r11 = buf + (eax - fbase)`; N inline `mov [r11+k_off], ecx` stores; jmp over slow; slow path: WIN64 bus_write helper calls.

`emit_pop`: mirror; SP += cnt*4 after; PC (bit15) special: `cmp ecx, 0xFFFFFFF0; jae fallback; and ecx, ~1; mov [r15+CG_PC_OFF], ecx`.

`codegen_supports`: added OP_PUSH, OP_POP, OP_T32_LDM, OP_T32_STM.

Deleted dead code: `emit_b_cond`, `emit_apsr_to_eflags`, `cond_to_jcc`, `emit_w16`.

### Task 2: emit_ldm_stm (T32 LDM/STM)

`emit_ldm_stm(cg, b, rn, reg_list, is_load, is_db, writeback, insn_pc)`:
- DB: eax = base - cnt*4 first; IA: eax = base
- Same flat/slow dual-path structure as emit_push_v
- LDM PC: EXC_RETURN check (cmp ecx, 0xFFFFFFF0; jae fallback; and+store)
- STM PC: writes compile-time `insn_pc+4` (no runtime read needed)
- Writeback: IA → rn += cnt*4; DB → rn = start address (already computed)

### Task 3: emit_b_cond_fast (B.cond without pushfq/popfq)

APSR layout: byte3 = N(bit7) Z(bit6) C(bit5) V(bit4).

Simple conds (0x0-0x7): `ld_al_apsr_byte3; test al, mask; jnz/jz rel32=13; st_pc(fall,11B); jmp_short(11); st_pc(taken,11B)`.

Composite (0x8-0xD): `ld_al_apsr_byte3; shr al,4` → al=0bNZCV; then:
- HI (0x8): `and al,6; cmp al,2; je/jne`
- GE (0xA): `mov ah,al; shr ah,3; xor al,ah; test al,1; je`
- GT (0xC): same xor + `test al,5; je`
- LS/LT/LE: inverse jcc of their complements

Eliminates ~30-cycle pushfq/popfq round-trip per conditional branch.

### Task 4: tests/test_jit_push_pop.c

6 sub-cases via SRAM fast path:
1. `push_hi_regs` — PUSH{r4-r7,lr}: verifies 5 words at new SP in ascending reg order
2. `push_lo_regs` — PUSH{r0-r3}: 4 words, SP -= 16
3. `pop_hi_regs` — POP{r4-r7}: loads ascending from old SP; SP += 16; registers set correctly
4. `pop_lo_regs` — POP{r0-r3}: same pattern
5. `t32_ldm_ia` — LDM r1!,{r2,r5}: loads 2 regs, writeback r1 += 8
6. `t32_stm_ia` — STM r0!,{r2,r5}: stores 2 regs to SRAM, writeback r0 += 8

### Task 5: bench best-of-3 + tightened <50ms gate

Best-of-3 timed trials: each trial resets CPU+bus+peripherals+firmware; JIT compiled blocks preserved from warmup across all trials; best elapsed time wins.

Hard assertion: `elapsed_s < 0.050` (was `< 0.070`).

Diagnostic: `native_steps / interp_steps / native%` printed per run.

Measured: 173M IPS, 10.7ms, native=100% (interp_steps=0).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] jmp_short offset truncation for large register lists**
- **Found during:** Task 4 test execution (test_jit_push_pop segfault on push_hi_regs)
- **Issue:** emit_push_v computed slow_body = 262B for 5 regs then cast to u8 → 6; jmp_short skipped only 6 bytes, landing inside slow path
- **Root cause 1:** slow body formula `4+3+2+6+7+10+2+4+4+3` missed `mov_r8d_imm` (6B); actual 51B/reg not 45B
- **Root cause 2:** no check for slow_body > 127; jmp_short (EB rel8) cannot encode offsets > 127
- **Fix:** compute slow_body correctly (51B/reg); use `E9 rel32` near jmp when slow_body > 127
- **Files modified:** `src/core/codegen.c`
- **Commit:** 51be12b

**2. [Rule 1 - Bug] jb/ja target offsets ignored jmp size change**
- **Found during:** Task 4 — bench segfault after push_pop test passed
- **Issue:** jb/ja target = fast_body + 2 (assuming jmp_short=2B); when jmp became 5B (near jmp), slow path started 3B later; jb/ja jumped into fast path code instead
- **Fix:** pre-compute slow_body and jmp_sz BEFORE emitting bounds-check jcc; include actual jmp_sz in ja_target/jb_target; apply same fix to emit_pop and emit_ldm_stm
- **Files modified:** `src/core/codegen.c`
- **Commit:** 51be12b

**3. [Rule 1 - Bug] emit_pop fast_body missing SP update (19B)**
- **Found during:** Same bench investigation
- **Issue:** fast_body = 23+cnt*14 excluded SP update (ld_eax+add_imm+st_eax=19B) that precedes the jmp; ja_target too small by 19
- **Fix:** fast_body += 19 in emit_pop; same correction applied to emit_ldm_stm
- **Files modified:** `src/core/codegen.c`
- **Commit:** 51be12b

## Self-Check: PASSED

Files created:
- `tests/test_jit_push_pop.c` — FOUND
- `.planning/phases/14-jit-depth/14-07-SUMMARY.md` — FOUND

Commits:
- f01b41d (Task 1+2+3: codegen_emit + emit_push_v/pop/ldm_stm/b_cond_fast + test site updates)
- 51be12b (Task 4: test_jit_push_pop + bug fixes)
- d91dd1e (Task 5: bench best-of-3 + <50ms gate + ratio diagnostic)

19/19 ctests pass. No regressions. Measured 173M IPS.

---

## Post-Merge Addendum: Slow-Path eax Clobber Fix

Two additional regressions were found in test7_freertos and test9_freertos_ipc that were introduced by this plan's codegen changes.

### Bug A: wrote_pc suppressed trailing st_pc for OP_POP / OP_T32_LDM

**Symptom:** test7 halted at wrong PC (0x8e) after infinite re-execution of a block.

**Root cause:** `codegen_emit` computed `wrote_pc=true` for the last instruction when `last_op == OP_POP || last_op == OP_T32_LDM`, suppressing the trailing `st_pc` epilogue. Since `insn_native_ok` already rejects any POP/LDM with bit15, the thunk never writes CG_PC_OFF. The CPU's PC was never advanced past the block start, causing infinite re-execution.

**Fix:** Removed OP_POP and OP_T32_LDM from the `wrote_pc` condition in `codegen_emit`. The trailing `st_pc` is now always emitted for these block-terminating instructions.

### Bug B: call_rax clobbers eax/rax used as base address in multi-register slow paths

**Symptom:** test9 STMDB {r0-r8,lr} wrote 9 registers to wrong addresses (0x5, 0x8, 0xC, ...) instead of correct SRAM addresses. Native thunk returned false. Emulator halted.

**Root cause:** In `emit_push_v`, `emit_pop`, and `emit_ldm_stm` slow paths, the base address was loaded into eax once before the register loop, then `mov_edx_eax` used it as the address argument for `bus_write`/`bus_read`. After `call_rax`, rax is clobbered with the return value (1=success, 0=failure). The next iteration's `mov_edx_eax` used 1 or 0 as the address — writing to absolute address 0x1+offset, 0x0+offset etc., which either hit flash (not writable → bus fault → thunk returns false) or wrote to wrong SRAM location.

Pattern in bus fault log:
```
write addr=0x00000005 (= 1 + 4)   <- eax=1 after first call_rax, +4 offset
write addr=0x00000008 (= 0 + 8)   <- eax=0 after second call_rax, +8 offset
```

**Fix:** Moved `ld_eax(cg, REG_SP)` (for push/pop) and `ld_eax(cg, rn)` + optional `sub_imm(cnt*4)` (for ldm/stm with is_db) from before the loop to inside each loop iteration, immediately before `mov_edx_eax`. Updated `slow_body` size computations: each register iteration gains 7B (ld_eax) for push/pop, and 7B + optional 5B (sub_imm) for ldm/stm. Both the inline slow path and the `slow_only_*` fallback path were fixed.

**Files changed:** `src/core/codegen.c`

**Regression tests added:** `tests/test_jit_push_pop.c` — three new slow-path sub-cases:
- `push_slow_path`: PUSH{r0-r3} with SRAM at non-standard base (forces bus_write calls)
- `pop_slow_path`: POP{r0-r3} same setup (forces bus_read calls)
- `stmdb_slow_path`: STMDB{r2,r3} with is_db=true slow path

**Verification:** 14/14 firmware tests pass (including test7_freertos R0=0x14 R1=0x14 and test9_freertos_ipc R0=0x37 R1=0x0a). 19/19 ctests pass. Bench: 137M IPS.
