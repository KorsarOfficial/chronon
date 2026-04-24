#ifndef CORTEX_M_NVIC_H
#define CORTEX_M_NVIC_H

#include "core/types.h"
#include "core/cpu.h"
#include "core/bus.h"

/* Cortex-M exception numbers (ARM ARM B1.5.2):
   1   Reset
   2   NMI
   3   HardFault
   11  SVCall
   14  PendSV
   15  SysTick
   16+ IRQ0..IRQn */

#define EXC_RESET      1
#define EXC_NMI        2
#define EXC_HARD_FAULT 3
#define EXC_SVC        11
#define EXC_PENDSV     14
#define EXC_SYSTICK    15

/* Stack frame pushed on exception entry (ARM ARM B1.5.6):
   [SP+0x00] R0
   [SP+0x04] R1
   [SP+0x08] R2
   [SP+0x0C] R3
   [SP+0x10] R12
   [SP+0x14] LR
   [SP+0x18] return addr
   [SP+0x1C] xPSR */

/* Trigger exception entry: save context, set PC to handler from VTOR[exc]. */
bool exc_enter(cpu_t* c, bus_t* b, u8 exc);
/* EXC_RETURN value execution: restore context, pop stack, return to thread. */
bool exc_return(cpu_t* c, bus_t* b, u32 exc_return);

#endif
