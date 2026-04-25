#include "core/tt.h"
#include "core/decoder.h"
#include "core/jit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

tt_t* g_tt          = NULL;
bool  g_replay_mode = false;

void ev_log_init(ev_log_t* lg, u32 cap) {
    lg->buf = (ev_t*)malloc((size_t)cap * sizeof(ev_t));
    lg->n   = 0;
    lg->cap = lg->buf ? cap : 0;
    lg->pos = 0;
}

void ev_log_free(ev_log_t* lg) {
    free(lg->buf);
    lg->buf = NULL;
    lg->n = lg->cap = lg->pos = 0;
}

bool ev_log_append(ev_log_t* lg, u64 cycle, u8 type, u32 payload) {
    if (lg->n >= lg->cap) {
        u32 nc = lg->cap ? lg->cap * 2 : 64;
        if (nc > TT_LOG_MAX) nc = TT_LOG_MAX;
        if (nc <= lg->cap) return false;
        ev_t* nb = (ev_t*)realloc(lg->buf, (size_t)nc * sizeof(ev_t));
        if (!nb) return false;
        lg->buf = nb;
        lg->cap = nc;
    }
    ev_t* e  = &lg->buf[lg->n++];
    e->cycle   = cycle;
    e->type    = type;
    e->pad[0]  = e->pad[1] = e->pad[2] = 0;
    e->payload = payload;
    return true;
}

/* Lower-bound: index of first entry with cycle >= target. */
u32 ev_log_seek(const ev_log_t* lg, u64 cycle) {
    u32 lo = 0, hi = lg->n;
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2;
        if (lg->buf[mid].cycle < cycle) lo = mid + 1;
        else                            hi = mid;
    }
    return lo;
}

/* No-op stubs; 13-04 replaces bodies with ev_log_append calls once g_tt is wired. */
void tt_record_irq(u64 cycle, u8 irq) {
    (void)cycle; (void)irq;
}

void tt_record_uart_rx(u64 cycle, u8 byte) {
    (void)cycle; (void)byte;
}

/* ---- Snapshot module (13-02) ---- */

jit_t* g_jit_for_tt = NULL;

u32 snap_xor32(const u8* d, u32 n) {
    u32 a = 0u, i = 0u;
    for (; i + 4u <= n; i += 4u) {
        u32 w; memcpy(&w, d + i, 4u); a ^= w;
    }
    for (; i < n; ++i) a ^= ((u32)d[i]) << ((i & 3u) * 8u);
    return a;
}

bool snap_save(snap_blob_t* b, cpu_t* c, bus_t* bus, tt_periph_t* p) {
    if (!b || !c || !bus || !p) return false;
    region_t* sr = bus_find_flat(bus, SRAM_BASE_ADDR);
    if (!sr || sr->size < SRAM_SIZE) return false;
    b->magic      = SNAP_MAGIC;
    b->version    = SNAP_VERSION;
    b->cycle      = c->cycles;
    b->cpu        = *c;
    b->st         = *p->st;
    b->nvic       = *p->nv;
    b->scb        = *p->scb;
    b->mpu        = *p->mpu;
    b->dwt        = *p->dwt;
    b->stm32      = *p->stm32;
    b->eth_state  = *p->eth;
    b->eth_state.bus = NULL;  /* don't serialize back-pointer */
    b->uart_state = *p->uart;
    b->sram_size  = SRAM_SIZE;
    memcpy(b->sram, sr->buf, SRAM_SIZE);
    b->checksum   = 0u;
    b->checksum   = snap_xor32((const u8*)b, (u32)(sizeof(*b) - sizeof(u32)));
    return true;
}

bool snap_restore(const snap_blob_t* b, cpu_t* c, bus_t* bus, tt_periph_t* p) {
    if (!b || !c || !bus || !p) return false;
    if (b->magic != SNAP_MAGIC || b->version != SNAP_VERSION) return false;
    if (b->sram_size != SRAM_SIZE) return false;
    snap_blob_t tmp = *b;
    u32 saved = tmp.checksum;
    tmp.checksum = 0u;
    if (snap_xor32((const u8*)&tmp, (u32)(sizeof(tmp) - sizeof(u32))) != saved) return false;
    region_t* sr = bus_find_flat(bus, SRAM_BASE_ADDR);
    if (!sr) return false;
    *c        = b->cpu;
    *p->st    = b->st;
    *p->nv    = b->nvic;
    *p->scb   = b->scb;
    *p->mpu   = b->mpu;
    *p->dwt   = b->dwt;
    *p->stm32 = b->stm32;
    *p->eth   = b->eth_state;
    p->eth->bus = bus;  /* fix back-pointer */
    *p->uart  = b->uart_state;
    memcpy(sr->buf, b->sram, SRAM_SIZE);
    run_dcache_invalidate();
    jit_reset_counters(g_jit_for_tt);
    return true;
}

bool snap_save_to_file(const snap_blob_t* b, const char* path) {
    if (!b || !path) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t n = fwrite(b, 1u, sizeof(*b), f);
    fclose(f);
    return n == sizeof(*b);
}

bool snap_load_from_file(snap_blob_t* b, const char* path) {
    if (!b || !path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(b, 1u, sizeof(*b), f);
    fclose(f);
    if (n != sizeof(*b)) return false;
    if (b->magic != SNAP_MAGIC || b->version != SNAP_VERSION) return false;
    snap_blob_t tmp = *b;
    u32 saved = tmp.checksum;
    tmp.checksum = 0u;
    return snap_xor32((const u8*)&tmp, (u32)(sizeof(tmp) - sizeof(u32))) == saved;
}

/* ---- Replay engine (13-03) ---- */

#include "core/run.h"

void tt_inject_event(cpu_t* c, bus_t* bus, tt_periph_t* p, const ev_t* e) {
    (void)c; (void)bus;
    if (!p || !e) return;
    switch (e->type) {
        case EVENT_UART_RX:
            if (p->uart) uart_inject_rx(p->uart, (u8)(e->payload & 0xFFu));
            break;
        case EVENT_IRQ_INJECT:
            if (p->nv) nvic_set_pending(p->nv, e->payload);
            break;
        case EVENT_ETH_RX:
            break;
        default: break;
    }
}

bool tt_replay(const snap_blob_t* s, const ev_log_t* lg, u64 target,
               cpu_t* c, bus_t* bus, tt_periph_t* p, jit_t* g) {
    if (!s || !c || !bus || !p) return false;
    bool prev = g_replay_mode;
    g_replay_mode = true;
    if (!snap_restore(s, c, bus, p)) { g_replay_mode = prev; return false; }
    if (target < c->cycles) { g_replay_mode = prev; return false; }
    u32 pos = lg ? ev_log_seek(lg, c->cycles) : 0u;
    const ev_t* buf = lg ? lg->buf : NULL;
    u32 n = lg ? lg->n : 0u;
    run_until_cycle(c, bus, target, p->st, p->scb, g, buf, n, &pos, p);
    g_replay_mode = prev;
    return true;
}
