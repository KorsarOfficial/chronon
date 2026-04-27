---
phase: 14
status: gaps_found
must_haves_total: 6
must_haves_verified: 5
date: 2026-04-27
gaps:
  - truth: "FreeRTOS 5M instructions executes in under 50ms (100M+ IPS)"
    status: partial
    reason: "Measured 56.33M IPS / 54.2ms. 1.9x speedup from 30M baseline. test_jit_bench asserts <70ms (passes) not <50ms. ROADMAP criterion unmet."
    artifacts:
      - path: "tests/test_jit_bench.c"
        issue: "Hard assert is elapsed < 0.070 (line 174), not < 0.050. ROADMAP says <50ms / 100M+ IPS."
    missing:
      - "Native PUSH/POP codegen to eliminate interp fallback on context-switch frames (Phase 15)"
      - "True machine-code block-to-block chaining (patch terminator jmp to next TB address)"
---

# Phase 14: JIT Depth Verification Report

**Phase Goal:** Lift native ARM-on-x86-64 JIT from ~30M IPS baseline to 100M+ IPS. Cover hot loops in real firmware (FreeRTOS, DSP) without falling back to interpreter.
**Verified:** 2026-04-27
**Status:** gaps_found -- 5/6 must-haves verified; JIT-06 (100M+ IPS) not met
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| #      | Truth                                                  | Status      | Evidence |
|--------|--------------------------------------------------------|-------------|----------|
| JIT-01 | Loop dispatcher chains TBs without C-frame return      | PARTIAL     | Software loop in jit_run_chained (jit.c:104-121), not machine-code patch |
| JIT-02 | Native LDR/STR for FreeRTOS context switch             | VERIFIED    | codegen_supports covers all T1/T32 variants (codegen.c:542-553); WIN64 helper-call pattern (codegen.c:332-374) |
| JIT-03 | NZCV flags correct via lahf+seto bit-shift             | VERIFIED    | emit_flags_nzcv (codegen.c:206-), emit_flags_nz (codegen.c:241-); 32-case cross-check in test_jit_flags |
| JIT-04 | B.cond emits x86 jcc rel32, falls through cleanly      | VERIFIED    | emit_b_cond (codegen.c:482-503); pushfq/popfq APSR-to-EFLAGS bridge; all 14 ARM cond codes covered |
| JIT-05 | LRU eviction (generation reset) when cache full        | VERIFIED    | compile_block (jit.c:38-41): jit_flush() on n_blocks >= JIT_MAX_BLOCKS; jit_flush zeros all arrays (jit.c:128-141) |
| JIT-06 | FreeRTOS 5M insns in <50ms (100M+ IPS)                | FAILED      | Measured **56.33M IPS / 54.2ms**. Target: 100M IPS / <50ms. Shortfall: ~43% IPS, +4.2ms |

**Score:** 5/6 truths verified (JIT-01 accepted as partial-pass per ROADMAP note; JIT-06 is the blocking gap)

---

## JIT-01 Detail: "Direct Block Chaining"

REQUIREMENTS.md definition: "terminator emits jmp rel32 to next TB" -- meaning the native machine code of TB-N's terminator would be patched at runtime to jump directly into TB-(N+1)'s native buffer, bypassing the C dispatcher entirely.

**What was built:** jit_run_chained (run.c:101,167 -> jit.c:104-121) is a C while-loop that calls jit_run() per block, returning through the C stack between blocks. This is software dispatch, not machine-code chaining. The ROADMAP parenthetical "pseudo-chain accepted per research" acknowledges this. Implementation satisfies the spirit (no interpreter fallback on hot paths) but not the letter of JIT-01 as written in REQUIREMENTS.md.

**Ruling:** Partial. Accepted as-built for Phase 14; true rel32 patching deferred.

---

## JIT-02 Detail: Native LDR/STR

codegen_supports (codegen.c:520-558) returns true for:
- T1: OP_LDR_IMM, OP_STR_IMM, OP_LDRB_IMM, OP_STRB_IMM, OP_LDRH_IMM, OP_STRH_IMM, OP_LDR_SP, OP_STR_SP, OP_LDR_LIT, OP_LDR_REG, OP_STR_REG
- T32: all corresponding T32 variants plus OP_T32_LDRD_IMM, OP_T32_STRD_IMM

WIN64 ABI: mov rcx, r14 (bus), mov edx, eax (addr) before helper call (codegen.c:332-333). Writeback forms not in codegen_supports -- interpreter fallback per design.

FreeRTOS test7 (14/14 firmware pass) exercises context-switch LDR/STR natively.

---

## JIT-03 Detail: NZCV Flags

emit_flags_nzcv (codegen.c:206): saves result to r11d, lahf+seto cl, extracts AH->edx, reconstructs APSR N=bit31, Z=bit30, C=bit29 (SUB inverts), V=bit28. emit_flags_nz (codegen.c:241) is the two-flag variant for logical ops.

Applied to: ADDS/SUBS, CMP/CMN/TST, T32 equivalents. test_jit_flags runs 32 cross-checks against interpreter. All pass (18/18 ctest).

---

## JIT-04 Detail: B.cond Native

emit_b_cond (codegen.c:482): calls emit_apsr_to_eflags (pushfq+pop+mask+bt+setc+or+push+popfq, codegen.c:431-452), then 0F 8X disp32 jcc. AL (cond=0xE) -> unconditional store-PC. NV (cond=0xF) -> fallthrough store-PC. Layout: apsr_to_eflags; jcc rel32(13); fall st_pc(11B); jmp short(11); taken st_pc(11B).

cond_to_jcc (codegen.c:458) maps all 14 ARM condition codes to x86 jcc opcodes. test_jit_branch covers taken/not-taken for representative conditions.

---

## JIT-05 Detail: LRU / Generation Reset

compile_block (jit.c:38-41): when n_blocks >= JIT_MAX_BLOCKS, calls jit_flush(). jit_flush (jit.c:128-141) resets: n_blocks=0, lookup_n=0, jit_steps=0, native_steps=0, interp_steps=0, cg.used=0, and zeros all lookup_idx[]/lookup_pc[]/counters[] arrays. Full generation reset (not LRU per-entry), correct for direct-mapped cache. test_jit_chain "eviction wrap" subtest verifies this.

---

## JIT-06 Detail: 100M+ IPS Gap (Headline)

| Metric          | Target (ROADMAP) | Measured (empirical) | Delta   |
|-----------------|------------------|----------------------|---------|
| IPS             | 100M+            | **56.33M**           | -43.7%  |
| elapsed (5M)    | <50ms            | **54.2ms**           | +4.2ms  |
| Speedup vs base | --               | 1.9x (from 30M)      | --      |

test_jit_bench hard assertion: elapsed < 0.070 (test_jit_bench.c:174) -- **passes**. The <50ms note at lines 175-176 is a fprintf warning, not a failing assert. 18/18 ctest passes without this gap being caught by CI.

Shortfall root causes:
1. No machine-code TB chaining -- each TB invocation traverses C stack (jit_run_chained while-loop). Estimated 10-20% overhead.
2. PUSH/POP not native -- FreeRTOS context-switch frames hit interpreter for OP_PUSH/OP_POP/OP_T32_LDM (not in codegen_supports). Most frequent Cortex-M instructions.
3. APSR-to-EFLAGS bridge cost -- every B.cond pays pushfq+popfq roundtrip (~16 bytes of slow path per branch).

---

## Cross-Cutting Checks

**TT-safety:** tt.c:137 -- jit_flush(g_jit_for_tt) called from snap_restore. test_tt_rewind, test_tt_firmware, test_tt_replay all pass (18/18 ctest). No regression.

**Decoder barrier fix:** decoder.c:406-407 -- ISB/DSB/DMB (0xF3BF prefix) decoded as NOP, no longer mis-decoded as B.cond.

**BASEPRI fix:** run.c:133 -- check_irqs_gdb only accepts interrupts when c->basepri == 0. FreeRTOS critical sections (basepri=16) correctly masked.

**SysTick/PendSV budget:** irq_safe_budget (run.c:62-76) caps chain to st->cvr and to JIT_MAX_BLOCK_LEN when pendsv_pending. Prevents context-switch starvation.

**Opcode coverage:** 17 native families before Phase 14 -> 52 now.

---

## Anti-Patterns Found

| File                      | Line | Pattern                           | Severity | Impact |
|---------------------------|------|-----------------------------------|----------|--------|
| tests/test_jit_bench.c    | 174  | Hard assert <70ms, not <50ms     | Warning  | CI passes but ROADMAP criterion not gated |
| src/core/jit.c            | 104  | Software chain loop, not jmp-patch | Info    | Contributes to JIT-06 IPS gap |

No TODO/FIXME/placeholder anti-patterns in production source files.

---

## Gaps Summary

One gap blocks full goal achievement:

**JIT-06 (partial):** Speedup achieved (30M -> 56M IPS, 1.9x) but the absolute ROADMAP criterion of 100M+ IPS / <50ms is not met. Measured 54.2ms is 8% over the 50ms wall. The test suite does not enforce the ROADMAP criterion -- test_jit_bench uses a 70ms gate, so this gap is invisible to CI.

Recommended closure path (Phase 14.1 or Phase 15):
1. Native OP_PUSH / OP_POP / OP_T32_STM / OP_T32_LDM codegen -- eliminates interp fallback on FreeRTOS function prologues/epilogues (highest expected gain)
2. True machine-code TB chaining: at end of compile, patch terminator jmp rel32 with resolved address of next compiled TB if already in cache
3. Replace pushfq/popfq in B.cond with direct APSR-bit-to-EFLAGS-bit moves to reduce B.cond overhead

---

_Verified: 2026-04-27_
_Verifier: Claude (gsd-verifier)_
