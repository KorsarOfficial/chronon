#ifndef CORTEX_M_UART_H
#define CORTEX_M_UART_H

#include "core/types.h"
#include "core/bus.h"

/* Minimal UART at 0x40004000:
   +0x00 DR   : write = TX byte, read = RX byte (0 if empty)
   +0x04 SR   : bit 0 TXE (always 1), bit 1 RXNE (0 if no input)
   +0x08 CR   : control (ignored for now) */

#define UART_BASE 0x40004000u
#define UART_SIZE 0x1000u

typedef struct uart_s {
    /* Output buffer (stdout by default). */
    int (*sink)(int c);
} uart_t;

int uart_attach(bus_t* b, uart_t* u);

#endif
