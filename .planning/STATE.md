# State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-26)

**Core value:** f(state, time, events) -> state' deterministic, fast, snapshotable, reversible
**Current focus:** v2.0 — time-travel + product platform

## Current Position

Phase: 16 (SHIPPED 2026-04-29)
Plan: 05 (complete — GitHub Action manifest + release.yml + lecerf-ci-example sample repo + phase close-out)
Status: Phase 16 complete (5/5 plans, CI-01..CI-06 met). 20/20 ctest; 14/14 firmware; 15/15 pytest 1.87s in 20.06 MB container; sample repo 3/3 pytest 0.37s
Last activity: 2026-04-29 — 16-05 executed; action.yml + release.yml + examples/lecerf-ci-example/ shipped; ROADMAP + PHASE-SUMMARY written
Next phase: Phase 17 (release pipeline + lecerf.dev landing) recommended; alternative Phase 15 (WASM + Web IDE)

## Performance Metrics

v1.0 shipped: 12 phases, 62 tests, ~30M IPS hybrid native JIT.
p13.01: 3 tasks, 4 files created, 12 modified, 5->6 tests, 65 min.
p13.02: 2 tasks, 1 file created, 3 modified, 6->8 tests, 30 min.
p13.03: 2 tasks, 1 file created, 6 modified, 8->8 tests (snapshot tests already counted), 45 min.
p13.04: 2 tasks, 1 file created, 3 modified, 8->9 tests, 35 min.
p13.05: 2 tasks, 5 files created, 2 modified, 9->10 tests, 45 min.
p13.06: 2 tasks, 1 file created, 5 modified, 10->11 tests, 25 min.
p14.01: 3 tasks, 1 file created, 6 modified, 11->12 tests, 35 min.
p14.02: 2 tasks, 1 file created, 2 modified, 12->13 tests, 45 min.
p14.03: 2 tasks, 1 file created, 2 modified, 13->14 tests, 12 min.
p14.04: 2 tasks, 1 file created, 2 modified, 14->15 tests, 75 min.
p14.05: 3 tasks, 1 file created, 4 modified, 15->16 tests, 45 min.
p14.06: 3 tasks, 2 files created, 3 modified, 17->18 tests, 90 min.
p14.07: 5 tasks, 1 file created, 9 modified, 18->19 tests, 120 min.
p16.01: 4 tasks, 3 files created, 2 modified, 19->20 tests, 35 min.
p16.02: 3 tasks, 10 files created, 2 modified, 20->26 tests (6 pytest added), 8 min.
p16.03: 2 tasks, 5 files created, 2 modified, 26->35 tests (9 pytest added; 15/15 0.72s), 25 min.
p16.04: 4 tasks, 4 files created (Dockerfile, .dockerignore, runner.py, test_docker.sh), 3 modified, 20.06 MB image, 5/5 docker gates, 60 min.
p16.05: 2 tasks, 12 files created (action.yml, release.yml, sample repo, PHASE-SUMMARY, 16-05 SUMMARY), 2 modified (ROADMAP, STATE), 38 tests cumulative (20 ctest + 14 fw + 15 python sample tests overlap), 35 min.

## Accumulated Context

### Decisions

- p1..p12 all decisions logged in MILESTONES.md
- p12 native JIT pattern: rdi=cpu, [rdi+R_OFF+i*4] = r[i]; supports MOV/ADD/SUB/AND/OR/EOR + imm; fallback to interp on miss
- determinism kernel needed before snapshot (must remove implicit time/rand)
- WASM port deferred to p15 — needs WSA-free network stack first
- p13.01: tt_record_irq/uart_rx use strong no-ops (MinGW static-lib weak symbol gap); 13-04 replaces bodies
- p13.01: run_steps_full_g(jit_t*) is the TT determinism path; run_steps_full_gdb(gdb_t*) keeps gdb integration
- p13.01: jit_t is ~2MB; must not be stack-allocated (Windows 1MB default stack)
- p13.02: snap_blob_t includes uart_state (rx_q/replay_mode); eth.bus zeroed on save, refilled on restore
- p13.03: run_until_cycle in run.c (co-located with run_steps_full_g), declared in tt.h; run.h uses struct forward decls
- p13.03: snap_blob_t ~263KB; must be static in tests, same as jit_t ~2MB
- p13.04: tt_record_irq/uart_rx bodies replaced in-place (no weak override); targets in TT-06 test must be >= first snap cycle (stride boundary)
- p13.04: tt_diff uses FILE* -> stdio.h added to tt.h; snap_entry_t defined in tt.h alongside full tt_t struct
- p13.04: rewind mean latency 0.3ms at 1M history; bsearch 7 cmps + 0.14ms memcpy + worst-case 10K cycles forward replay
- p13.05: tt_on_cycle stride-boundary fix: use last_snap_cycle+stride threshold (not %stride==0); modulo fails when startup adds non-zero cycle offset before first instruction batch
- p13.05: firmware/test_tt flash at 0x00000000 (emulator default); volatile sram_pad needed to prevent optimizer from eliminating loops
- p13.05: phase 13 complete; TT-01..TT-08 all satisfied; 10/10 tests pass; shippable
- p13.06: TT-02 ETH gap closure; frames[] side-blob in tt_t (NOT snap_blob_t); ev_t.payload reused as u32 frame_id; eth_inject_rx dual-use (record-time + replay-time); EVENT_ETH_RX no longer a no-op; 11/11 tests pass
- p14.01: WIN64 ABI fix: thunk prologue saves rcx/rdx->r15/r14 (non-volatile); [r15+disp32] REX.B=1 addressing throughout; 4 pushes+sub rsp,32 = 64B stack aligned; jit_flush exported (zeros n_blocks+lookup_n+cg.used+all tables); snap_restore calls jit_flush (TB cache invalidation on rewind); test_jit_abi 15/15 assertions; 12/12 ctest + 14/14 firmware
- p14.02: native LDR/STR: sub rsp,16 scratch slot per call site; bus_read (rcx=bus,rdx=addr,r8d=sz,r9=&out@[rsp+0]) + bus_write (r9d=val) via mov rax,imm64+call rax; bl failure accumulator (xor ebx,ebx at prologue; or bl,1 on fault; emit_epilogue_check dual-path); 22 new opcode families (LDR/STR/LDRB/STRB/LDRH/STRH imm+reg+SP+LIT T1+T32 + LDRD/STRD); LDRD/STRD uses i->rs for second reg; native coverage 17->39 families; 13/13 ctest + 14/14 firmware
- p14.03: NZCV native: lahf clobbers AH (rax bits[15:8]); fix = save eax->r11d before lahf, movzx edx,ah before restoring eax; ARM_C=NOT CF for sub (xor r10d,1); emit_flags_nzcv(is_sub) + emit_flags_nz; CMP/CMN/TST discard result (no st_eax); T1 always-flag; T32 gate on set_flags; T32_ADDW/SUBW never flag; native coverage 39->48; 14/14 ctest + 14/14 firmware
- p14.04: B.cond native: emit_apsr_to_eflags via pushfq+and(~0x08C1)+4xbt+setc/setnc+movzx+or+popfq; ARM.C stored as NOT(C) in CF so jcc table works (CS=jae, CC=jb, HI=ja, LS=jbe); emit_b_cond layout disp32=13 skips 11B+2B to taken label; emit_b_uncond+emit_t32_bl simple PC/LR stores; codegen_emit suppresses trailing st_pc for branch terminators; native coverage 48->52; 15/15 ctest
- p14.05: jit_run_chained: tight while loop (halted|total<max_steps|remaining<JIT_MAX_BLOCK_LEN|jit_run false); overshoot <=31 cycles; compile_block eviction: jit_flush+continue on n_blocks==JIT_MAX_BLOCKS (generation reset); run_steps_full_g+gdb use jit_run_chained; gdb->stepping skips chain; 16/16 ctest; firmware 11/14 (3 pre-existing failures from 14-04, not regressions)
- p14.06: QPC timing in tools/main.c (always-on; IPS+elapsed to stderr); test_jit_bench (warmup 500K, peripheral memset, timed 5M-cap run, ASSERT elapsed<70ms, warns if >50ms); DWT O(1) batch update (cyccnt+=jit_steps); measured 57M IPS (range 47-64M, Windows jitter); ROADMAP 100M+ IPS NOT met; 18/18 ctest; 14/14 firmware; phase 14 status: almost-shippable
- p14.07: codegen_emit gains bus_t* param for flat-SRAM optimisation; emit_push_v/emit_pop/emit_ldm_stm with flat-SRAM baked-pointer fast path (bounds check jb/ja rel8; r10=buf imm64; r11=buf+(eax-fbase)); emit_b_cond_fast: APSR byte3 direct bit-test (no pushfq/popfq); B.cond auto-fix Rule-1: jmp_short replaced with E9 rel32 when slow_body > 127; jb/ja targets pre-computed using actual jmp_sz; measured 173M IPS, 10.7ms, native=100%; bench gate <50ms; 19/19 ctest; ROADMAP 100M+ MET; phase 14 SHIPPABLE
- p16.01: liblecerf SHARED (OUTPUT_NAME=liblecerf PREFIX="" → liblecerf.dll); tools/main.c rewritten onto board_create/flash/run/destroy; --gdb= path kept via direct board struct access; lecerf-smoke two-board isolation demo; test_lecerf_api 5 smoke tests (null-board, create+run, cpu-isolation, uart-drain, tt-isolation); FIRMWARE_DIR=../../firmware for ctest CWD=build/tests/; 20/20 ctest; 14/14 firmware
- p16.02: scikit-build-core rejected (MSVC cl.exe can't compile __builtin_ctz); setuptools + pre-built MinGW DLL in package_data; ctypes uses lecerf_board_* prefix (not lecerf_* from plan sketch); Board/RunResult snapshot all regs at run() time (17 ctypes calls amortized); CI uses msys2 MinGW build → MSVC Python 3.12 ctypes smoke; 6/6 pytest 0.27s; 20/20 ctest unaffected
- p16.03: pytest11 entry point in pyproject.toml; pytest_plugin.py with board(function-scope,--lecerf-board) + board_all(parametrized stm32f103/stm32f407/generic-m4); test_blink (3), test_uart (1), test_registers (5 parametrized); 15/15 pytest 0.72s; --lecerf-board=stm32f407 override verified
- p16.04: two-stage Alpine Dockerfile; cortex_m_core POSITION_INDEPENDENT_CODE=ON to link static lib into liblecerf.so; b->jit=NULL on !_WIN32 (interpreter path on Linux/musl since JIT codegen is WIN64-ABI-locked); LECERF_FW_DIR env var in conftest.py for docker mount override; final image 20.06 MB; 15/15 pytest in 1.87s
- p16.05: Docker container Action chosen over JS Action for reproducibility; ghcr.io owner lowercased at publish time (Docker spec mandates); sample repo in-tree for now (split post-v1); sample workflow references KorsarOfficial/cortex-m-emu/.github/actions/lecerf-runner@main relative path; release.yml: 3 jobs (build-wheel-linux + build-and-push-docker + release); softprops/action-gh-release@v2; sample 3/3 pytest 0.37s local
- Phase 16 SHIPPED 2026-04-29: 5/5 plans, CI-01..CI-06 all met; +2535/-203 LOC; 20.06 MB image; ~17 s end-to-end CI projection (under 30 s gate)

### Pending Todos

- Phase 15: REL32 direct block patching (true zero-dispatch chaining) or further opcode expansion
- WASM-compatible socket layer (postMessage)

### Blockers

none.

## Session Continuity

Last session: 2026-04-29
Stopped at: Completed 16-05-PLAN.md (GitHub Action + release pipeline + sample repo + Phase 16 close-out); Phase 16 SHIPPED
Resume file: none
Next: Phase 17 plan-and-execute (release pipeline + lecerf.dev landing) OR Phase 15 (WASM + Web IDE)
