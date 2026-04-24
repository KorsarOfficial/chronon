#ifndef CORTEX_M_SYSTICK_H
#define CORTEX_M_SYSTICK_H

#include "core/types.h"
#include "core/bus.h"

/* SysTick registers at 0xE000E010 (SCS/ITM/NVIC region):
   +0x00 CSR   : bit 0 ENABLE, bit 1 TICKINT, bit 2 CLKSOURCE, bit 16 COUNTFLAG (RC)
   +0x04 RVR   : reload value (24-bit)
   +0x08 CVR   : current value (24-bit) — writes clear it
   +0x0C CALIB : implementation calibration (we return 0) */

#define SYSTICK_BASE 0xE000E010u
#define SYSTICK_SIZE 0x10u

typedef struct systick_s {
    u32 csr;
    u32 rvr;
    u32 cvr;
    bool count_flag;
    bool irq_pending;
} systick_t;

int  systick_attach(bus_t* b, systick_t* t);
void systick_tick(systick_t* t, u32 cycles); /* advance by N cycles */

#endif
