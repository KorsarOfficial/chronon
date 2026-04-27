---
status: resolved
trigger: "4 firmware test regressions from plan 14-04 native B.cond/B/BL emit"
created: 2026-04-26T00:00:00Z
updated: 2026-04-27T00:00:00Z
symptoms_prefilled: true
---

## Current Focus

hypothesis: CONFIRMED (two independent root causes found and fixed)
test: firmware/run_all.sh
expecting: 14/14 pass
next_action: DONE — resolved

## Symptoms

expected: firmware tests complete with correct register values
actual: tests run to max-instruction-limit or produce wrong counter values
errors: test10 R0=0x113 (counter never stops at 5), test7/9/13 run forever
reproduction: bash firmware/run_all.sh
started: after commits 6da40c8, 0fa401b, 9eccd33 (plan 14-04)

## Eliminated

- hypothesis: BASEPRI not respected — interrupts fire during critical sections and corrupt state
  evidence: Adding BASEPRI check to check_irqs fixes FreeRTOS critical section semantics but test7/test9 crash persists at the same step (490424→490425)
  timestamp: 2026-04-27

- hypothesis: Corrupted LR from JIT native emission of BL causes wrong BX return
  evidence: Trace at crash shows LR=0x565 is stable and valid; the crash is a straight-line PC=0xEA2→0xC0D44 not a BX to bad address
  timestamp: 2026-04-27

## Evidence

- timestamp: 2026-04-27
  checked: test10_stm32_blink binary (puts_ function at 0x5E-0x68)
  found: LDRB.W r1,[r0,#1]! (T4 form, writeback=true) is inside a JIT-compiled block; emit_load ignores writeback flag so r0 is never post-incremented; infinite loop over same byte
  implication: codegen must reject T32 memory ops with writeback=true

- timestamp: 2026-04-27
  checked: test7_freertos.elf disassembly at vPortEnterCritical (0xE96)
  found: DSB SY at 0xEA2 = bytes F3BF 8F4F; ISB SY at 0xE9E = bytes F3BF 8F6F; both have upper halfword 0xF3BF
  implication: decoder checks w0==0xF3AF for barrier group; 0xF3BF falls through to B.cond T3 decoder

- timestamp: 2026-04-27
  checked: decoder math for 0xF3BF 0x8F4F decoded as B T3
  found: w1=0x8F4F → S=0,J2=0,J1=0,imm11=0x74F=1871; w0=0xF3BF → imm6=0x3F=63, cond=(0xF3BF>>6)&0xF=14(AL); imm21=(0<<20)|(0<<19)|(0<<18)|(63<<12)|(1871<<1)=0xBFEFE=786174... recalc: (63<<12)=0x3F000=258048; (1871<<1)=3742; total=261790; sext21=261790; target=0xEA2+4+261790*2... actual observed: target=0xC0D44 confirmed by trace
  implication: DSB at 0xEA2 → branch to 0xC0D44 (unmapped flash, all-zero) → halts

- timestamp: 2026-04-27
  checked: run.c debug trace output
  found: step=490424 PC=00000ea2 LR=00000565 basepri=16; step=490425 PC=000c0d44 LR=00000565; exact jump matches DSB decoded as branch
  implication: root cause confirmed — decoder bug causes ISB/DSB to be treated as conditional branches

## Resolution

root_cause: TWO root causes:
  1. (test10/test13) src/core/codegen.c: codegen_supports accepted T32_LDR/STR_IMM and variants unconditionally, but emit_load ignores writeback/add/index fields. T4 post-increment forms (writeback=true) emitted wrong code (no r_base update). Fix: added insn_native_ok() guard that rejects T32 memory ops unless add=true, index=true, writeback=false (T3 simple-offset form only).
  2. (test7/test9) src/core/decoder.c: barrier instructions ISB/DSB/DMB encode as 0xF3BF xxxx; the hint/barrier check tested w0==0xF3AF only. 0xF3BF fell through to B.cond T3 decoder, producing a conditional branch (cond=14=AL, huge imm) that jumped to garbage address in flash. Fix: changed check to w0==0xF3AFu || w0==0xF3BFu.
  Bonus fix: src/core/run.c check_irqs: BASEPRI was not checked — SysTick/PendSV fired even during FreeRTOS critical sections (BASEPRI=16). Fixed by adding && c->basepri == 0 to the interrupt dispatch condition.

fix: |
  1. codegen.c: added insn_native_ok() before codegen_emit loop; T32 memory ops with writeback=true fall to interpreter
  2. decoder.c: `if (w0 == 0xF3AFu || w0 == 0xF3BFu)` — both hint and barrier groups decode as OP_T32_NOP
  3. run.c: check_irqs condition extended with `&& c->basepri == 0`
  4. tests/test_decoder.c: added t32_isb_decodes_as_nop, t32_dsb_decodes_as_nop, t32_dmb_decodes_as_nop regression tests
  5. tests/test_jit_ldr_str.c: t32_ldr_imm test explicitly sets add=true, index=true, writeback=false to match T3 form

verification: 14/14 firmware tests PASS, 15/15 ctests PASS

files_changed:
  - src/core/decoder.c
  - src/core/codegen.c
  - src/core/run.c
  - tests/test_decoder.c
  - tests/test_jit_ldr_str.c
