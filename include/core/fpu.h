#ifndef CORTEX_M_FPU_H
#define CORTEX_M_FPU_H

#include "core/types.h"

/* VFPv4 single-precision register file (ARMv7-M FPU, ARM ARM B3.6).
   32 single-precision regs S0..S31, viewable as 16 double D0..D15 (we
   support single only). */
typedef struct fpu_s {
    union {
        float    s[32];
        u32      u[32];
        double   d[16];
    } reg;
    u32 fpscr;     /* status/control */
    u32 fpccr;     /* coproc control */
    bool enabled;  /* CPACR.CP10/CP11 */
} fpu_t;

void fpu_reset(fpu_t* f);

#endif
