#include "core/cpu.h"
#include <string.h>

void cpu_reset(cpu_t* c, core_t core) {
    memset(c, 0, sizeof(*c));
    c->core = core;
    c->mode = MODE_THREAD;
    c->epsr = (1u << 24);
    c->halted = false;
    fpu_reset(&c->fpu);
}

u32 cpu_read_reg(const cpu_t* c, u32 n) {
    /* PC reads as current PC + 4 per ARM ARM A2.3.1. Caller adjusts. */
    return c->r[n & 0xF];
}

void cpu_write_reg(cpu_t* c, u32 n, u32 v) {
    n &= 0xF;
    if (n == REG_PC) {
        c->r[REG_PC] = v & ~1u; /* EPSR.T stays 1 */
    } else {
        c->r[n] = v;
    }
}

void cpu_set_flags_nz(cpu_t* c, u32 result) {
    u32 f = c->apsr & ~(APSR_N | APSR_Z);
    if (result & 0x80000000u) f |= APSR_N;
    if (result == 0)          f |= APSR_Z;
    c->apsr = f;
}

/* Addition flags: result = a + b + carry_in.
   C = carry out, V = signed overflow. */
void cpu_set_flags_nzcv_add(cpu_t* c, u32 a, u32 b, u32 result, bool carry_in) {
    u32 f = c->apsr & ~(APSR_N | APSR_Z | APSR_C | APSR_V);
    if (result & 0x80000000u) f |= APSR_N;
    if (result == 0)          f |= APSR_Z;
    u64 ext = (u64)a + (u64)b + (carry_in ? 1u : 0u);
    if (ext >> 32)            f |= APSR_C;
    if ((~(a ^ b) & (a ^ result)) & 0x80000000u) f |= APSR_V;
    c->apsr = f;
}

/* ITSTATE bits[7:5] = firstcond[3:1] (constant in block),
   bits[4:0] = state, where bit[4] is current cond[0] (T/E flips it),
   bits[3:0] are the remaining mask. Algorithm per ARM ARM A7.3.2. */
void cpu_it_advance(cpu_t* c) {
    if ((c->itstate & 0x07) == 0) {
        c->itstate = 0; /* IT block over */
    } else {
        u8 hi3 = c->itstate & 0xE0;
        u8 lo5 = c->itstate & 0x1F;
        lo5 = (u8)((lo5 << 1) & 0x1F);
        c->itstate = (u8)(hi3 | lo5);
    }
}

/* Subtraction: result = a - b = a + ~b + 1. */
void cpu_set_flags_nzcv_sub(cpu_t* c, u32 a, u32 b, u32 result) {
    u32 f = c->apsr & ~(APSR_N | APSR_Z | APSR_C | APSR_V);
    if (result & 0x80000000u) f |= APSR_N;
    if (result == 0)          f |= APSR_Z;
    if ((u64)a >= (u64)b)     f |= APSR_C; /* no borrow */
    if (((a ^ b) & (a ^ result)) & 0x80000000u) f |= APSR_V;
    c->apsr = f;
}
