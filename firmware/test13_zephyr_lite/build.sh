#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
GCC="/c/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gcc.exe"
OBJCOPY="/c/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-objcopy.exe"
CFLAGS="-mcpu=cortex-m3 -mthumb -Os -ffreestanding -nostdlib -fno-exceptions -Wall -I."
LDFLAGS="-Wl,--gc-sections -T link.ld -nostartfiles"
"$GCC" $CFLAGS $LDFLAGS startup.s zephyr_lite.c main.c -o test13_zephyr_lite.elf
"$OBJCOPY" -O binary test13_zephyr_lite.elf test13_zephyr_lite.bin
ls -la test13_zephyr_lite.bin
