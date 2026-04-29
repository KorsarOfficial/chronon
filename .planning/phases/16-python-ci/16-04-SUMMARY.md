# 16-04 Summary: Alpine Docker <50MB

## Outcome

Two-stage Alpine Docker image built and verified.

- **Final image size: 20.06 MB** (target was <50 MB; well under budget)
- All 5 verification gates pass (build, size, --version, --help, pytest)
- 15/15 pytest tests run inside the container in 1.87s

## Files created

- `docker/Dockerfile` — two-stage: alpine builder (gcc + cmake) → python:3.12-alpine runtime
- `docker/.dockerignore` — excludes .planning, .git, build, __pycache__, *.bin
- `docker/runner.py` — entry script: --version / --help / `<fw_dir> <tests_dir>` invokes pytest
- `docker/test_docker.sh` — 5-gate local verification script

## Files modified

- `CMakeLists.txt` — added `POSITION_INDEPENDENT_CODE ON` on `cortex_m_core` (required to link static lib into the .so)
- `src/core/board.c` — JIT allocation gated behind `#if defined(_WIN32)`; on non-Windows the interpreter handles execution (the JIT codegen is locked to WIN64 ABI; System V ABI port is future work)
- `python/tests/conftest.py` — `FIRMWARE_DIR` honors `LECERF_FW_DIR` env var so the docker runner can override the firmware path via mount

## Critical fixes during execution

1. **Linker error: `relocation R_X86_64_PC32 against symbol stdout can not be used when making a shared object; recompile with -fPIC`**
   Static `cortex_m_core` was compiled without -fPIC; can't link into shared `liblecerf.so` on Linux.
   Fix: `set_property(TARGET cortex_m_core PROPERTY POSITION_INDEPENDENT_CODE ON)` in CMakeLists.txt.

2. **Runtime: segfault inside `board.run()` on Linux**
   The JIT generates x86-64 code for the WIN64 ABI (rcx/rdx for first two args, r15=cpu, r14=bus). On Linux/musl the System V ABI uses rdi/rsi, so the entry trampoline mismatched.
   Fix: `b->jit = NULL` on non-Windows; `run_steps_full_gc` already falls through to the interpreter when `ctx->jit == NULL`. All 15 pytest tests pass via the interpreter path.

3. **Pytest cache write to read-only mount**
   Mounting `python/tests` read-only blocked pytest's `.pytest_cache` writes (warnings, not failures).
   Fix: `test_docker.sh` mounts tests dir read-write (the host file is git-tracked, so accidental writes show up in `git status` immediately).

## Image layer breakdown

```
$ docker history lecerf:dev
20.06 MB total
  ~14 MB python:3.12-alpine base
  ~5 MB pytest + dependencies
  ~1 MB lecerf wheel (liblecerf.so 70 KB + .py source)
  ~30 KB runner.py
```

Trimming applied (in same RUN layer as install):
- `rm -rf /usr/local/lib/python3.12/site-packages/pip` (~5 MB)
- `rm -rf /usr/local/lib/python3.12/site-packages/setuptools` (~1 MB)
- `find ... -name __pycache__ -exec rm -rf {} +` (~2 MB)
- `rm -rf /tmp/*.whl /root/.cache /var/cache/apk/*`

## Verification (5 gates)

```
$ bash docker/test_docker.sh
=== Step 1: build image ===                       ok (3.6s after warm cache)
=== Step 2: size gate ===   image size: 20057356 bytes — PASS: under 50 MB
=== Step 3: --version ===   lecerf 0.1.0
=== Step 4: --help ===      ok
=== Step 5: pytest run ===  15 passed in 1.87s
=== ALL DOCKER GATES PASSED ===
```

## Wallclock

- Cold cache build: ~120s (alpine apk + cmake configure + cortex_m_core compile)
- Warm cache build: ~3.6s
- pytest inside container: 1.87s

## Next

Wave 5 (16-05): GitHub Action manifest + lecerf-ci-example sample repo + phase close-out.
