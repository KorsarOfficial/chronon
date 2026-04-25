# State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-26)

**Core value:** f(state, time, events) -> state' deterministic, fast, snapshotable, reversible
**Current focus:** v2.0 — time-travel + product platform

## Current Position

Phase: 13
Plan: 02 (next)
Status: 13-01 complete; TT-01 determinism kernel done; ready for 13-02 snapshot
Last activity: 2026-04-26 — p13.01 TT-01 determinism kernel: ev_t/ev_log_t/tt_periph_t, jit counters, bus_find_flat, uart replay-mode, nvic ext hook, 2-run byte-eq test

## Performance Metrics

v1.0 shipped: 12 phases, 62 tests, ~30M IPS hybrid native JIT.
p13.01: 3 tasks, 4 files created, 12 modified, 5->6 tests, 65 min.

## Accumulated Context

### Decisions

- p1..p12 all decisions logged in MILESTONES.md
- p12 native JIT pattern: rdi=cpu, [rdi+R_OFF+i*4] = r[i]; supports MOV/ADD/SUB/AND/OR/EOR + imm; fallback to interp on miss
- determinism kernel needed before snapshot (must remove implicit time/rand)
- WASM port deferred to p15 — needs WSA-free network stack first
- p13.01: tt_record_irq/uart_rx use strong no-ops (MinGW static-lib weak symbol gap); 13-04 replaces bodies
- p13.01: run_steps_full_g(jit_t*) is the TT determinism path; run_steps_full_gdb(gdb_t*) keeps gdb integration
- p13.01: jit_t is ~2MB; must not be stack-allocated (Windows 1MB default stack)

### Pending Todos

- direct block chaining (jmp rel32 inter-TB)
- LDR/STR native via helper-call
- flag-setter ops via LEA tricks
- WASM-compatible socket layer (postMessage)

### Blockers

none.

## Session Continuity

Last session: 2026-04-26
Stopped at: Completed 13-01-PLAN.md
Resume file: none
