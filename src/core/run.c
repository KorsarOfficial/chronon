#include "core/cpu.h"
#include "core/bus.h"
#include "core/decoder.h"

extern bool execute(cpu_t* c, bus_t* bus, const insn_t* i);

/* Run the CPU until halt or max_steps reached.
   Returns number of instructions executed. */
u64 run_steps(cpu_t* c, bus_t* bus, u64 max_steps) {
    u64 i = 0;
    for (; i < max_steps && !c->halted; ++i) {
        insn_t ins;
        decode(bus, c->r[REG_PC], &ins);
        if (!execute(c, bus, &ins)) break;
    }
    return i;
}
