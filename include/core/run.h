#ifndef CORTEX_M_RUN_H
#define CORTEX_M_RUN_H

#include "core/types.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "periph/systick.h"
#include "periph/scb.h"
#include "core/jit.h"

/* Run with explicit jit instance (for TT determinism: caller owns jit_t). */
u64 run_steps_full_g(cpu_t* c, bus_t* bus, u64 max_steps,
                     systick_t* st, scb_t* scb, jit_t* g);
u64 run_steps_full  (cpu_t* c, bus_t* bus, u64 max_steps,
                     systick_t* st, scb_t* scb);
u64 run_steps_st    (cpu_t* c, bus_t* bus, u64 max_steps, systick_t* st);
u64 run_steps       (cpu_t* c, bus_t* bus, u64 max_steps);
void run_dcache_invalidate(void);

#endif
