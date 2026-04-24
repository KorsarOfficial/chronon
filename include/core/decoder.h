#ifndef CORTEX_M_DECODER_H
#define CORTEX_M_DECODER_H

#include "core/types.h"

/* Decoded instruction. Encoding is identified by opcode enum, operands
   are normalized so the executor does not re-parse bits. */

typedef enum {
    OP_UNDEFINED = 0,

    /* ARMv6-M Thumb-1 encodings */
    OP_LSL_IMM, OP_LSR_IMM, OP_ASR_IMM,
    OP_ADD_REG, OP_SUB_REG,
    OP_ADD_IMM3, OP_SUB_IMM3,
    OP_MOV_IMM, OP_CMP_IMM, OP_ADD_IMM8, OP_SUB_IMM8,
    OP_AND_REG, OP_EOR_REG, OP_LSL_REG, OP_LSR_REG,
    OP_ASR_REG, OP_ADC_REG, OP_SBC_REG, OP_ROR_REG,
    OP_TST_REG, OP_RSB_IMM, OP_CMP_REG, OP_CMN_REG,
    OP_ORR_REG, OP_MUL, OP_BIC_REG, OP_MVN_REG,
    OP_ADD_REG_T2, OP_CMP_REG_T2, OP_MOV_REG,
    OP_BX, OP_BLX_REG,
    OP_LDR_LIT,
    OP_STR_REG, OP_STRH_REG, OP_STRB_REG, OP_LDRSB_REG,
    OP_LDR_REG, OP_LDRH_REG, OP_LDRB_REG, OP_LDRSH_REG,
    OP_STR_IMM, OP_LDR_IMM, OP_STRB_IMM, OP_LDRB_IMM,
    OP_STRH_IMM, OP_LDRH_IMM,
    OP_STR_SP, OP_LDR_SP,
    OP_ADR, OP_ADD_SP_IMM,
    OP_ADD_SP_SP, OP_SUB_SP_SP,
    OP_SXTH, OP_SXTB, OP_UXTH, OP_UXTB,
    OP_PUSH, OP_POP,
    OP_REV, OP_REV16, OP_REVSH,
    OP_CPS, OP_BKPT,
    OP_STM, OP_LDM,
    OP_B_COND, OP_B_UNCOND,
    OP_SVC, OP_UDF,
    OP_NOP, OP_YIELD, OP_WFE, OP_WFI, OP_SEV,
    OP_IT,

    /* Thumb-2 32-bit encodings (ARMv7-M) */
    OP_T32_BL,
    OP_T32_BRANCH_COND,
    OP_T32_DATA_IMM,
    OP_T32_DATA_REG,
    OP_T32_LDR_IMM, OP_T32_STR_IMM,
    OP_T32_LDRH_IMM, OP_T32_STRH_IMM,
    OP_T32_LDRB_IMM, OP_T32_STRB_IMM,
    OP_T32_MSR, OP_T32_MRS,
    OP_T32_LDMIA, OP_T32_STMDB,
    /* Placeholder — full set in decoder/t32.c. */

    OP_COUNT
} opcode_t;

typedef struct insn_s {
    opcode_t op;
    u32 pc;           /* instruction address */
    u8  size;         /* 2 (Thumb) or 4 (Thumb-2) */
    u8  rd, rn, rm, rs;
    u32 imm;          /* pre-decoded immediate (zero/sign extended) */
    u32 raw;          /* raw 16 or 32 bit encoding for debug */
    u8  cond;         /* condition code (0xE = AL = unconditional) */
    u32 reg_list;     /* bitmask for PUSH/POP/LDM/STM */
    /* IT block context is carried on CPU, not per-insn. */
} insn_t;

/* Decode at PC. Returns size (2 or 4). Sets out->op = OP_UNDEFINED on fail. */
struct bus_s;
u8 decode(struct bus_s* bus, addr_t pc, insn_t* out);

const char* opcode_name(opcode_t op);

#endif
