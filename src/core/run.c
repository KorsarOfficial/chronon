#include "core/cpu.h"
#include "core/bus.h"
#include "core/decoder.h"
#include "core/nvic.h"
#include "periph/systick.h"

extern bool execute(cpu_t* c, bus_t* bus, const insn_t* i);

/* Run the CPU until halt or max_steps reached.
   Optional systick pointer lets us tick the counter and dispatch SysTick IRQ. */
u64 run_steps_st(cpu_t* c, bus_t* bus, u64 max_steps, systick_t* st) {
    u64 i = 0;
    for (; i < max_steps && !c->halted; ++i) {
        insn_t ins;
        decode(bus, c->r[REG_PC], &ins);
        if (!execute(c, bus, &ins)) break;

        if (st) {
            systick_tick(st, 1);
            if (st->irq_pending && c->mode == MODE_THREAD && !(c->primask & 1u)) {
                st->irq_pending = false;
                exc_enter(c, bus, EXC_SYSTICK);
            }
        }
    }
    return i;
}

u64 run_steps(cpu_t* c, bus_t* bus, u64 max_steps) {
    return run_steps_st(c, bus, max_steps, NULL);
}
