---
phase: 14-jit-depth
plan: 03
subsystem: jit
tags: [x86-64, codegen, flags, nzcv, lahf, seto, arm-thumb, cmp, cmn, tst]

requires:
  - phase: 14-jit-depth/14-02
    provides: WIN64 ABI thunk prologue/epilogue; r15/r14 non-volatile; bl failure flag; LDR/STR native ops

provides:
  - emit_flags_nzcv(is_sub): native NZCV update via lahf+seto+bit-shifts; ARM_C=NOT CF for SUB
  - emit_flags_nz: NZ-only update for AND/ORR/EOR/TST/MOV; preserves C and V bits
  - native CMP/CMN/TST T1+T32 — flag-only ops; no result store
  - ADD/SUB/AND/ORR/EOR T1 always-flag retrofit (latent bug fix)
  - T32 ADD/SUB/AND/ORR/EOR conditional on insn_t.set_flags
  - test_jit_flags: 32 cross-check cases native vs interpreter APSR

affects: [14-04-jit-depth, B.cond native, any phase reading APSR after JIT ops]

tech-stack:
  added: []
  patterns:
    - "lahf clobbers AH (bits[15:8] of rax = result); save result to r11d before lahf, extract AH to edx before restoring eax"
    - "ARM_C = NOT x86_CF for subtract: xor r10d, 1 inserted before shl r10d, 29"
    - "NZCV bit positions: N=31 Z=30 C=29 V=28; mask 0x0FFFFFFF clears all four"
    - "emit flags BEFORE st_eax (eax still holds result after restore)"
    - "CMP/CMN/TST: omit st_eax entirely — result is discarded, only APSR written"

key-files:
  created: [tests/test_jit_flags.c]
  modified: [src/core/codegen.c, tests/CMakeLists.txt]

key-decisions:
  - "lahf clobber fix: save eax to r11d before lahf; movzx edx,ah before mov eax,r11d so AH is captured while still live"
  - "Use r11d (volatile WIN64) and r10d (volatile WIN64) as scratch; esi (callee-saved, pushed in prologue) as NZCV accumulator"
  - "T1 MOV_IMM always sets NZ (matches interpreter behavior: cpu_set_flags_nz unconditional)"
  - "T32_MOVW never sets flags (16-bit zero-extend, no S bit)"
  - "CMP_REG_T2 (hi-register form): native via same op_sub_ec + emit_flags_nzcv; no PC-relative guard needed for test inputs"

patterns-established:
  - "lahf/seto sequence: mov r11d,eax -> lahf -> seto cl -> movzx edx,ah -> mov eax,r11d -> bit-shift NZCV -> merge apsr"
  - "Flag gates: T1 ops always emit; T32 ops gate on i->set_flags; ADDW/SUBW never"

duration: 12min
completed: 2026-04-27
---

# Phase 14 Plan 03: NZCV Native Flag Setters Summary

**Native APSR write via lahf+seto+shl bit-map: ADD/SUB/AND/ORR/EOR/CMP/CMN/TST now set NZCV in JIT path; latent T1 flag bug fixed; 9 new opcode families; 13->14 ctest**

## Performance

- **Duration:** 12 min
- **Started:** 2026-04-27T11:24:26Z
- **Completed:** 2026-04-27T11:36:29Z
- **Tasks:** 2
- **Files modified:** 3 (codegen.c, test_jit_flags.c, CMakeLists.txt)

## Accomplishments

- emit_flags_nzcv(is_sub) emits lahf+seto+bit-shift sequence that reconstructs APSR.NZCV from x86 SF/ZF/CF/OF; ARM_C = NOT CF for subtract
- emit_flags_nz emits lahf+NZ-only merge preserving C(bit29) and V(bit28) for AND/ORR/EOR/TST
- CMP/CMN/TST/T32_CMP/T32_CMN added to codegen_supports + emit_op; result discarded (no st_eax)
- Latent ADD_REG/SUB_REG/ADD_IMM3/IMM8/SUB_IMM3/IMM8 flag bug fixed (T1 always sets flags outside IT)
- T32 ADD/SUB/AND/ORR/EOR gate on i->set_flags; T32_ADDW/SUBW never set flags (T4 encoding)
- 32-case cross-check test: native thunk apsr == interpreter apsr for N/Z/C/V edge inputs

## Task Commits

1. **Task 1: emit_flags_nzcv / emit_flags_nz + case-arm split + Step B/C/D** - `b65d6d8`
2. **Task 2: test_jit_flags.c 32 cross-check cases** - `a9f0a93`

## Files Created/Modified

- `src/core/codegen.c` - emit_flags_nzcv(is_sub), emit_flags_nz, mov_r11d_eax/mov_eax_r11d, ld/st_r10d_apsr, split ADD/SUB case arms, CMP/CMN/TST cases, codegen_supports update
- `tests/test_jit_flags.c` - 32 run_pair cross-checks covering ADD/SUB/AND/ORR/EOR/CMP/CMN/TST; T32 SET_FLAGS gate; no-flag guard
- `tests/CMakeLists.txt` - test_jit_flags added; ctest 13->14

## Decisions Made

- **lahf clobber fix**: lahf stores EFLAGS into AH (bits[15:8] of rax). The result is in eax, so lahf overwrites the high byte. Fixed by: save eax -> r11d before lahf, then movzx edx,ah immediately after lahf (while AH is still valid flags), then mov eax,r11d to restore. The order of movzx before mov-restore is critical.
- **r11d for result save**: r11d is volatile in WIN64 ABI; no save/restore needed. r10d also volatile. esi is callee-saved and pushed in prologue, safe to use as NZCV accumulator.
- **T1 MOV_IMM always sets NZ**: matches executor.c which calls cpu_set_flags_nz unconditionally for OP_MOV_IMM. Native does test eax,eax + emit_flags_nz.
- **CMP_REG_T2**: the hi-register form; test inputs use r[1]/r[2] which are not PC, so the native op_sub_ec path is correct for the tested cases.
- **T32_MOVW never flags**: 16-bit zero-extend, no S bit per ARM ARM T3 encoding.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] lahf clobbers AH before movzx edx,ah**
- **Found during:** Task 1 (testing), first run revealed r0 values corrupted by lahf overwriting upper byte of eax
- **Issue:** lahf stores EFLAGS into AH (rax bits[15:8]), corrupting the ALU result stored there. The original plan sketch had mov_eax_r11d called right after lahf+seto, which then destroyed AH before movzx could sample it.
- **Fix:** Reordered sequence: mov r11d,eax -> lahf -> seto cl -> movzx edx,ah -> mov eax,r11d. Extract AH to edx BEFORE restoring eax from r11d.
- **Files modified:** src/core/codegen.c (emit_flags_nzcv and emit_flags_nz)
- **Verification:** 32/32 cross-check cases pass; r0 and apsr both match interpreter
- **Committed in:** b65d6d8 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug in lahf/AH ordering)
**Impact on plan:** Fix was necessary for correctness. No scope creep.

## Issues Encountered

- Initial run: r0 values were corrupted (e.g., ADD_REG V: native r0=80009600 instead of 80000000). Root cause: lahf overwrites AH. Fix: extract AH to edx before restoring eax from r11d.
- Second run after first fix attempt: apsr wrong because mov_eax_r11d was placed BETWEEN lahf and movzx_edx_ah. Final correct order: save -> lahf+seto -> movzx(edx,ah) -> restore.

## Next Phase Readiness

- APSR is now accurate after all ALU ops in JIT path
- 14-04 can implement native B.cond: reads APSR set by upstream native CMP/SUB in same TB
- Native opcode coverage: 39 (14-02) + 9 new flag-setter families = 48 total families

---

## Self-Check: PASSED

Files verified:
- FOUND: src/core/codegen.c
- FOUND: tests/test_jit_flags.c
- FOUND: tests/CMakeLists.txt

Commits verified:
- FOUND: b65d6d8 (Task 1)
- FOUND: a9f0a93 (Task 2)

Tests: 14/14 ctest pass; 14/14 firmware pass

*Phase: 14-jit-depth*
*Completed: 2026-04-27*
