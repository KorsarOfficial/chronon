#include "core/cpu.h"
#include "core/bus.h"
#include "core/decoder.h"

/* Condition code evaluation. ARM ARM A7.3. */
static bool cond_pass(u32 apsr, u8 cond) {
    bool n = (apsr & APSR_N) != 0;
    bool z = (apsr & APSR_Z) != 0;
    bool c = (apsr & APSR_C) != 0;
    bool v = (apsr & APSR_V) != 0;
    switch (cond & 0xF) {
        case 0x0: return z;                 /* EQ */
        case 0x1: return !z;                /* NE */
        case 0x2: return c;                 /* CS/HS */
        case 0x3: return !c;                /* CC/LO */
        case 0x4: return n;                 /* MI */
        case 0x5: return !n;                /* PL */
        case 0x6: return v;                 /* VS */
        case 0x7: return !v;                /* VC */
        case 0x8: return c && !z;           /* HI */
        case 0x9: return !c || z;           /* LS */
        case 0xA: return n == v;            /* GE */
        case 0xB: return n != v;            /* LT */
        case 0xC: return !z && (n == v);    /* GT */
        case 0xD: return z || (n != v);     /* LE */
        case 0xE: return true;              /* AL */
        case 0xF: return true;              /* reserved */
    }
    return false;
}

/* Executor: given decoded insn, mutate CPU/bus state. Returns false on fault. */
bool execute(cpu_t* c, bus_t* bus, const insn_t* i) {
    /* Advance PC to next by default; branches override. */
    u32 next_pc = c->r[REG_PC] + i->size;

    switch (i->op) {
        case OP_NOP:
            break;

        case OP_MOV_IMM: {
            c->r[i->rd] = i->imm;
            cpu_set_flags_nz(c, i->imm);
            break;
        }

        case OP_ADD_REG: {
            u32 a = c->r[i->rn], b = c->r[i->rm];
            u32 r = a + b;
            c->r[i->rd] = r;
            cpu_set_flags_nzcv_add(c, a, b, r, false);
            break;
        }

        case OP_SUB_REG: {
            u32 a = c->r[i->rn], b = c->r[i->rm];
            u32 r = a - b;
            c->r[i->rd] = r;
            cpu_set_flags_nzcv_sub(c, a, b, r);
            break;
        }

        case OP_ADD_IMM3:
        case OP_ADD_IMM8: {
            u32 a = c->r[i->rn], b = i->imm;
            u32 r = a + b;
            c->r[i->rd] = r;
            cpu_set_flags_nzcv_add(c, a, b, r, false);
            break;
        }

        case OP_SUB_IMM3:
        case OP_SUB_IMM8: {
            u32 a = c->r[i->rn], b = i->imm;
            u32 r = a - b;
            c->r[i->rd] = r;
            cpu_set_flags_nzcv_sub(c, a, b, r);
            break;
        }

        case OP_CMP_IMM: {
            u32 a = c->r[i->rn], b = i->imm;
            cpu_set_flags_nzcv_sub(c, a, b, a - b);
            break;
        }

        case OP_CMP_REG: {
            u32 a = c->r[i->rn], b = c->r[i->rm];
            cpu_set_flags_nzcv_sub(c, a, b, a - b);
            break;
        }

        case OP_AND_REG: {
            u32 r = c->r[i->rn] & c->r[i->rm];
            c->r[i->rd] = r;
            cpu_set_flags_nz(c, r);
            break;
        }

        case OP_EOR_REG: {
            u32 r = c->r[i->rn] ^ c->r[i->rm];
            c->r[i->rd] = r;
            cpu_set_flags_nz(c, r);
            break;
        }

        case OP_LSL_IMM: {
            u32 v = c->r[i->rm];
            u32 r = i->imm ? (v << i->imm) : v;
            c->r[i->rd] = r;
            cpu_set_flags_nz(c, r);
            break;
        }

        case OP_LSR_IMM: {
            u32 v = c->r[i->rm];
            u32 sh = i->imm ? i->imm : 32;
            u32 r = sh >= 32 ? 0 : (v >> sh);
            c->r[i->rd] = r;
            cpu_set_flags_nz(c, r);
            break;
        }

        case OP_MUL: {
            u32 r = c->r[i->rn] * c->r[i->rm];
            c->r[i->rd] = r;
            cpu_set_flags_nz(c, r);
            break;
        }

        case OP_B_COND: {
            if (cond_pass(c->apsr, i->cond)) {
                /* PC at branch = current + 4 (pipeline offset per ARM ARM). */
                next_pc = c->r[REG_PC] + 4 + i->imm;
            }
            break;
        }

        case OP_B_UNCOND: {
            next_pc = c->r[REG_PC] + 4 + i->imm;
            break;
        }

        case OP_UDF:
            c->halted = true;
            return false;

        case OP_SVC:
            /* Phase 1: treat SVC as halt with marker. Full impl in NVIC. */
            c->halted = true;
            return true;

        default:
            c->halted = true;
            return false;
    }

    c->r[REG_PC] = next_pc;
    c->cycles++;
    return true;
}
