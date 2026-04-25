#include "periph/eth.h"
#include <stdio.h>

/* Loopback handler: when firmware writes TX_LEN, copy frame from TX_ADDR
   to RX_ADDR with a simple ICMP echo-reply transformation (swap source
   and destination MAC + IP, set ICMP type 0=reply, recompute checksum). */

static void make_echo_reply(bus_t* b, addr_t src, addr_t dst, u32 len) {
    if (len < 14 + 20 + 8) {
        /* not an Ethernet+IPv4+ICMP frame; just copy */
        for (u32 i = 0; i < len; ++i) {
            u32 v = 0;
            bus_read(b, src + i, 1, &v);
            bus_write(b, dst + i, 1, v);
        }
        return;
    }
    u8 buf[1600];
    u32 n = len < sizeof(buf) ? len : sizeof(buf);
    for (u32 i = 0; i < n; ++i) {
        u32 v = 0; bus_read(b, src + i, 1, &v); buf[i] = (u8)v;
    }
    /* Swap MAC dst/src (0..5 and 6..11) */
    u8 tmp[6];
    for (int i = 0; i < 6; ++i) { tmp[i] = buf[i]; buf[i] = buf[6+i]; buf[6+i] = tmp[i]; }
    /* IPv4 — bytes 14..33. Swap src(26..29) and dst(30..33). */
    for (int i = 0; i < 4; ++i) {
        tmp[i] = buf[26+i]; buf[26+i] = buf[30+i]; buf[30+i] = tmp[i];
    }
    /* ICMP — at offset 14+20=34. Set type=0 (Echo Reply). */
    buf[34] = 0;
    /* Recompute ICMP checksum (bytes 36..n - simple, just zero it). */
    buf[36] = 0; buf[37] = 0;
    u32 sum = 0;
    for (u32 i = 34; i + 1 < n; i += 2) sum += (u32)((buf[i] << 8) | buf[i+1]);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    sum = ~sum & 0xFFFF;
    buf[36] = (u8)(sum >> 8);
    buf[37] = (u8)sum;
    /* Write back to RX */
    for (u32 i = 0; i < n; ++i) bus_write(b, dst + i, 1, buf[i]);
}

static u32 eth_read(void* ctx, addr_t off, u32 size) {
    (void)size;
    eth_t* e = (eth_t*)ctx;
    switch (off) {
        case 0x00: return e->ctrl;
        case 0x04: return e->tx_addr;
        case 0x08: return e->tx_len;
        case 0x0C: return e->rx_addr;
        case 0x10: return e->rx_len;
        case 0x14: return e->status;
    }
    return 0;
}
static void eth_write(void* ctx, addr_t off, u32 v, u32 size) {
    (void)size;
    eth_t* e = (eth_t*)ctx;
    switch (off) {
        case 0x00: e->ctrl = v; break;
        case 0x04: e->tx_addr = v; break;
        case 0x08: e->tx_len = v;
                   if (e->bus && e->rx_addr) {
                       make_echo_reply(e->bus, e->tx_addr, e->rx_addr, v);
                       e->rx_len = v;
                       e->status |= 0x3;
                   }
                   break;
        case 0x0C: e->rx_addr = v; break;
        case 0x10: e->rx_len = v; break;
        case 0x14: e->status &= ~v; break;
    }
}

int eth_attach(bus_t* b, eth_t* e) {
    *e = (eth_t){0};
    e->bus = b;
    return bus_add_mmio(b, "eth", ETH_BASE, ETH_SIZE, e, eth_read, eth_write);
}
