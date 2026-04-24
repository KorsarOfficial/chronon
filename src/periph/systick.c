#include "periph/systick.h"

static u32 st_read(void* ctx, addr_t off, u32 size) {
    (void)size;
    systick_t* t = (systick_t*)ctx;
    switch (off) {
        case 0x00: {
            u32 v = t->csr;
            if (t->count_flag) { v |= (1u << 16); t->count_flag = false; }
            return v;
        }
        case 0x04: return t->rvr;
        case 0x08: return t->cvr & 0x00FFFFFFu;
        case 0x0C: return 0;
    }
    return 0;
}

static void st_write(void* ctx, addr_t off, u32 val, u32 size) {
    (void)size;
    systick_t* t = (systick_t*)ctx;
    switch (off) {
        case 0x00: t->csr = val & 0x07u; break;
        case 0x04: t->rvr = val & 0x00FFFFFFu; break;
        case 0x08: t->cvr = 0; t->count_flag = false; break;
    }
}

int systick_attach(bus_t* b, systick_t* t) {
    *t = (systick_t){0};
    return bus_add_mmio(b, "systick", SYSTICK_BASE, SYSTICK_SIZE, t, st_read, st_write);
}

/* Advance counter by `cycles`. If enabled and hit 0, reload and flag. */
void systick_tick(systick_t* t, u32 cycles) {
    if (!(t->csr & 1u)) return; /* disabled */
    u32 cvr = t->cvr;
    /* On enable transition, cvr is still 0 by spec; first reload is from RVR.
       To avoid an immediate IRQ storm before firmware sets up PSP, we only
       raise IRQ when cvr was > 0 and reached 0 through counting. */
    if (cvr == 0) cvr = t->rvr;
    while (cycles) {
        if (cvr == 0) break; /* RVR = 0: stopped */
        if (cycles >= cvr) {
            cycles -= cvr;
            cvr = t->rvr;
            t->count_flag = true;
            if (t->csr & 2u) t->irq_pending = true;
            if (cvr == 0) break;
        } else {
            cvr -= cycles;
            cycles = 0;
        }
    }
    t->cvr = cvr;
}
