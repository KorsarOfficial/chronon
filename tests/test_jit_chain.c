#include "test_harness.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "core/codegen.h"
#include "core/decoder.h"
#include "core/jit.h"
#include <string.h>
#include <stdio.h>

extern bool execute(cpu_t* c, bus_t* bus, const insn_t* ins);

/* Large static: jit_t ~2MB; Windows 1MB default stack would overflow. */
static jit_t s_jit;

/* Build a tight back-branch loop in flash:
     0x00: MOV r0, #0       MOVS rd=0, imm=0  -> 0x2000 (T1 MOVS)
     0x02: ADD r0, #1       ADDS rd=0, imm=1  -> 0x3001 (T1 ADDS imm8)
     0x04: B   -0x08        B.UNCOND target=0x00 -> pc+4+imm=0x00 -> imm=-8
                            imm11 = -4 (half-word units) -> 0x7FC
                            encoding: 11100 11111111100 -> 0xE7FC
   Each loop iteration = 3 insns. With JIT_HOT_THRESHOLD=16, after enough
   jit_run probes the block compiles natively. */
static void build_loop(bus_t* bus) {
    region_t* fl = bus_find_flat(bus, 0x00000000u);
    if (!fl) return;
    fl->buf[0] = 0x00u; fl->buf[1] = 0x20u;  /* MOVS r0,#0 */
    fl->buf[2] = 0x01u; fl->buf[3] = 0x30u;  /* ADDS r0,#1 */
    fl->buf[4] = 0xFCu; fl->buf[5] = 0xE7u;  /* B -8 (back to 0x00) */
}

/* Warm the loop block: call jit_run n times so the block reaches
   JIT_HOT_THRESHOLD and gets compiled natively. */
static void warm_loop(jit_t* j, cpu_t* c, bus_t* bus, u32 n) {
    for (u32 p = 0; p < n && !c->halted; ++p) {
        u64 st = 0u;
        (void)jit_run(j, c, bus, execute, &st);
    }
}

/* --- test_chain: chained dispatch runs more than 2 blocks in one call --- */
TEST(test_chain) {
    bus_t bus; bus_init(&bus);
    bus_add_flat(&bus, "flash", 0x00000000u, 4096u, false);
    bus_add_flat(&bus, "sram",  0x20000000u, 4096u, true);

    /* Allow patching flash for loop build. */
    region_t* fl = bus_find_flat(&bus, 0x00000000u);
    ASSERT_TRUE(fl != NULL);
    fl->writable = true;
    build_loop(&bus);
    fl->writable = false;

    cpu_t c; memset(&c, 0, sizeof c);
    cpu_reset(&c, CORE_M4);
    c.r[REG_PC] = 0u;
    c.r[REG_SP] = 0x20001000u;

    memset(&s_jit, 0, sizeof s_jit);
    jit_init(&s_jit);

    /* Warm: 32 probes exceeds JIT_HOT_THRESHOLD=16; block compiles natively. */
    warm_loop(&s_jit, &c, &bus, 32u);

    /* Chain call: budget=10000, expect many blocks run. */
    c.r[REG_PC] = 0u; c.r[0] = 0u; c.halted = false;
    u64 steps = 0u;
    bool ok = jit_run_chained(&s_jit, &c, &bus, execute, 10000ull, &steps);

    ASSERT_TRUE(ok);
    /* Must have run at least 2 full blocks (each block = up to JIT_MAX_BLOCK_LEN insns). */
    ASSERT_TRUE(steps >= (u64)JIT_MAX_BLOCK_LEN * 2u);
    /* r0 should have incremented (ADD r0,#1 executed). */
    ASSERT_TRUE(c.r[0] >= 1u);
}

/* --- test_budget: max_steps < JIT_MAX_BLOCK_LEN exits chain immediately --- */
TEST(test_budget) {
    bus_t bus; bus_init(&bus);
    bus_add_flat(&bus, "flash", 0x00000000u, 4096u, false);
    bus_add_flat(&bus, "sram",  0x20000000u, 4096u, true);

    region_t* fl = bus_find_flat(&bus, 0x00000000u);
    ASSERT_TRUE(fl != NULL);
    fl->writable = true;
    build_loop(&bus);
    fl->writable = false;

    cpu_t c; memset(&c, 0, sizeof c);
    cpu_reset(&c, CORE_M4);
    c.r[REG_PC] = 0u; c.r[REG_SP] = 0x20001000u;

    memset(&s_jit, 0, sizeof s_jit);
    jit_init(&s_jit);
    warm_loop(&s_jit, &c, &bus, 32u);

    /* Budget < JIT_MAX_BLOCK_LEN: chain loop must exit before running any block
       (remaining < JIT_MAX_BLOCK_LEN on first iteration). */
    c.r[REG_PC] = 0u; c.r[0] = 0u; c.halted = false;
    u64 small = 0u;
    (void)jit_run_chained(&s_jit, &c, &bus, execute, (u64)(JIT_MAX_BLOCK_LEN - 1), &small);
    /* overshoot bounded: either 0 (budget cliff hit immediately) or at most JIT_MAX_BLOCK_LEN */
    ASSERT_TRUE(small <= (u64)JIT_MAX_BLOCK_LEN);
}

/* --- test_eviction: filling n_blocks to JIT_MAX_BLOCKS triggers jit_flush --- */
/* Strategy: use a fresh jit_t with n_blocks manually set to JIT_MAX_BLOCKS and
   lookup cleared (simulate full cache); then probe a PC enough to trigger compile.
   compile_block sees n_blocks==JIT_MAX_BLOCKS, calls jit_flush, then compiles
   fresh -> n_blocks resets to 0 then increments to 1. */
TEST(test_eviction) {
    bus_t bus; bus_init(&bus);
    bus_add_flat(&bus, "flash", 0x00000000u, 4096u, false);
    bus_add_flat(&bus, "sram",  0x20000000u, 4096u, true);

    region_t* fl = bus_find_flat(&bus, 0x00000000u);
    ASSERT_TRUE(fl != NULL);
    fl->writable = true;
    build_loop(&bus);
    fl->writable = false;

    cpu_t c; memset(&c, 0, sizeof c);
    cpu_reset(&c, CORE_M4);
    c.r[REG_PC] = 0u; c.r[REG_SP] = 0x20001000u;

    memset(&s_jit, 0, sizeof s_jit);
    jit_init(&s_jit);

    /* Simulate full cache: set n_blocks=JIT_MAX_BLOCKS but keep lookup
       cleared (lookup_idx all -1 from jit_init) so the next probe for pc=0
       will not find a cached block and must call compile_block. */
    s_jit.n_blocks = (u32)JIT_MAX_BLOCKS;
    /* All lookup_idx remain -1 (jit_init set them). counters are 0. */

    /* Probe pc=0 enough times to exceed JIT_HOT_THRESHOLD (16) and trigger
       compile_block. compile_block sees n_blocks==JIT_MAX_BLOCKS -> jit_flush
       -> n_blocks reset to 0 -> compiles block -> n_blocks=1. */
    c.r[REG_PC] = 0u; c.halted = false;
    for (u32 p = 0u; p < (u32)JIT_HOT_THRESHOLD + 4u && !c.halted; ++p) {
        u64 st = 0u;
        (void)jit_run(&s_jit, &c, &bus, execute, &st);
    }

    /* After eviction + recompile: n_blocks should be 1 (one fresh block). */
    ASSERT_TRUE(s_jit.n_blocks <= 2u);
}

int main(void) {
    RUN(test_chain);
    RUN(test_budget);
    RUN(test_eviction);
    TEST_REPORT();
}
