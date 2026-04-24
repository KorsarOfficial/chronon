#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/cpu.h"
#include "core/bus.h"
#include "core/decoder.h"
#include "periph/uart.h"
#include "periph/systick.h"

extern u64 run_steps_st(cpu_t* c, bus_t* bus, u64 max_steps, systick_t* st);

/* Cortex-M memory layout defaults (ARM ARM B3):
   Flash   0x00000000 - 0x1FFFFFFF
   SRAM    0x20000000 - 0x3FFFFFFF
   Periph  0x40000000 - 0x5FFFFFFF  (MMIO — future)
*/
#define FLASH_BASE 0x00000000u
#define FLASH_SIZE (1u << 20)      /* 1 MB */
#define SRAM_BASE  0x20000000u
#define SRAM_SIZE  (256u << 10)    /* 256 KB */

static u8* read_file(const char* path, u32* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    u8* buf = (u8*)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_size = (u32)n;
    return buf;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <firmware.bin> [max_steps]\n", argv[0]);
        return 1;
    }
    u64 max_steps = argc >= 3 ? strtoull(argv[2], NULL, 0) : 10000000ull;

    u32 sz = 0;
    u8* blob = read_file(argv[1], &sz);
    if (!blob) return 1;

    bus_t bus; bus_init(&bus);
    bus_add_flat(&bus, "flash", FLASH_BASE, FLASH_SIZE, false);
    bus_add_flat(&bus, "sram",  SRAM_BASE,  SRAM_SIZE,  true);
    static uart_t uart0 = {0};
    uart_attach(&bus, &uart0);
    static systick_t systick = {0};
    systick_attach(&bus, &systick);
    bus_load_blob(&bus, FLASH_BASE, blob, sz);
    free(blob);

    cpu_t cpu;
    cpu_reset(&cpu, CORE_M4);
    /* Cortex-M reset: SP = [0x0], PC = [0x4] per ARM ARM B1.5.5 */
    cpu.msp = bus_r32(&bus, 0x0);
    cpu.r[REG_SP] = cpu.msp;
    u32 entry = bus_r32(&bus, 0x4) & ~1u;
    cpu.r[REG_PC] = entry ? entry : FLASH_BASE;

    u64 n = run_steps_st(&cpu, &bus, max_steps, &systick);

    fprintf(stderr, "halted after %llu instructions\n", (unsigned long long)n);
    fprintf(stderr, "R0=%08x R1=%08x R2=%08x R3=%08x\n",
            cpu.r[0], cpu.r[1], cpu.r[2], cpu.r[3]);
    fprintf(stderr, "PC=%08x SP=%08x APSR=%08x\n",
            cpu.r[REG_PC], cpu.r[REG_SP], cpu.apsr);
    return 0;
}
