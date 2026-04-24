#include "core/bus.h"
#include "core/cpu.h"
#include "core/decoder.h"
#include "test_harness.h"

extern u64 run_steps(cpu_t* c, bus_t* bus, u64 max_steps);

static void setup(bus_t* b, cpu_t* c) {
    bus_init(b);
    bus_add_flat(b, "flash", 0, 0x1000, false);
    bus_add_flat(b, "sram", 0x20000000, 0x1000, true);
    cpu_reset(c, CORE_M4);
    c->r[REG_PC] = 0;
    c->r[REG_SP] = 0x20001000;
}

/* Program: MOV R0, #7 ; MOV R1, #35 ; ADD R2, R0, R1 ; UDF.
   Expected: R2 == 42. */
TEST(add_program) {
    bus_t b; cpu_t c;
    setup(&b, &c);

    u16 prog[] = {
        0x2007,              /* MOV R0, #7 */
        0x2123,              /* MOV R1, #35 */
        0x1840,              /* ADDS R0, R0, R1 */
        0x4602,              /* MOV R2, R0 — NOT implemented in phase 1, placeholder */
        0xDEFE               /* UDF — halt */
    };
    /* We only have ADD_REG encoding without MOV R2,R0 yet — use different prog. */
    u16 prog2[] = {
        0x2007,   /* MOV R0, #7         — R0=7     */
        0x2123,   /* MOV R1, #35        — R1=35    */
        0x1842,   /* ADDS R2, R0, R1    — R2=42    */
        0xDEFE,   /* UDF                — halt     */
    };
    (void)prog;
    bus_load_blob(&b, 0, (u8*)prog2, sizeof(prog2));

    run_steps(&c, &b, 100);
    ASSERT_EQ_U32(c.r[0], 7);
    ASSERT_EQ_U32(c.r[1], 35);
    ASSERT_EQ_U32(c.r[2], 42);
}

/* Flags: CMP R0, R1 with R0=10, R1=5 => Z=0 N=0 C=1 V=0 */
TEST(cmp_flags) {
    bus_t b; cpu_t c;
    setup(&b, &c);
    u16 prog[] = {
        0x200A,   /* MOV R0, #10 */
        0x2105,   /* MOV R1, #5  */
        0x4288,   /* CMP R0, R1  */
        0xDEFE,   /* UDF         */
    };
    bus_load_blob(&b, 0, (u8*)prog, sizeof(prog));
    run_steps(&c, &b, 100);
    ASSERT_TRUE((c.apsr & APSR_Z) == 0);
    ASSERT_TRUE((c.apsr & APSR_N) == 0);
    ASSERT_TRUE((c.apsr & APSR_C) != 0);
    ASSERT_TRUE((c.apsr & APSR_V) == 0);
}

/* Branch: if R0 == R1 then R2 = 1, else R2 = 2. */
TEST(branch_eq) {
    bus_t b; cpu_t c;
    setup(&b, &c);
    u16 prog[] = {
        0x2005,   /* MOV R0, #5 */
        0x2105,   /* MOV R1, #5 */
        0x4288,   /* CMP R0, R1 */
        0xD001,   /* BEQ +2 (skip next instr) */
        0x2202,   /* MOV R2, #2 */
        0xE000,   /* B +0  */
        0x2201,   /* MOV R2, #1 */
        0xDEFE,   /* UDF */
    };
    bus_load_blob(&b, 0, (u8*)prog, sizeof(prog));
    run_steps(&c, &b, 100);
    ASSERT_EQ_U32(c.r[2], 1);
}

int main(void) {
    RUN(add_program);
    RUN(cmp_flags);
    RUN(branch_eq);
    TEST_REPORT();
}
