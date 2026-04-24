#include "core/bus.h"
#include "core/cpu.h"
#include "core/decoder.h"
#include "test_harness.h"

extern u64 run_steps(cpu_t* c, bus_t* bus, u64 max_steps);
extern u32 thumb_expand_imm_pub(u32 i12);

static void setup(bus_t* b, cpu_t* c) {
    bus_init(b);
    bus_add_flat(b, "flash", 0, 0x2000, true);
    bus_add_flat(b, "sram", 0x20000000, 0x2000, true);
    cpu_reset(c, CORE_M4);
    c->r[REG_PC] = 0;
    c->r[REG_SP] = 0x20001000;
}

/* Each Thumb-2 instruction is 4 bytes; w0 is at lower addr, w1 higher.
   In little-endian memory, halfwords are stored as themselves. */
static void emit_t32(u8* p, u16 w0, u16 w1) {
    p[0] = w0 & 0xFF; p[1] = (w0 >> 8) & 0xFF;
    p[2] = w1 & 0xFF; p[3] = (w1 >> 8) & 0xFF;
}

TEST(expand_imm) {
    /* i12 = 0x000FF -> 0x000000FF */
    ASSERT_EQ_U32(thumb_expand_imm_pub(0x0FF), 0xFF);
    /* i12 = 0x100FF -> 0x00FF00FF */
    ASSERT_EQ_U32(thumb_expand_imm_pub(0x1FF), 0x00FF00FF);
    /* i12 = 0x200FF -> 0xFF00FF00 */
    ASSERT_EQ_U32(thumb_expand_imm_pub(0x2FF), 0xFF00FF00);
    /* i12 = 0x300FF -> 0xFFFFFFFF */
    ASSERT_EQ_U32(thumb_expand_imm_pub(0x3FF), 0xFFFFFFFF);
}

/* MOVW/MOVT: build 32-bit constant 0xDEADBEEF in R0. */
TEST(movw_movt) {
    bus_t b; cpu_t c; setup(&b, &c);
    u8 prog[16] = {0};
    /* MOVW R0, #0xBEEF
       Encoding: f240 60ef => imm4=0, i=1, imm3=6, imm8=0xEF, Rd=0
       Verify by hand-encoding. f240 = 1111 0010 0100 0000 (imm4=0, S already implicit)
       Use precomputed bytes from objdump for correctness:
         f240 60ef  -> bytes: 40 f2 ef 60
         f2c0 d0ad  -> bytes: c0 f2 ad d0  (MOVT R0,#0xDEAD)
    */
    /* MOVW R0,#0xBEEF -> f64b 60ef -> bytes: 4b f6 ef 60 */
    prog[0] = 0x4B; prog[1] = 0xF6; prog[2] = 0xEF; prog[3] = 0x60;
    /* MOVT R0,#0xDEAD -> f6cd 60ad -> bytes: cd f6 ad 60 */
    prog[4] = 0xCD; prog[5] = 0xF6; prog[6] = 0xAD; prog[7] = 0x60;
    prog[8] = 0xFE; prog[9] = 0xDE; /* UDF */
    bus_load_blob(&b, 0, prog, sizeof(prog));
    run_steps(&c, &b, 100);
    ASSERT_EQ_U32(c.r[0], 0xDEADBEEF);
}

/* IT block: CMP R0, R1 ; ITE EQ ; MOVEQ R2,#1 ; MOVNE R2,#2.
   With R0==R1, expect R2 = 1. */
TEST(it_block_eq) {
    bus_t b; cpu_t c; setup(&b, &c);
    u8 prog[16] = {0};
    /* MOV R0, #5 : 0x2005
       MOV R1, #5 : 0x2105
       CMP R0, R1 : 0x4288
       ITE EQ     : 0xBF0C  (cond=0 (EQ), mask=1100 -> ITE EQ)
       MOV R2, #1 : 0x2201   (executed if EQ)
       MOV R2, #2 : 0x2202   (executed if NE)
       UDF        : 0xDEFE
    */
    u16 ws[] = { 0x2005, 0x2105, 0x4288, 0xBF0C, 0x2201, 0x2202, 0xDEFE };
    for (int i = 0; i < 7; ++i) {
        prog[i*2]   = ws[i] & 0xFF;
        prog[i*2+1] = (ws[i] >> 8) & 0xFF;
    }
    bus_load_blob(&b, 0, prog, sizeof(prog));
    run_steps(&c, &b, 100);
    ASSERT_EQ_U32(c.r[2], 1);
}

/* Same IT block, but R0 != R1. Expect R2 = 2. */
TEST(it_block_ne) {
    bus_t b; cpu_t c; setup(&b, &c);
    u8 prog[16] = {0};
    u16 ws[] = { 0x2005, 0x2107, 0x4288, 0xBF0C, 0x2201, 0x2202, 0xDEFE };
    for (int i = 0; i < 7; ++i) {
        prog[i*2]   = ws[i] & 0xFF;
        prog[i*2+1] = (ws[i] >> 8) & 0xFF;
    }
    bus_load_blob(&b, 0, prog, sizeof(prog));
    run_steps(&c, &b, 100);
    ASSERT_EQ_U32(c.r[2], 2);
}

int main(void) {
    RUN(expand_imm);
    RUN(movw_movt);
    RUN(it_block_eq);
    RUN(it_block_ne);
    TEST_REPORT();
}
