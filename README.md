# cortex-m-emu

ARMv6-M / ARMv7-M CPU emulator in pure C11. Runs ARM Cortex-M0/M0+/M1/M3/M4/M7 firmware without hardware.

## Status

Phase 1: architectural foundation — CPU state, memory bus, Thumb decoder skeleton, test harness.

## Build

```bash
cmake -B build -G "MinGW Makefiles"   # or "Visual Studio 17 2022" or Ninja
cmake --build build
```

Artifacts:
- `build/cortex-m.exe` — emulator CLI
- `build/tests/test_*.exe` — unit tests

## Run

```bash
cortex-m.exe firmware.bin
```

## Architecture

```
+-----------------------------------+
| CLI (tools/main.c)                |
+-----------------------------------+
| Runtime loop (src/core/run.c)     |
+-----+------+------+---------------+
| CPU | MMU  | NVIC | Peripherals   |
+-----+------+------+---------------+
| Bus (region dispatch, MMIO hooks) |
+-----------------------------------+
| Memory backing (RAM / Flash blobs)|
+-----------------------------------+
```

- **CPU** — registers, PSR, mode, exception state
- **Decoder** — Thumb (16-bit) + Thumb-2 (32-bit)
- **Bus** — region-based routing; callbacks for MMIO
- **NVIC** — 256-IRQ priority-based exception controller
- **Peripherals** — UART, SysTick, GPIO, Timers (callback-driven)

## Supported cores

| Core | Arch | Thumb | Thumb-2 | FPU | MPU | Status |
|------|------|-------|---------|-----|-----|--------|
| Cortex-M0/M0+/M1 | ARMv6-M | yes | no | no | no | target |
| Cortex-M3 | ARMv7-M | yes | yes | no | opt | target |
| Cortex-M4 | ARMv7-M | yes | yes | opt | opt | target |
| Cortex-M7 | ARMv7-M | yes | yes | opt | opt | target |
| Cortex-M23/M33 | ARMv8-M | yes | yes | opt | opt | future |

## License

MIT
