#ifndef CORTEX_M_ETH_H
#define CORTEX_M_ETH_H

#include "core/types.h"
#include "core/bus.h"

/* Minimal Ethernet MAC mock at 0x40028000.
   +0x00 CTRL — bit 0 = enable
   +0x04 TX_ADDR — pointer to TX frame buffer in SRAM
   +0x08 TX_LEN  — frame length, write triggers TX
   +0x0C RX_ADDR — pointer to RX buffer
   +0x10 RX_LEN  — number of bytes in latest RX frame (0 = no data)
   +0x14 STATUS  — bit 0 RX_READY, bit 1 TX_DONE

   Loopback policy: a TX'd frame is echoed back into the RX buffer with
   simple swap of MAC/IP/UDP/ICMP fields (so a ping appears to be answered). */

#define ETH_BASE 0x40028000u
#define ETH_SIZE 0x100u

typedef struct eth_s {
    u32 ctrl;
    u32 tx_addr;
    u32 tx_len;
    u32 rx_addr;
    u32 rx_len;
    u32 status;
    struct bus_s* bus;  /* back-pointer for loopback memcpy */
} eth_t;

int eth_attach(struct bus_s* b, eth_t* e);

/* Externally-driven RX entrypoint. Copies up to len bytes from frame[] into
   the firmware-mapped RX buffer at e->rx_addr (via bus_write). Sets rx_len
   and status |= 0x3 (RX_READY|TX_DONE bits). No-op if e->rx_addr == 0 or
   bus is NULL. Used by both record-time host driver (via tt_record_eth_rx)
   and replay-time tt_inject_event. */
void eth_inject_rx(eth_t* e, const u8* frame, u32 len);

#endif
