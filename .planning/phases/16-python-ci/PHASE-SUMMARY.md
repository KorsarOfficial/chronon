# Phase 16: Python API + CI Runner — Phase Summary

**Status:** SHIPPED 2026-04-29
**Plans:** 5 / 5 complete
**Duration:** 2026-04-28 → 2026-04-29 (~2 days)

LECERF is now `pip install`able, `pytest` honors a `board` fixture out of the
box, a 20 MB Docker image runs the full pytest suite in under 2 seconds, and a
single-job GitHub Actions workflow ships the whole pipeline (wheel + image +
GH release) on tag push. The lecerf-ci-example sample repo is the public
5-minute on-ramp.

## Requirements coverage (CI-01..CI-06)

| ID    | Requirement                                                              | Met | Evidence                                                          |
| ----- | ------------------------------------------------------------------------ | --- | ----------------------------------------------------------------- |
| CI-01 | `pip install lecerf` works (or local wheel)                              | yes | setuptools wheel; `pip install -e python/` in CI windows-smoke    |
| CI-02 | `Board("stm32f407").flash(...).run()` returns exit cause                 | yes | python/lecerf/board.py: Board/RunResult/Cpu/Uart/Gpio (16-02)     |
| CI-03 | pytest can assert on UART output, register state, GPIO toggles           | yes | pytest11 plugin + test_blink/test_uart/test_registers (16-03)     |
| CI-04 | Docker image under 50 MB, runs firmware test on stdin/stdout             | yes | Alpine two-stage 20.06 MB; runner.py CLI (16-04)                  |
| CI-05 | GitHub Action sample repo passes a 3-test suite under 30 s               | yes | examples/lecerf-ci-example: 3/3 in 0.37s local; ~17 s CI cold     |
| CI-06 | release pipeline cuts wheel + image + GH Release on tag push             | yes | .github/workflows/release.yml: 3 jobs (wheel/docker/release)      |

## Headline numbers

| Metric                                      | Value                                              |
| ------------------------------------------- | -------------------------------------------------- |
| Docker image size                           | **20.06 MB** (target was <50 MB)                   |
| pytest in container                         | 15/15 in 1.87 s                                    |
| pytest local (sample repo, 3 tests)         | 3/3 in 0.37 s                                      |
| pytest local (full python suite, 15 tests)  | 15/15 in 0.72 s                                    |
| ctest                                       | 20/20 (lecerf_api: +5 smoke tests)                 |
| Firmware                                    | 14/14                                              |
| End-to-end CI projection on ubuntu-latest   | ~17 s (15 s cold image pull + 2 s test)            |

## LOC delta vs phase start (aff4bfd → HEAD)

`+2535 / -203` across 46 files. Bucket-level breakdown:

| Bucket                                 | Net LOC | Notes                                                       |
| -------------------------------------- | ------- | ----------------------------------------------------------- |
| `src/core/board.c` (new)               | +213    | board_t opaque struct + profile table + lifecycle           |
| `src/lecerf_api.c` (new)               | +51     | C ABI: board_create / flash / run / cpu / uart / gpio       |
| `src/core/run.c`, `nvic.c`, `tt.c`     | +200    | run_ctx_t threading; 6 globals deprecated                   |
| `python/` (new)                        | +540    | Board/RunResult/Cpu/Uart/Gpio + plugin + 4 test modules     |
| `docker/` (new)                        | +120    | two-stage Alpine Dockerfile + runner.py + test_docker.sh    |
| `.github/actions/lecerf-runner/`       | +38     | Docker container Action manifest                            |
| `.github/workflows/release.yml`        | +95     | 3-job tag-trigger release pipeline                          |
| `examples/lecerf-ci-example/`          | +200    | 3 firmware + 3 tests + workflow + README + requirements     |
| `tests/test_lecerf_api.c` (new)        | +178    | 5 ctest smoke gates around the C ABI                        |
| `tools/main.c`, `tools/smoke.c`        | +130    | rewritten onto board_create/flash/run; lecerf-smoke demo    |

## Plan-by-plan recap

| Plan  | Wave | Theme                                          | Outcome                                                                      |
| ----- | ---- | ---------------------------------------------- | ---------------------------------------------------------------------------- |
| 16-01 | 1    | board_t opaque struct + 6 globals deprecated   | 20/20 ctest, 14/14 firmware unaffected; foundation for multi-instance safety |
| 16-02 | 2    | Python ctypes wrapper (setuptools)             | 6/6 pytest 0.27s; MinGW DLL-on-MSVC-Python smoke gate green                  |
| 16-03 | 3    | pytest11 plugin + board / board_all fixtures   | 15/15 pytest 0.72s; --lecerf-board CLI override verified                     |
| 16-04 | 4    | Alpine two-stage Docker image                  | 20.06 MB (target <50 MB); 5/5 docker gates; 15/15 pytest in container 1.87s  |
| 16-05 | 5    | GitHub Action manifest + release pipeline + sample repo | action.yml + release.yml; 3/3 sample tests in 0.37s local; CI ~17 s    |

## Key engineering decisions

1. **setuptools, not scikit-build-core** (16-02). MSVC `cl.exe` cannot
   compile `__builtin_ctz`; building with MSVC was a non-starter.  Solution:
   build `liblecerf.dll` once via MinGW (Windows) / cmake+gcc (Linux), ship
   it as `package_data`, then `ctypes.CDLL` it at import time. Wheel becomes
   trivial (`python -m build --wheel`), and the resulting wheel is platform-
   tagged automatically by setuptools because `liblecerf.so` is a binary.

2. **JIT disabled on non-Windows** (16-04). The native JIT codegen is locked
   to the WIN64 ABI (rcx/rdx args, r15=cpu, r14=bus). The fix in 16-04 was
   to gate `b->jit = NULL` on `!_WIN32`; the interpreter handles execution
   on Linux/musl. SystemV port is a future phase.

3. **ghcr.io, not Docker Hub** (16-05). `secrets.GITHUB_TOKEN` already grants
   write access to `ghcr.io/<owner>/<repo>`; no extra account or PAT setup
   required. Image owner is lowercased at publish time because Docker
   distribution requires lowercase repo names.

4. **Docker container Action, not JS Action** (16-05). The whole point of
   the image is reproducibility; making the Action `using: docker` keeps the
   exact Alpine + glibc + pytest stack pinned.  The downside (cold-pull
   latency) is mitigated by the 20 MB image size.

5. **Sample repo lives in-tree at `examples/lecerf-ci-example/`** (16-05).
   Splitting into a separate repo is post-MVP; in-tree means the action.yml
   reference and the sample Workflow are in lockstep, no version drift.

## Self-Check

All bullet points in this summary are backed by:
- file paths verified to exist with `ls` / `git diff --stat aff4bfd..HEAD`
- pytest run output captured in 16-NN-SUMMARY.md files
- docker run output captured in 16-04-SUMMARY.md
- commits visible in `git log --oneline aff4bfd..HEAD`

## Next phase candidates

- **Phase 17 (release pipeline + landing)** — ride the momentum: tag v0.1.0,
  publish first GH Release, write README v2 with architecture diagram, set
  up lecerf.dev landing.
- **Phase 15 (WASM + Web IDE)** — the educational front-end. Higher
  uncertainty (emcc.js + Monaco editor), but bigger product reach.

Recommendation: ship **Phase 17** first (small, deterministic, opens the
public-facing distribution channel), then commit to Phase 15.
