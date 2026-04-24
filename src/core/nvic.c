#include "core/nvic.h"

/* VTOR is at 0xE000ED08 on ARMv7-M, but for simplicity we use fixed vector
   table at address 0 (ARMv6-M default). Real VTOR support comes later. */

bool exc_enter(cpu_t* c, bus_t* b, u8 exc) {
    /* Select SP for stacking. On entry, MSP is always used (ARM ARM B1.5.6).
       Push 8 words: R0, R1, R2, R3, R12, LR, ReturnAddress, xPSR. */
    addr_t sp = c->r[REG_SP];
    sp -= 32;
    addr_t p = sp;

    u32 xpsr = (c->apsr & 0xF8000000u) | ((c->epsr & 0x0600FC00u) | (exc & 0x1FFu));

    if (!bus_write(b, p + 0x00, 4, c->r[REG_R0]))  return false;
    if (!bus_write(b, p + 0x04, 4, c->r[REG_R1]))  return false;
    if (!bus_write(b, p + 0x08, 4, c->r[REG_R2]))  return false;
    if (!bus_write(b, p + 0x0C, 4, c->r[REG_R3]))  return false;
    if (!bus_write(b, p + 0x10, 4, c->r[REG_R12])) return false;
    if (!bus_write(b, p + 0x14, 4, c->r[REG_LR]))  return false;
    if (!bus_write(b, p + 0x18, 4, c->r[REG_PC]))  return false;
    if (!bus_write(b, p + 0x1C, 4, xpsr))           return false;

    c->r[REG_SP] = sp;
    c->msp = sp;

    /* LR = EXC_RETURN value; for thread MSP return: 0xFFFFFFF9.
       For thread PSP: 0xFFFFFFFD. Handler: 0xFFFFFFF1. */
    c->r[REG_LR] = 0xFFFFFFF9u;

    /* Read handler address from vector table at 4 * exc. */
    u32 handler = 0;
    if (!bus_read(b, 4u * (u32)exc, 4, &handler)) return false;
    c->r[REG_PC] = handler & ~1u;
    c->mode = MODE_HANDLER;
    c->ipsr = exc;
    c->itstate = 0;
    return true;
}

bool exc_return(cpu_t* c, bus_t* b, u32 exc_return) {
    (void)exc_return; /* We only support 0xFFFFFFF9 (thread+MSP) here. */
    addr_t sp = c->r[REG_SP];
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
    c->r[REG_SP] = sp + 32;
    c->msp = c->r[REG_SP];
    c->mode = MODE_THREAD;
    c->itstate = 0;
    return true;
}
