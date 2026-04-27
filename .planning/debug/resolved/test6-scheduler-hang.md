---
status: resolved
trigger: "test6 mini-RTOS hangs forever after plan 14-07 PUSH/POP/LDM/STM/B.cond native codegen"
created: 2026-04-26T00:00:00Z
updated: 2026-04-26T02:00:00Z
---

## Current Focus

hypothesis: RESOLVED - insn_native_ok now refuses to compile POP/LDM with PC bit set; interpreter handles EXC_RETURN via exc_return() correctly
test: firmware/run_all.sh 12/14 (test6 passes; test7/test9 pre-existing failures); ctest 19/19; bench 170M IPS
expecting: committed and archived
next_action: done

## Symptoms

expected: test6 mini-RTOS scheduler runs ~50 alternations, halts at PC=0xaa
actual: infinite "AABBAABB..." output, never halts
errors: none - just infinite loop
reproduction: ./build/cortex-m.exe firmware/test6/test6.bin
started: after commits f01b41d/51be12b/d91dd1e (plan 14-07 native PUSH/POP/LDM/STM/B.cond)

## Eliminated

- hypothesis: B.cond fast path wrong condition codes (EQ/NE/GE/GT etc.)
  evidence: careful bit-level analysis of APSR byte3 → nibble shift confirms all conds correct
  timestamp: 2026-04-26T00:45:00Z

- hypothesis: PUSH store iteration order off-by-one
  evidence: both interpreter and codegen iterate k=0..14 storing at sp+k_off ascending; identical
  timestamp: 2026-04-26T00:45:00Z

## Evidence

- timestamp: 2026-04-26T00:30:00Z
  checked: firmware/test6/startup.s systick_handler
  found: systick_handler uses PUSH {r4, lr} and POP {r4, pc} where lr=0xFFFFFFFD (EXC_RETURN)
  implication: POP {pc} with EXC_RETURN value is the critical code path

- timestamp: 2026-04-26T00:40:00Z
  checked: emit_pop fast path (line 644-658)
  found: jcc_rel8(0x73, 3u) — jae jumps +3 past itself; comment says "jae +N: or bl,1 (3B)" but or_bl_1 is never emitted; jae lands 3 bytes into and_ecx_imm32
  implication: for EXC_RETURN, execution lands at byte `FF` inside and_ecx_imm32 = potential garbage execution / PC store with 0xFFFFFFFD

- timestamp: 2026-04-26T00:42:00Z
  checked: emit_pop slow path (flat-SRAM path, line 700-705) and slow_only_pop (line 739-744)
  found: jcc_rel8(0x73, 9u) — but and_r10d(7B)+store(7B)=14B; jae +9 only skips 9, lands 5B before end of store
  implication: same EXC_RETURN mishandling in slow paths

- timestamp: 2026-04-26T00:45:00Z
  checked: slow_body calculation for has_pc (line 622)
  found: slow_body += 9+2+9+9=29 but actual emitted bytes = 7+2+7+7=23; overcount by 6
  implication: jmp over slow (from fast path end) skips 6 bytes past the slow path → execution enters epilogue at wrong byte

- timestamp: 2026-04-26T00:50:00Z
  checked: emit_ldm_stm has_pc commit (lines 894-899, 962-966)
  found: same jcc_rel8(0x73, 9u) and same 29-byte overcount in slow_body
  implication: LDM with PC in reg_list has same EXC_RETURN mishandling

## Evidence

- timestamp: 2026-04-26T01:30:00Z
  checked: emit_epilogue_check in codegen.c
  found: when bl=1 (set by or_bl_1), epilogue executes `mov byte [r15+CG_HALT_OFF], 1` → sets c->halted=true AND returns false; interpreter fallback loop checks `if (c->halted) break` at i=0 → exits without executing anything; main run loop then exits because !c->halted is false
  implication: or_bl_1 for EXC_RETURN case halts the emulator, not falls back to interpreter; the "TB fail → interpreter fallback" design for EXC_RETURN was wrong

- timestamp: 2026-04-26T01:35:00Z
  checked: jit_run (jit.c:89-97) and run.c loop condition
  found: bk->native returns false → steps=0 → interpreter fallback: for(i=0; c->halted) break → 0 steps → jit_run returns false → jit_run_chained breaks → run loop exits (c->halted=true)
  implication: the halted state from the native epilogue prevents any interpreter fallback; run loop exits at PC=0x98 (the POP block's start PC, never updated by native code)

## Resolution

root_cause: Two-layer failure. (1) jcc offsets wrong (jae+3 instead of +15, jae+9 instead of +16) and or_bl_1 missing in emit_pop/emit_ldm_stm EXC_RETURN check → wrong bytes executed. (2) Even with correct jcc/or_bl_1, the codegen epilogue unconditionally sets c->halted=1 on bl=1 — there is no "soft fallback" path via or_bl_1. For EXC_RETURN, setting halted halts the emulator. The design note "or_bl_1 → interpreter fallback" was incorrect.
fix: insn_native_ok returns false for OP_POP and OP_T32_LDM when bit15 of reg_list is set → codegen_emit returns NULL → block not compiled natively → interpreter always handles POP/LDM with PC → exc_return() called correctly. Also: jcc offset fixes and or_bl_1 emission remain (correct code even if currently dead for has_pc path).
verification: test6 PC=0xaa, halted after 11471 instructions. firmware/run_all.sh: 12/14 (test7/test9 pre-existing). ctest 19/19. bench 170M IPS native=100%. Regression tests pop_exc_return_no_native + t32_ldm_with_pc_no_native added to test_jit_push_pop.c.
files_changed: [src/core/codegen.c, tests/test_jit_push_pop.c]
