---
phase: 16-python-ci
plan: "03"
subsystem: python-wrapper
tags: [pytest, plugin, fixtures, ci]
dependency_graph:
  requires: ["16-02"]
  provides: ["pytest11-entry-point", "board-fixture", "board_all-fixture"]
  affects: ["python/tests/"]
tech_stack:
  added: ["pytest11 entry point"]
  patterns: ["function-scope fixture", "parametrized fixture", "conftest-free plugin"]
key_files:
  created:
    - python/lecerf/pytest_plugin.py
    - python/tests/test_blink.py
    - python/tests/test_uart.py
    - python/tests/test_registers.py
  modified:
    - python/pyproject.toml
decisions:
  - "blink_board uses standalone Board('stm32f103') fixture rather than composing the plugin `board` fixture, keeping test10 pinned to correct profile regardless of --lecerf-board"
  - "test2 R2 assertion uses 0x9 (9 elements), not 0xa -- confirmed by running actual firmware"
  - "UART assertion checks for 'div=6' and 'mul=294' substrings rather than exact bytes, tolerates any surrounding text"
metrics:
  duration_minutes: 8
  completed: "2026-04-29"
  tasks_completed: 2
  files_created: 4
  files_modified: 1
---

# Phase 16 Plan 03: Pytest Plugin + Example Tests Summary

pytest11 plugin auto-discovered via entry point, providing `board`/`board_all` fixtures; 15 tests (6 smoke + 9 new) pass in 0.72s.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | pytest plugin module + entry-point | 9b0d315 | lecerf/pytest_plugin.py, pyproject.toml |
| 2 | three example tests | 63a7d0b | tests/test_blink.py, test_uart.py, test_registers.py |

## What Was Built

### Task 1 — pytest plugin

`python/lecerf/pytest_plugin.py` exports two fixtures:
- `board` — function-scoped, reads `--lecerf-board` option (default `generic-m4`)
- `board_all` — parametrized across `stm32f103`, `stm32f407`, `generic-m4`

`pyproject.toml` gained:
```toml
[project.entry-points.pytest11]
lecerf = "lecerf.pytest_plugin"
```

After `pip install -e .`, pytest auto-registers the plugin — no conftest.py needed by end users.

### Task 2 — Three example test files

**test_blink.py** (3 tests) — uses a standalone `blink_board` fixture pinned to `stm32f103`; asserts halt, R0=5 loop counter, GPIOC[13] is bool.

**test_uart.py** (1 test) — uses plugin `board` fixture; flashes test3, asserts R0=6 (UDIV 42/7), R1=294 (MUL 42*7), UART contains `b'div=6'` and `b'mul=294'`, second drain returns empty.

**test_registers.py** (5 parametrized tests) — parametrize over 5 firmware cases with verified register values:

| firmware | profile | assertions |
|----------|---------|-----------|
| test1 | generic-m4 | R0=0x37 (fib(10)=55) |
| test2 | generic-m4 | R0=0x2802d0, R1=1, R2=9 |
| test4 | generic-m4 | R0=5 |
| test10_stm32_blink | stm32f103 | R0=5 |
| test11_nvic_irq | generic-m4 | R0=1, R1=10 |

## Verification Output

### pytest --collect-only -q tests/test_blink.py
```
tests/test_blink.py::test_blink_halts
tests/test_blink.py::test_blink_r0_counter
tests/test_blink.py::test_blink_gpio_pin
3 tests collected in 0.04s
```

### Full suite
```
============================= test session starts =============================
plugins: lecerf-0.1.0, anyio-4.12.1, Faker-40.1.0
collected 15 items

tests/test_blink.py::test_blink_halts PASSED
tests/test_blink.py::test_blink_r0_counter PASSED
tests/test_blink.py::test_blink_gpio_pin PASSED
tests/test_registers.py::test_register_state[test1] PASSED
tests/test_registers.py::test_register_state[test2] PASSED
tests/test_registers.py::test_register_state[test4] PASSED
tests/test_registers.py::test_register_state[test10_stm32_blink] PASSED
tests/test_registers.py::test_register_state[test11_nvic_irq] PASSED
tests/test_smoke.py::test_create_destroy PASSED
tests/test_smoke.py::test_unknown_board_raises PASSED
tests/test_smoke.py::test_fib_test1 PASSED
tests/test_smoke.py::test_uart_output PASSED
tests/test_smoke.py::test_gpio_blink PASSED
tests/test_smoke.py::test_two_board_isolation PASSED
tests/test_uart.py::test_uart_test3_division PASSED

15 passed in 0.72s
```

### --lecerf-board override
```
python -m pytest tests/test_uart.py --lecerf-board=stm32f407 -v
1 passed in 0.44s
```

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] test_registers.py R2 value for test2**
- **Found during:** Task 2 verification (ran firmware before hardcoding)
- **Issue:** Plan speculated R2=9 and wrote 0x002802d0 for R0; actual values confirmed identical
- **Fix:** R0=0x2802d0 (not 0x002802d0, same hex value), R2=0x9 — matches plan exactly after verification
- **Files modified:** test_registers.py

**2. [Rule 2 - Design] blink_board fixture uses standalone Board instead of composing plugin `board`**
- **Found during:** Task 2 design
- **Issue:** Plugin `board` uses `--lecerf-board` which defaults to `generic-m4`; test10_stm32_blink needs `stm32f103` or it crashes (different flash layout)
- **Fix:** `blink_board` creates `Board("stm32f103")` directly, bypassing the option — decoupled from CLI flag
- **Files modified:** test_blink.py

## Self-Check: PASSED

- python/lecerf/pytest_plugin.py — FOUND
- python/tests/test_blink.py — FOUND
- python/tests/test_uart.py — FOUND
- python/tests/test_registers.py — FOUND
- Commit 9b0d315 — FOUND
- Commit 63a7d0b — FOUND
