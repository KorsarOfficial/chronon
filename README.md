# cortex-m-emu

Full-system ARM Cortex-M emulator in pure C11. Runs Cortex-M0/M0+/M1/M3/M4/M4F/M7
firmware (Thumb-1 + Thumb-2 + VFPv4-SP) without hardware. Boots real
FreeRTOS-Kernel and STM32F103 firmware unmodified, exposes a GDB Remote
Serial Protocol stub for `arm-none-eabi-gdb` debugging.

```
+--------------------------+
| arm-none-eabi-gdb        |
|  (target remote :1234)   |
+-----------+--------------+
            | TCP RSP
+-----------v-----------------------------+
| cortex-m-emu                            |
| +-------------------------------------+ |
| |  CPU{r[16],APSR,IPSR,EPSR}          | |
| |  fetch -> dcache -> decode -> exec  | |
| |  Mode: Thread/Handler, MSP/PSP      | |
| +-------------------------------------+ |
| |  NVIC + SCB + SysTick + DWT + MPU   | |
| |  VFP (16-region MPU, FPv4 SP)       | |
| +-------------------------------------+ |
| |  Bus: flat regions + MMIO callbacks | |
| +-------------------------------------+ |
| |  Periphery: UART, STM32 (RCC/GPIO/  | |
| |  USART), DWT cycle counter          | |
| +-------------------------------------+ |
+-----------------------------------------+
```

## Status

| Test | Result |
|------|--------|
| 5 unit suites (decoder, executor, bus, memory, T32) | **48/48 ✓** |
| 10 firmware integration tests | **10/10 ✓** |

Tested firmware:
1. `fib(10) = 55` (Cortex-M0)
2. Bubble-sort + recursive `factorial(6)` (Cortex-M3 -O2 with IT block, STRD)
3. UART `printf` + UDIV/MUL
4. SysTick hardware IRQ counter (5 ticks)
5. MSR/MRS PSP + manual PendSV pending via SCB.ICSR
6. Mini-RTOS — 2-task round-robin scheduler, R4-R11 context switching
7. **FreeRTOS-Kernel V10.6.2 ARM_CM3 port** — 2 tasks with `vTaskDelay`
8. FPU (Cortex-M4F) — `√(3²+4²)=5`, area, abs (VLDR, VMUL, VADD, VSQRT, VDIV, VSUB, VNEG, VCVT, VMOV-imm)
9. FreeRTOS Queue producer/consumer — `Σ(1..10) = 55`
10. STM32F103 Blue Pill blink — RCC + GPIOC PC13 + USART1 (real-board firmware unmodified)

## Architecture

```
include/
  core/
    types.h    u8/u16/u32/u64, FORCE_INLINE, LIKELY
    cpu.h      CPU state + flags + IT state + FPU
    fpu.h      32 single-prec regs + FPSCR
    bus.h      region-based memory bus, flat + MMIO
    decoder.h  insn_t + opcode enum
    nvic.h     exc_enter / exc_return
    gdb.h      RSP stub
  periph/
    uart.h     simple TX-to-stdout UART
    systick.h  Cortex-M SysTick at 0xE000E010
    scb.h      SCB ICSR/VTOR/AIRCR at 0xE000ED00
    mpu.h      MPU at 0xE000ED90, 8 regions
    dwt.h      DWT CYCCNT at 0xE0001000
    stm32.h    STM32F103 RCC/GPIO/USART
src/
  core/
    cpu.c      flag computation (NZCV), IT advance
    bus.c      region dispatch
    decoder.c  Thumb-1 + full Thumb-2 decoder
    executor.c interpretation of all decoded ops
    nvic.c     8-word stack frame, EXC_RETURN
    fpu.c      reset
    gdb.c      Remote Serial Protocol over TCP
    run.c      fetch-decode-execute loop + dcache
  periph/      MMIO callbacks for each peripheral
tools/main.c   CLI: load .bin, reset vector, run
tests/         5 unit suites
firmware/      10 self-contained ARM firmwares + test runner
```

## Supported ISA

**Thumb-1 (ARMv6-M):** all ~60 instructions

**Thumb-2 (ARMv7-M):**
- Data-proc modified immediate: 16 ops (AND, BIC, ORR, ORN, EOR, ADD, ADC, SBC, SUB, RSB, MOV, MVN, TST, TEQ, CMN, CMP)
- Plain immediate: MOVW, MOVT, ADDW, SUBW, ADR
- Data-proc register with shift: same 16 ops
- Memory: LDR/STR (T3, T4 forms), LDRD, STRD, LDM, STM (IA, DB)
- Branches: BL, B.W (cond + uncond)
- Multiply/divide: MUL, MLA, MLS, UMULL, SMULL, UMLAL, SMLAL, UDIV, SDIV
- Bitfield: BFI, BFC, UBFX, SBFX
- Bit ops: CLZ, RBIT
- Register shifts: LSL.W, LSR.W, ASR.W, ROR.W
- Compare-and-branch: CBZ, CBNZ
- IT block (full state machine)
- TBB, TBH (table branch)
- CPS (interrupt enable/disable)
- MSR/MRS (PSP, MSP, PRIMASK, BASEPRI, FAULTMASK, CONTROL, APSR/IPSR/EPSR)

**VFPv4 single-precision (Cortex-M4F):**
- VLDR, VSTR
- VADD, VSUB, VMUL, VDIV
- VSQRT, VNEG, VABS
- VMOV (reg, imm with VFPExpandImm32, R↔F)
- VCMP, VCVT (F↔I)
- VMRS, VMSR (FPSCR)

**System:**
- NVIC: SysTick + PendSV with SCB.ICSR.PENDSVSET
- 8-word exception stack frame (R0-R3, R12, LR, PC, xPSR)
- EXC_RETURN: 0xFFFFFFF9 (thread+MSP), 0xFFFFFFFD (thread+PSP), 0xFFFFFFF1 (handler)
- Thread/Handler modes, MSP/PSP, CONTROL.SPSEL switching
- MPU: 8 regions, AP/SIZE/SRD, PRIVDEFENA fallback
- DWT cycle counter

## Build

Requires CMake 3.15+, MinGW gcc 14+ (or MSVC), arm-none-eabi-gcc 13+.

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

Outputs:
- `build/cortex-m.exe` — emulator CLI
- `build/tests/test_*.exe` — unit tests

## Run

```bash
# basic
cortex-m firmware.bin

# with max-instructions limit
cortex-m firmware.bin 1000000

# with GDB stub
cortex-m firmware.bin --gdb=1234
```

In another terminal:

```bash
arm-none-eabi-gdb firmware.elf
(gdb) target remote :1234
(gdb) break main
(gdb) continue
(gdb) info registers
(gdb) step
```

## Memory map

| Range                       | What |
|-----------------------------|------|
| `0x00000000`–`0x00100000`   | Flash (1 MB, RX) |
| `0x20000000`–`0x20040000`   | SRAM (256 KB, RW) |
| `0x40004000`–`0x40004FFF`   | Generic UART (writes → stdout) |
| `0x40010800`–`0x40011000`   | STM32 GPIOA/B/C |
| `0x40013800`–`0x40013BFF`   | STM32 USART1 |
| `0x40021000`–`0x40021400`   | STM32 RCC |
| `0xE0001000`–`0xE00010FF`   | DWT |
| `0xE000E010`–`0xE000E020`   | SysTick |
| `0xE000ED00`–`0xE000ED90`   | SCB |
| `0xE000ED90`–`0xE000EDF0`   | MPU |
| `0xE000EDFC`                | DEMCR |

## License

MIT.
