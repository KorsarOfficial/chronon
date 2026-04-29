# lecerf-ci-example

Sample firmware test suite for **LECERF** — the Cortex-M emulator built for CI.

This directory is a self-contained, copy-paste demo: three pre-built firmware
binaries, three pytest tests, one GitHub Actions workflow, and a one-page
tutorial.  CI runs the full suite in **under 30 seconds** on a stock
GitHub-hosted runner (cold image pull included).

## What this shows

| Firmware                   | Board       | Asserts                                  |
| -------------------------- | ----------- | ---------------------------------------- |
| `test1.bin`                | generic-m4  | `R0 == 55` after fib(10)                 |
| `test3.bin`                | generic-m4  | `R0 == 6`, `R1 == 294`, UART captured    |
| `test10_stm32_blink.bin`   | stm32f103   | `R0 == 5` after 5 GPIO toggles           |

The three behaviours together exercise the **CPU** (arithmetic + flags),
the **UART peripheral** (memory-mapped sink at `0x40004000`) and **GPIO**
(STM32F103 register file at `0x40010800`).

## 5-minute onboarding

```bash
# 1. Clone this directory into a fresh GitHub repo (it is self-contained):
cp -r examples/lecerf-ci-example/ /tmp/my-firmware-tests
cd /tmp/my-firmware-tests
git init && git add . && git commit -m "initial"
git remote add origin git@github.com:<you>/<repo>.git
git push -u origin main

# 2. The included .github/workflows/firmware-test.yml fires on every push.
#    It calls the LECERF Docker-container Action; on first push the runner
#    pulls the image (~20 MB compressed) and runs `pytest tests/` inside.

# 3. Watch the green check appear under the Actions tab.
```

## Layout

```
firmware/
  test1.bin                    # fibonacci -> R0 = 55
  test3.bin                    # UDIV / MUL -> UART output
  test10_stm32_blink.bin       # GPIO toggle, R0 = 5
tests/
  conftest.py                  # tiny fwpath() helper
  test_fib.py                  # asserts R0 == 55 after fib firmware
  test_uart.py                 # asserts R0=6, R1=294, UART bytes captured
  test_blink.py                # locks board=stm32f103, asserts R0 == 5
.github/workflows/firmware-test.yml   # 1-job CI calling the LECERF action
requirements.txt
```

## Local run (no Docker, fastest dev loop)

```bash
pip install -r requirements.txt
pytest tests/ -v
# 3 passed in <1s
```

## Local run (with Docker — bit-for-bit identical to CI)

```bash
docker run --rm \
    -v "$(pwd)/firmware:/fw:ro" \
    -v "$(pwd)/tests:/tests:ro" \
    ghcr.io/korsarofficial/lecerf:latest /fw /tests
# 3 passed in <2s after image is pulled
```

## Adding your own firmware test

1. Drop `myfw.bin` (Cortex-M `arm-none-eabi-objcopy -O binary`) into `firmware/`.
2. Create `tests/test_myfw.py`:
   ```python
   from conftest import fwpath

   def test_myfw_halts(board):
       r = board.flash(fwpath("myfw")).run(timeout_ms=1000)
       assert r.exit_cause == "halt"
       # Add assertions on r.r[N], board.uart.output(), board.gpio[port][pin], etc.
   ```
3. `git push`. CI runs it.

## How the `board` fixture works

`pip install lecerf` registers a pytest11 entry point named **lecerf**, which
contributes two fixtures:

- `board` — function-scoped `Board(profile)`; profile defaults to `generic-m4`,
  override with `--lecerf-board=stm32f407` on the pytest CLI.
- `board_all` — parametrized across `stm32f103`, `stm32f407`, `generic-m4`;
  each consumer test runs three times.

If you need to lock a specific test to a board (as `test_blink.py` does), just
override `board` locally:

```python
@pytest.fixture
def board():
    from lecerf import Board
    b = Board("stm32f103")
    yield b
    del b
```

## Why does CI finish in under 30 seconds?

| Stage                                 | Time   |
| ------------------------------------- | ------ |
| Cold ghcr.io pull (~20 MB compressed) | ~15 s  |
| Container start + pytest collect      | ~1 s   |
| 3 firmware tests run                  | ~1 s   |
| Total                                 | ~17 s  |

The emulator runs at 100M+ IPS through its native JIT, so the firmware
themselves are not the bottleneck — Docker pull dominates.

## License

MIT.
