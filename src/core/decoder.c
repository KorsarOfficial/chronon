#include "core/decoder.h"
#include "core/bus.h"

/* Minimal Thumb-1 decoder skeleton. Grows with each phase.
   Full table in ARM ARM A5.2 (ARMv7-M, The Thumb instruction set encoding).
   Layout: bits 15:10 select top-level group. */

static void set_undef(insn_t* i, u32 raw, addr_t pc) {
    i->op = OP_UNDEFINED; i->pc = pc; i->size = 2; i->raw = raw;
    i->rd = i->rn = i->rm = i->rs = 0; i->imm = 0; i->cond = 0xE; i->reg_list = 0;
}

static u8 decode_thumb16(u16 w, addr_t pc, insn_t* out) {
    set_undef(out, w, pc);
    out->raw = w;

    /* ARM ARM A5.2.1: bit[15:11] discriminator.
       00011 = ADD/SUB reg/imm3, else 000xx = shift-immediate. */

    /* ADD/SUB register or immediate (3-bit): bit[15:11] = 00011 */
    if ((w & 0xF800u) == 0x1800u) {
        u8 i_bit = (w >> 10) & 1;
        u8 opc   = (w >> 9)  & 1;
        out->rn = (w >> 3) & 0x7;
        out->rd = w & 0x7;
        if (i_bit == 0) {
            out->rm = (w >> 6) & 0x7;
            out->op = opc ? OP_SUB_REG : OP_ADD_REG;
        } else {
            out->imm = (w >> 6) & 0x7;
            out->op = opc ? OP_SUB_IMM3 : OP_ADD_IMM3;
        }
        return 2;
    }

    /* Shift (immediate): bit[15:13] = 000 and NOT add/sub reg/imm3. */
    if ((w & 0xE000u) == 0x0000u) {
        u8 op = (w >> 11) & 0x3;
        u8 imm5 = (w >> 6) & 0x1F;
        out->rm = (w >> 3) & 0x7;
        out->rd = w & 0x7;
        out->imm = imm5;
        if (op == 0) out->op = OP_LSL_IMM;
        else if (op == 1) out->op = OP_LSR_IMM;
        else              out->op = OP_ASR_IMM;
        return 2;
    }

    /* Move/compare/add/subtract immediate (8-bit) */
    if ((w & 0xE000u) == 0x2000u) {
        u8 op = (w >> 11) & 0x3;
        out->rd = (w >> 8) & 0x7;
        out->rn = out->rd;
        out->imm = w & 0xFF;
        switch (op) {
            case 0: out->op = OP_MOV_IMM;  break;
            case 1: out->op = OP_CMP_IMM;  break;
            case 2: out->op = OP_ADD_IMM8; break;
            case 3: out->op = OP_SUB_IMM8; break;
        }
        return 2;
    }

    /* Data-processing (register) — ARM ARM A5.2.2 */
    if ((w & 0xFC00u) == 0x4000u) {
        u8 op = (w >> 6) & 0xF;
        out->rm = (w >> 3) & 0x7;
        out->rd = w & 0x7;
        out->rn = out->rd;
        static const opcode_t ops[16] = {
            OP_AND_REG, OP_EOR_REG, OP_LSL_REG, OP_LSR_REG,
            OP_ASR_REG, OP_ADC_REG, OP_SBC_REG, OP_ROR_REG,
            OP_TST_REG, OP_RSB_IMM, OP_CMP_REG, OP_CMN_REG,
            OP_ORR_REG, OP_MUL,     OP_BIC_REG, OP_MVN_REG
        };
        out->op = ops[op];
        return 2;
    }

    /* Conditional branch B<cond> imm8 (or SVC/UDF) — A5.2.6 */
    if ((w & 0xF000u) == 0xD000u) {
        u8 cond = (w >> 8) & 0xF;
        if (cond == 0xE) { out->op = OP_UDF; out->imm = w & 0xFF; return 2; }
        if (cond == 0xF) { out->op = OP_SVC; out->imm = w & 0xFF; return 2; }
        out->op = OP_B_COND;
        out->cond = cond;
        i32 off = (i32)(i8)(w & 0xFF);
        out->imm = (u32)(off << 1);
        return 2;
    }

    /* Unconditional branch B imm11 — A5.2.7 */
    if ((w & 0xF800u) == 0xE000u) {
        out->op = OP_B_UNCOND;
        out->cond = 0xE;
        i32 off = (i32)((w & 0x7FF) << 21) >> 20; /* sign-extend 11-bit, <<1 */
        out->imm = (u32)off;
        return 2;
    }

    /* NOP-hint 0xBF00. Hints form: 1011 1111 xxxx yyyy — A5.2.8.
       For Phase 1 we map 0xBF00 (NOP). Others extended later. */
    if (w == 0xBF00u) { out->op = OP_NOP; return 2; }

    /* Unknown — leave as UNDEFINED. */
    return 2;
}

static bool is_t32(u16 w) {
    /* ARM ARM A5.1: 11101, 11110, 11111 => 32-bit Thumb-2 */
    u16 top5 = (w >> 11) & 0x1F;
    return top5 == 0x1D || top5 == 0x1E || top5 == 0x1F;
}

u8 decode(struct bus_s* bus, addr_t pc, insn_t* out) {
    u16 w0 = bus_r16(bus, pc);
    if (!is_t32(w0)) {
        return decode_thumb16(w0, pc, out);
    }
    /* Phase 2: full Thumb-2 decode. For now mark undefined with size=4. */
    set_undef(out, w0, pc);
    out->size = 4;
    return 4;
}

const char* opcode_name(opcode_t op) {
    static const char* names[] = {
        [OP_UNDEFINED] = "UNDEF",
        [OP_LSL_IMM]   = "LSL",
        [OP_LSR_IMM]   = "LSR",
        [OP_ASR_IMM]   = "ASR",
        [OP_ADD_REG]   = "ADD",
        [OP_SUB_REG]   = "SUB",
        [OP_ADD_IMM3]  = "ADD",
        [OP_SUB_IMM3]  = "SUB",
        [OP_MOV_IMM]   = "MOV",
        [OP_CMP_IMM]   = "CMP",
        [OP_ADD_IMM8]  = "ADD",
        [OP_SUB_IMM8]  = "SUB",
        [OP_AND_REG]   = "AND",
        [OP_EOR_REG]   = "EOR",
        [OP_MUL]       = "MUL",
        [OP_B_COND]    = "B.cond",
        [OP_B_UNCOND]  = "B",
        [OP_SVC]       = "SVC",
        [OP_UDF]       = "UDF",
        [OP_NOP]       = "NOP",
    };
    if (op >= OP_COUNT) return "???";
    const char* n = names[op];
    return n ? n : "???";
}
