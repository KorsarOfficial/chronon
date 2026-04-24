#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

GCC="/c/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gcc.exe"
OBJCOPY="/c/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-objcopy.exe"
OBJDUMP="/c/Program Files (x86)/Arm/GNU Toolchain mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-objdump.exe"

CFLAGS="-mcpu=cortex-m3 -mthumb -O2 -ffreestanding -nostdlib -fno-exceptions -fno-stack-protector -Wall"
LDFLAGS="-Wl,--gc-sections -T link.ld -nostartfiles"

"$GCC" $CFLAGS -c startup.s -o startup.o
"$GCC" $CFLAGS -c main.c    -o main.o
"$GCC" $CFLAGS $LDFLAGS startup.o main.o -o test4.elf
"$OBJCOPY" -O binary test4.elf test4.bin

echo "--- disasm ---"
"$OBJDUMP" -d test4.elf | head -50
echo "--- size ---"
ls -la test4.bin
