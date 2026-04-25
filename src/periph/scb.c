#include "periph/scb.h"
#include "core/cpu.h"

extern cpu_t* g_cpu_for_scb;

static u32 scb_read(void* ctx, addr_t off, u32 size) {
    (void)size;
    scb_t* s = (scb_t*)ctx;
    switch (off) {
        case 0x00: return 0x410FC231u;
        case 0x04: {
            u32 v = 0;
            if (s->pendsv_pending) v |= (1u << 28);
            return v;
        }
        case 0x08: return s->vtor;
        case 0x0C: return s->aircr;
        case 0x14: return s->ccr;
        case 0x28: return g_cpu_for_scb ? g_cpu_for_scb->cfsr : 0;
        case 0x2C: return g_cpu_for_scb ? g_cpu_for_scb->hfsr : 0;
        case 0x34: return g_cpu_for_scb ? g_cpu_for_scb->mmfar : 0;
        case 0x38: return g_cpu_for_scb ? g_cpu_for_scb->bfar : 0;
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
        case 0x28: if (g_cpu_for_scb) g_cpu_for_scb->cfsr &= ~val; break;
        case 0x2C: if (g_cpu_for_scb) g_cpu_for_scb->hfsr &= ~val; break;
    }
}

cpu_t* g_cpu_for_scb = NULL;

int scb_attach(bus_t* b, scb_t* s) {
    *s = (scb_t){0};
    return bus_add_mmio(b, "scb", SCB_BASE, SCB_SIZE, s, scb_read, scb_write);
}
