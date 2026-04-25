#ifndef CORTEX_M_CPU_H
#define CORTEX_M_CPU_H

#include "core/types.h"
#include "core/fpu.h"

/* ARMv7-M register file: R0-R12 general, R13=SP, R14=LR, R15=PC.
   Two stack pointers per ARM ARM B1.4.7: MSP (main) and PSP (process).
   Active SP selected by CONTROL.SPSEL. */

#define REG_R0   0
#define REG_R1   1
#define REG_R2   2
#define REG_R3   3
#define REG_R4   4
#define REG_R5   5
#define REG_R6   6
#define REG_R7   7
#define REG_R8   8
#define REG_R9   9
#define REG_R10  10
#define REG_R11  11
#define REG_R12  12
#define REG_SP   13
#define REG_LR   14
#define REG_PC   15

/* APSR flag bits (ARM ARM B1.4.2). */
#define APSR_N (1u << 31)
#define APSR_Z (1u << 30)
#define APSR_C (1u << 29)
#define APSR_V (1u << 28)
#define APSR_Q (1u << 27)

/* IPSR holds current exception number (0 if Thread mode). */

typedef enum {
    MODE_THREAD = 0,
    MODE_HANDLER = 1,
} cpu_mode_t;

typedef enum {
    CORE_M0 = 0,
    CORE_M0_PLUS,
    CORE_M1,
    CORE_M3,
    CORE_M4,
    CORE_M7,
} core_t;

typedef struct cpu_s {
    u32 r[16];        /* R0..R15; r[SP] aliases active SP */
    u32 msp;          /* main stack pointer */
    u32 psp;          /* process stack pointer */
    u32 apsr;         /* flags: N Z C V Q */
    u32 ipsr;         /* exception number */
    u32 epsr;         /* T bit + IT state */
    u32 primask;      /* interrupt mask */
    u32 faultmask;    /* fault mask */
    u32 basepri;      /* priority threshold */
    u32 control;      /* nPriv + SPSEL + FPCA */
    cpu_mode_t mode;
    core_t core;
    u64 cycles;       /* instruction counter */
    bool halted;      /* CPU stopped (wfi or sim shutdown) */
    /* ITSTATE per ARM ARM A7.3.2: 8 bits encoding cond + length. */
    u8 itstate;
    fpu_t fpu;
} cpu_t;

/* IT helpers: extract current cond and step state. */
FORCE_INLINE bool cpu_in_it(const cpu_t* c) { return (c->itstate & 0x0F) != 0; }
FORCE_INLINE u8   cpu_it_cond(const cpu_t* c) { return (c->itstate >> 4) & 0xF; }
void cpu_it_advance(cpu_t* c);

void cpu_reset(cpu_t* c, core_t core);
u32  cpu_read_reg(const cpu_t* c, u32 n);
void cpu_write_reg(cpu_t* c, u32 n, u32 v);
void cpu_set_flags_nz(cpu_t* c, u32 result);
void cpu_set_flags_nzcv_add(cpu_t* c, u32 a, u32 b, u32 result, bool carry_in);
void cpu_set_flags_nzcv_sub(cpu_t* c, u32 a, u32 b, u32 result);

#endif
