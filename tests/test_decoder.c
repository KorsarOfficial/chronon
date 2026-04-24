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

int main(void) {
    RUN(mov_imm);
    RUN(add_reg);
    RUN(add_imm3);
    RUN(cmp_imm);
    RUN(nop);
    TEST_REPORT();
}
