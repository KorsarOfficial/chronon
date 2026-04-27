#include "core/bus.h"
#include "core/decoder.h"
#include "test_harness.h"

static void setup(bus_t* b) {
    bus_init(b);
    bus_add_flat(b, "flash", 0, 0x1000, false);
}

TEST(mov_imm) {
    bus_t b; setup(&b);
    /* MOV R0, #42 -> 0x202A */
    u16 w = 0x2000 | (0 << 8) | 42;
    bus_load_blob(&b, 0, (u8*)&w, 2);
    insn_t i;
    decode(&b, 0, &i);
    ASSERT_EQ_U32(i.op, OP_MOV_IMM);
    ASSERT_EQ_U32(i.rd, 0);
    ASSERT_EQ_U32(i.imm, 42);
}

TEST(add_reg) {
    bus_t b; setup(&b);
    /* ADDS R2, R0, R1 -> 0x1842 */
    u16 w = 0x1800 | (1 << 6) | (0 << 3) | 2;
    bus_load_blob(&b, 0, (u8*)&w, 2);
    insn_t i;
    decode(&b, 0, &i);
    ASSERT_EQ_U32(i.op, OP_ADD_REG);
    ASSERT_EQ_U32(i.rd, 2);
    ASSERT_EQ_U32(i.rn, 0);
    ASSERT_EQ_U32(i.rm, 1);
}

TEST(add_imm3) {
    bus_t b; setup(&b);
    /* ADDS R1, R0, #5 -> 0x1D41 */
    u16 w = 0x1C00 | (5 << 6) | (0 << 3) | 1;
    bus_load_blob(&b, 0, (u8*)&w, 2);
    insn_t i;
    decode(&b, 0, &i);
    ASSERT_EQ_U32(i.op, OP_ADD_IMM3);
    ASSERT_EQ_U32(i.rd, 1);
    ASSERT_EQ_U32(i.rn, 0);
    ASSERT_EQ_U32(i.imm, 5);
}

TEST(cmp_imm) {
    bus_t b; setup(&b);
    /* CMP R3, #10 -> 0x2B0A */
    u16 w = 0x2800 | (1 << 11) | (3 << 8) | 10;
    bus_load_blob(&b, 0, (u8*)&w, 2);
    insn_t i;
    decode(&b, 0, &i);
    ASSERT_EQ_U32(i.op, OP_CMP_IMM);
    ASSERT_EQ_U32(i.rn, 3);
    ASSERT_EQ_U32(i.imm, 10);
}

TEST(nop) {
    bus_t b; setup(&b);
    u16 w = 0xBF00;
    bus_load_blob(&b, 0, (u8*)&w, 2);
    insn_t i;
    decode(&b, 0, &i);
    ASSERT_EQ_U32(i.op, OP_NOP);
}

/* Regression: T32 barrier instructions (ISB/DSB/DMB) have w0=0xF3BF.
   Before the fix they fell through to the B.cond T3 decoder and produced
   a branch to a garbage address (confirmed crash in test7_freertos at step
   490424: DSB at PC=0xEA2 decoded as B AL imm=786078 -> target=0xC0D44).
   After the fix both 0xF3AF (hints) and 0xF3BF (barriers) decode as NOP. */
TEST(t32_isb_decodes_as_nop) {
    bus_t b; setup(&b);
    /* ISB SY: F3BF 8F6F (little-endian halfwords) */
    u16 words[2] = { 0xF3BFu, 0x8F6Fu };
    bus_load_blob(&b, 0, (u8*)words, 4);
    insn_t i;
    u8 sz = decode(&b, 0, &i);
    ASSERT_EQ_U32(sz, 4);
    ASSERT_EQ_U32(i.op, OP_T32_NOP);
}

TEST(t32_dsb_decodes_as_nop) {
    bus_t b; setup(&b);
    /* DSB SY: F3BF 8F4F */
    u16 words[2] = { 0xF3BFu, 0x8F4Fu };
    bus_load_blob(&b, 0, (u8*)words, 4);
    insn_t i;
    u8 sz = decode(&b, 0, &i);
    ASSERT_EQ_U32(sz, 4);
    ASSERT_EQ_U32(i.op, OP_T32_NOP);
}

TEST(t32_dmb_decodes_as_nop) {
    bus_t b; setup(&b);
    /* DMB SY: F3BF 8F5F */
    u16 words[2] = { 0xF3BFu, 0x8F5Fu };
    bus_load_blob(&b, 0, (u8*)words, 4);
    insn_t i;
    u8 sz = decode(&b, 0, &i);
    ASSERT_EQ_U32(sz, 4);
    ASSERT_EQ_U32(i.op, OP_T32_NOP);
}

int main(void) {
    RUN(mov_imm);
    RUN(add_reg);
    RUN(add_imm3);
    RUN(cmp_imm);
    RUN(nop);
    RUN(t32_isb_decodes_as_nop);
    RUN(t32_dsb_decodes_as_nop);
    RUN(t32_dmb_decodes_as_nop);
    TEST_REPORT();
}
