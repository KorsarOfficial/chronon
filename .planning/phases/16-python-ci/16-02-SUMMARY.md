---
phase: 16-python-ci
plan: "02"
subsystem: python-wrapper
tags: [python, ctypes, liblecerf, pip, pytest, ci, github-actions]
dependency_graph:
  requires: ["16-01"]   # liblecerf.dll + lecerf.h C ABI
  provides: ["python-lecerf-package", "windows-smoke-ci"]
  affects: ["16-03"]    # future: snapshot/rewind Python API
tech_stack:
  added: [setuptools, pytest, ctypes, github-actions-msys2]
  patterns: [ctypes-cdll-from-file-dir, dataclass-snapshot, proxy-objects]
key_files:
  created:
    - python/pyproject.toml
    - python/MANIFEST.in
    - python/lecerf/__init__.py
    - python/lecerf/_core.py
    - python/lecerf/board.py
    - python/lecerf/run_result.py
    - python/tests/__init__.py
    - python/tests/conftest.py
    - python/tests/test_smoke.py
    - .github/workflows/ci.yml
  modified:
    - CMakeLists.txt
    - .gitignore
decisions:
  - "scikit-build-core replaced with setuptools: MSVC cl.exe cannot compile __builtin_ctz code; pre-built MinGW DLL copied to python/lecerf/ for both editable installs and wheel packaging"
  - "CI uses msys2/setup-msys2 MINGW64 to build liblecerf.dll, then MSVC Python 3.12 via setup-python for the ctypes smoke — exactly the cross-toolchain scenario we needed to gate"
  - "lecerf_board_* function prefix used throughout (not lecerf_* as plan sketch showed — actual ABI from lecerf.h)"
  - "RunResult captures full register snapshot at run() time to avoid 17 ctypes round-trips on register access"
metrics:
  duration_seconds: 467
  completed: "2026-04-29"
  tasks_completed: 3
  files_created: 10
  files_modified: 2
---

# Phase 16 Plan 02: Python ctypes wrapper + 6-test smoke suite Summary

Pip-installable `lecerf` package using setuptools + ctypes; pre-built MinGW liblecerf.dll
bundled inside wheel; ergonomic Board/RunResult/Cpu/Uart/Gpio API; 6/6 smoke tests in 0.27s.

## What Was Built

### Task 1: Package skeleton + ctypes loader

`python/pyproject.toml` uses `setuptools>=68` (scikit-build-core rejected — see Deviations).
`python/lecerf/_core.py` loads `liblecerf.dll` via `ctypes.CDLL(os.path.join(os.path.dirname(__file__), name))`;
all `lecerf_board_*` functions get `argtypes` + `restype` before first call; symbol assertion at import time.
`CMakeLists.txt` gates `cortex-m`, `lecerf-smoke`, and `add_subdirectory(tests)` behind
`if(NOT LECERF_BUILD_PYTHON)` so wheel builds skip test executables.
`install(TARGETS liblecerf LIBRARY DESTINATION lecerf RUNTIME DESTINATION lecerf)` added.

### Task 2: API classes

- `RunResult` dataclass: `exit_cause` str, `cycles` int, `r[16]` snapshot, `apsr`
- `Board`: `flash(path|bytes)->self`, `run(timeout_ms)->RunResult`, `__del__` safe
- `_Cpu`: `r[N]` live register proxy, `apsr` property
- `_Gpio`: `gpio['GPIOA'/'GPIOB'/'GPIOC'][0-15].value` -> bool
- `_Uart`: `output()` drains TX buffer -> bytes (64 KB buffer)
- `__init__.py` exports `Board`, `RunResult`, `__version__ = "0.1.0"`

### Task 3: 6 smoke tests + CI

Six pytest tests, 0.27 s total:
1. `test_create_destroy` — lifecycle, no segfault on del
2. `test_unknown_board_raises` — RuntimeError with "Unknown board"
3. `test_fib_test1` — halt + R0=0x37 (fib(10)=55)
4. `test_uart_output` — UART captures `b'div=6 mul=294\n'`
5. `test_gpio_blink` — `gpio['GPIOC'][13].value` is bool
6. `test_two_board_isolation` — A.R0 unchanged after B.run()

`.github/workflows/ci.yml` `windows-smoke` job: MSYS2 MinGW builds liblecerf.dll ->
DLL copied to `python/lecerf/` -> MSVC Python 3.12 installs editable package ->
ctypes import smoke + full pytest suite on every push.

## Metrics

| Metric | Value |
|--------|-------|
| wheel size | 160 677 bytes (160 KB) |
| liblecerf.dll size | 386 456 bytes (377 KB) |
| pip install -e wallclock | ~4 s |
| pytest 6-test wallclock | 0.27 s |
| ctest 20/20 | still passing |
| MinGW DLL in MSVC CPython | confirmed via ctypes.CDLL load |

## Sample output

```
$ python -c "from lecerf import Board; b = Board('stm32f103'); print(b)"
<Board handle=0x1f1ceb91250>

$ python -m pytest python/tests/ -v
============================= test session starts =============================
platform win32 -- Python 3.14.2, pytest-9.0.2
collected 6 items

python\tests\test_smoke.py::test_create_destroy PASSED
python\tests\test_smoke.py::test_unknown_board_raises PASSED
python\tests\test_smoke.py::test_fib_test1 PASSED
python\tests\test_smoke.py::test_uart_output PASSED
python\tests\test_smoke.py::test_gpio_blink PASSED
python\tests\test_smoke.py::test_two_board_isolation PASSED

============================== 6 passed in 0.42s ==============================
```

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 4 - Architecture] scikit-build-core replaced with setuptools**

- **Found during:** Task 1 verification (`pip install -e python/`)
- **Issue:** scikit-build-core 0.12.2 invoked MSVC cl.exe (Visual Studio 18 2026 detected as default generator). The C code uses `__builtin_ctz`, GCC-specific builtins, and `__attribute__` constructs that MSVC rejects with `error C2124: division by zero` and `C4013: __builtin_ctz undefined`. scikit-build-core cannot override the compiler to MinGW in the editable-install path without complex cmake toolchain files.
- **Decision:** Replace scikit-build-core with `setuptools>=68` + `wheel`. The DLL is pre-built by MinGW (already in `build/liblecerf.dll`); setuptools just packages the `.py` files + the DLL as `package_data`. No recompilation on end-user machines — which was the original goal ("zero compile on end-user machines").
- **CI impact:** The `.github/workflows/ci.yml` windows-smoke job explicitly builds liblecerf.dll with MSYS2 MinGW first, then uses MSVC Python for the ctypes smoke. This actually tests the exact cross-toolchain scenario the plan wanted to gate.
- **Files modified:** `python/pyproject.toml`
- **Commits:** b2a37cb

**2. [Rule 1 - Bug] Corrected function prefix in _core.py**

- **Found during:** Task 1 code review against `include/lecerf.h`
- **Issue:** The plan's API sketch used `lecerf_board_create`, `lecerf_flash`, `lecerf_run`, etc. (mixed prefix). The actual `lecerf.h` consistently uses `lecerf_board_*` for all functions: `lecerf_board_flash`, `lecerf_board_run`, `lecerf_board_uart_drain`, `lecerf_board_gpio_get`, `lecerf_board_cpu_reg`.
- **Fix:** Used the correct `lecerf_board_*` prefix throughout `_core.py` and `board.py`.
- **Files modified:** `python/lecerf/_core.py`, `python/lecerf/board.py`

## Self-Check

Files exist:
- `python/pyproject.toml` — YES
- `python/lecerf/__init__.py` — YES
- `python/lecerf/_core.py` — YES
- `python/lecerf/board.py` — YES
- `python/lecerf/run_result.py` — YES
- `python/tests/conftest.py` — YES
- `python/tests/test_smoke.py` — YES
- `.github/workflows/ci.yml` — YES
- `python/lecerf/liblecerf.dll` — YES (gitignored, copied from build/)

Commits:
- b2a37cb — python: setuptools wheel + ctypes loader for liblecerf
- 199bb0e — python: Board / RunResult / Cpu / Uart / Gpio ergonomic API
- a21304b — python: 6 smoke tests + windows-smoke CI gate for MinGW DLL on MSVC Python

## Self-Check: PASSED
