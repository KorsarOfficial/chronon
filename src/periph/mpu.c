#include "periph/mpu.h"

/* RASR fields (ARM ARM B3.5.5):
   bit[0]      ENABLE
   bit[5:1]    SIZE (region size = 2^(SIZE+1) bytes; SIZE >= 4 → 32 bytes min)
   bit[15:8]   SRD (sub-region disable, 8 bits, divides into 8 sub-regions)
   bit[26:24]  AP (access permissions)
   bit[28]     XN (execute never)
*/

static u32 region_size_bytes(u32 rasr) {
    u32 sz = (rasr >> 1) & 0x1F;
    if (sz < 4) return 0;
    return 1u << (sz + 1);
}

static bool ap_permits(u32 ap, bool is_write, bool is_priv) {
    /* AP per ARM ARM B3.5.5:
       000 no access
       001 priv RW, unpriv no
       010 priv RW, unpriv RO
       011 priv RW, unpriv RW
       100 reserved
       101 priv RO, unpriv no
       110 priv RO, unpriv RO
       111 priv RO, unpriv RO */
    switch (ap & 7) {
        case 0: return false;
        case 1: return is_priv;
        case 2: return is_priv || !is_write;
        case 3: return true;
        case 5: return is_priv && !is_write;
        case 6: case 7: return !is_write;
    }
    return false;
}

bool mpu_check(const mpu_t* m, addr_t a, u32 size, bool is_write, bool is_priv) {
    if (!(m->ctrl & 1u)) return true; /* MPU disabled */
    /* Highest-numbered enabled overlapping region wins. */
    int hit = -1;
    for (int i = MPU_REGIONS - 1; i >= 0; --i) {
        const mpu_region_t* r = &m->r[i];
        if (!r->enabled) continue;
        u32 rsz = region_size_bytes(r->rasr);
        if (rsz == 0) continue;
        if (a >= r->base && a + size <= r->base + rsz) { hit = i; break; }
    }
    if (hit < 0) {
        /* No region matches: PRIVDEFENA controls fallback for privileged */
        if ((m->ctrl & 4u) && is_priv) return true;
        return false;
    }
    u32 ap = (m->r[hit].rasr >> 24) & 7;
    return ap_permits(ap, is_write, is_priv);
}

static u32 mpu_read(void* ctx, addr_t off, u32 size) {
    (void)size;
    mpu_t* m = (mpu_t*)ctx;
    switch (off) {
        case 0x00: return (u32)MPU_REGIONS << 8;          /* TYPE */
        case 0x04: return m->ctrl;
        case 0x08: return m->rnr;
        case 0x0C: return m->r[m->rnr & 7].base | (m->rnr & 7);
        case 0x10: return m->r[m->rnr & 7].rasr | (m->r[m->rnr & 7].enabled ? 1 : 0);
        default: return 0;
    }
}

static void mpu_write(void* ctx, addr_t off, u32 val, u32 size) {
    (void)size;
    mpu_t* m = (mpu_t*)ctx;
    switch (off) {
        case 0x04: m->ctrl = val & 7u; break;
        case 0x08: m->rnr = val & 7u; break;
        case 0x0C: {
            u32 idx = (val & 0x10u) ? (val & 0xFu) : (m->rnr & 7);
            m->r[idx & 7].base = val & 0xFFFFFFE0u;
            if (val & 0x10u) m->rnr = idx & 7;
            break;
        }
        case 0x10: {
            u32 idx = m->rnr & 7;
            m->r[idx].rasr = val & 0x1FFFFFFEu;
            m->r[idx].enabled = (val & 1u) != 0;
            break;
        }
    }
}

int mpu_attach(bus_t* b, mpu_t* m) {
    *m = (mpu_t){0};
    return bus_add_mmio(b, "mpu", MPU_BASE, MPU_SIZE, m, mpu_read, mpu_write);
}
