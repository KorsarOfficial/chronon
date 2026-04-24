#include "core/nvic.h"

/* Cortex-M exception entry / exit with MSP/PSP selection.
   ARM ARM B1.5.6 / B1.5.7. */

bool exc_enter(cpu_t* c, bus_t* b, u8 exc) {
    /* Determine stack to push on: if in Thread mode and CONTROL.SPSEL=1, use PSP;
       otherwise MSP. Also sync current SP back to MSP/PSP. */
    bool use_psp = (c->mode == MODE_THREAD) && ((c->control & 2u) != 0);
    addr_t sp;
    if (use_psp) { c->psp = c->r[REG_SP]; sp = c->psp; }
    else         { c->msp = c->r[REG_SP]; sp = c->msp; }

    sp -= 32;
    addr_t p = sp;

    u32 xpsr = (c->apsr & 0xF8000000u) | (c->epsr & 0x0600FC00u) | ((u32)exc & 0x1FFu);

    if (!bus_write(b, p + 0x00, 4, c->r[REG_R0]))  return false;
    if (!bus_write(b, p + 0x04, 4, c->r[REG_R1]))  return false;
    if (!bus_write(b, p + 0x08, 4, c->r[REG_R2]))  return false;
    if (!bus_write(b, p + 0x0C, 4, c->r[REG_R3]))  return false;
    if (!bus_write(b, p + 0x10, 4, c->r[REG_R12])) return false;
    if (!bus_write(b, p + 0x14, 4, c->r[REG_LR]))  return false;
    if (!bus_write(b, p + 0x18, 4, c->r[REG_PC]))  return false;
    if (!bus_write(b, p + 0x1C, 4, xpsr))           return false;

    if (use_psp) c->psp = sp;
    else         c->msp = sp;

    /* After entry, handler always runs on MSP (Handler mode). */
    c->mode = MODE_HANDLER;
    c->r[REG_SP] = c->msp;

    /* EXC_RETURN: bits[3:0] = 1001 return to thread with MSP,
       1101 return to thread with PSP, 0001 return to handler. */
    u32 exc_ret;
    if (use_psp) exc_ret = 0xFFFFFFFDu;
    else         exc_ret = 0xFFFFFFF9u;
    c->r[REG_LR] = exc_ret;

    /* Jump to handler from vector table. */
    u32 handler = 0;
    if (!bus_read(b, 4u * (u32)exc, 4, &handler)) return false;
    c->r[REG_PC] = handler & ~1u;
    c->ipsr = exc;
    c->itstate = 0;
    return true;
}

bool exc_return(cpu_t* c, bus_t* b, u32 exc_return) {
    /* Select where to pop from. */
    bool to_psp = (exc_return & 0xFu) == 0xDu;
    bool to_handler = (exc_return & 0xFu) == 0x1u;

    addr_t sp = to_psp ? c->psp : c->msp;

    u32 r0, r1, r2, r3, r12, lr, pc, xpsr;
    if (!bus_read(b, sp + 0x00, 4, &r0))  return false;
    if (!bus_read(b, sp + 0x04, 4, &r1))  return false;
    if (!bus_read(b, sp + 0x08, 4, &r2))  return false;
    if (!bus_read(b, sp + 0x0C, 4, &r3))  return false;
    if (!bus_read(b, sp + 0x10, 4, &r12)) return false;
    if (!bus_read(b, sp + 0x14, 4, &lr))  return false;
    if (!bus_read(b, sp + 0x18, 4, &pc))  return false;
    if (!bus_read(b, sp + 0x1C, 4, &xpsr))return false;
    c->r[REG_R0] = r0;  c->r[REG_R1] = r1;
    c->r[REG_R2] = r2;  c->r[REG_R3] = r3;
    c->r[REG_R12] = r12;
    c->r[REG_LR] = lr;
    c->r[REG_PC] = pc & ~1u;
    c->apsr = xpsr & 0xF8000000u;
    c->ipsr = xpsr & 0x1FFu;

    if (to_psp)        c->psp = sp + 32;
    else if (to_handler) c->msp = sp + 32;
    else               c->msp = sp + 32;

    if (to_handler) {
        c->mode = MODE_HANDLER;
        c->r[REG_SP] = c->msp;
    } else {
        c->mode = MODE_THREAD;
        /* Update CONTROL.SPSEL based on where we returned. */
        if (to_psp) {
            c->control |= 2u;
            c->r[REG_SP] = c->psp;
        } else {
            c->control &= ~2u;
            c->r[REG_SP] = c->msp;
        }
    }
    c->itstate = 0;
    return true;
}
