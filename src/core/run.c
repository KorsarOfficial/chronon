#include <stdio.h>
#include <stdlib.h>
#include "core/cpu.h"
#include "core/bus.h"
#include "core/decoder.h"
#include "core/nvic.h"
#include "periph/systick.h"
#include "periph/scb.h"

extern bool execute(cpu_t* c, bus_t* bus, const insn_t* i);

/* Priorities: SysTick > PendSV by default. Both pre-empt Thread mode when IRQs
   are not masked. Within Handler mode, tail-chaining is handled implicitly
   because we only enter a new exception when no other handler is running. */
u64 run_steps_full(cpu_t* c, bus_t* bus, u64 max_steps,
                   systick_t* st, scb_t* scb) {
    u64 i = 0;
    for (; i < max_steps && !c->halted; ++i) {
        insn_t ins;
        decode(bus, c->r[REG_PC], &ins);
        if (!execute(c, bus, &ins)) break;

        if (st) systick_tick(st, 1);

        /* Only pre-empt Thread mode (no nested exceptions in this core). */
        if (c->mode == MODE_THREAD && !(c->primask & 1u)) {
            if (st && st->irq_pending) {
                st->irq_pending = false;
                exc_enter(c, bus, EXC_SYSTICK);
                continue;
            }
            if (scb && scb->pendsv_pending) {
                scb->pendsv_pending = false;
                exc_enter(c, bus, EXC_PENDSV);
                continue;
            }
        }
    }
    return i;
}

u64 run_steps_st(cpu_t* c, bus_t* bus, u64 max_steps, systick_t* st) {
    return run_steps_full(c, bus, max_steps, st, NULL);
}

u64 run_steps(cpu_t* c, bus_t* bus, u64 max_steps) {
    return run_steps_full(c, bus, max_steps, NULL, NULL);
}
