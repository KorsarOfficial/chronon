#include "periph/uart.h"
#include <stdio.h>

static u32 uart_read(void* ctx, addr_t off, u32 size) {
    (void)ctx; (void)size;
    switch (off) {
        case 0x00: return 0;          /* RX: nothing */
        case 0x04: return 0x1;        /* SR: TXE=1 */
        default:   return 0;
    }
}

static void uart_write(void* ctx, addr_t off, u32 val, u32 size) {
    (void)size;
    uart_t* u = (uart_t*)ctx;
    if (off == 0x00) {
        int c = (int)(val & 0xFF);
        if (u && u->sink) u->sink(c);
        else { fputc(c, stdout); fflush(stdout); }
    }
}

int uart_attach(bus_t* b, uart_t* u) {
    return bus_add_mmio(b, "uart", UART_BASE, UART_SIZE, u, uart_read, uart_write);
}
