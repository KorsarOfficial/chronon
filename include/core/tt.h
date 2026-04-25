#ifndef CORTEX_M_TT_H
#define CORTEX_M_TT_H

#include "core/types.h"
#include "core/cpu.h"
#include "core/bus.h"
#include "core/nvic.h"
#include "core/jit.h"
#include "periph/systick.h"
#include "periph/scb.h"
#include "periph/mpu.h"
#include "periph/dwt.h"
#include "periph/stm32.h"
#include "periph/eth.h"
#include "periph/uart.h"

enum {
    EVENT_NONE       = 0,
    EVENT_UART_RX    = 1,
    EVENT_IRQ_INJECT = 2,
    EVENT_ETH_RX     = 3,
};

typedef struct ev_s {
    u64 cycle;
    u8  type;
    u8  pad[3];
    u32 payload;
} ev_t;

_Static_assert(sizeof(ev_t) == 16, "ev_t must be 16B");

#define TT_LOG_MAX 65536u

typedef struct ev_log_s {
    ev_t* buf;   /* malloc-owned, capacity = cap */
    u32   n;     /* used count */
    u32   cap;
    u32   pos;   /* replay cursor */
} ev_log_t;

typedef struct tt_periph_s {
    systick_t* st;
    nvic_t*    nv;
    scb_t*     scb;
    mpu_t*     mpu;
    dwt_t*     dwt;
    stm32_t*   stm32;
    eth_t*     eth;
    uart_t*    uart; /* used by 13-03 tt_inject_event for EVENT_UART_RX routing */
} tt_periph_t;

/* Forward decls; full defs land in 13-02 and 13-04. */
typedef struct snap_blob_s snap_blob_t;
typedef struct tt_s        tt_t;

/* Event log */
void ev_log_init  (ev_log_t* lg, u32 cap);
void ev_log_free  (ev_log_t* lg);
bool ev_log_append(ev_log_t* lg, u64 cycle, u8 type, u32 payload);
u32  ev_log_seek  (const ev_log_t* lg, u64 cycle); /* lower_bound by cycle */

/* Weak record hooks; 13-04 provides strong overrides. */
void tt_record_irq    (u64 cycle, u8 irq);
void tt_record_uart_rx(u64 cycle, u8 byte);

/* Module-global tt pointer used by uart/nvic without threading tt through every call.
   NULL = no recording. Set by tt_create in 13-04. */
extern tt_t* g_tt;
extern bool  g_replay_mode; /* true during tt_replay; suppresses side effects */

#endif
