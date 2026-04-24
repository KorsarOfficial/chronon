#include "periph/scb.h"

static u32 scb_read(void* ctx, addr_t off, u32 size) {
    (void)size;
    scb_t* s = (scb_t*)ctx;
    switch (off) {
        case 0x00: return 0x410FC231u;  /* Fake Cortex-M3 r2p1 CPUID */
        case 0x04: {
            u32 v = 0;
            if (s->pendsv_pending) v |= (1u << 28);
            return v;
        }
        case 0x08: return s->vtor;
        case 0x0C: return s->aircr;
        case 0x14: return s->ccr;
    }
    return 0;
}

static void scb_write(void* ctx, addr_t off, u32 val, u32 size) {
    (void)size;
    scb_t* s = (scb_t*)ctx;
    switch (off) {
        case 0x04: /* ICSR */
            if (val & (1u << 28)) s->pendsv_pending = true;        /* PENDSVSET */
            if (val & (1u << 27)) s->pendsv_pending = false;       /* PENDSVCLR */
            if (val & (1u << 26)) s->systick_pending_manual = true;/* PENDSTSET */
            if (val & (1u << 25)) s->systick_pending_manual = false;
            break;
        case 0x08: s->vtor  = val & 0xFFFFFF80u; break;
        case 0x0C: s->aircr = val; break;
        case 0x14: s->ccr   = val; break;
    }
}

int scb_attach(bus_t* b, scb_t* s) {
    *s = (scb_t){0};
    return bus_add_mmio(b, "scb", SCB_BASE, SCB_SIZE, s, scb_read, scb_write);
}
