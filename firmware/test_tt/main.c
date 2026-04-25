#include <stdint.h>

#define UART_DR (*(volatile uint32_t*)0x40011004u)
#define UART_SR (*(volatile uint32_t*)0x40011000u)

static const char tag[] = "MERIDIAN_TT_PASS";

/* volatile: prevent optimizer from eliding loops */
static volatile uint32_t sram_pad[64];

static void uart_putc(char c) {
    UART_DR = (uint32_t)(unsigned char)c;
}

/* fib(n): a=F(n), b=F(n+1) */
static uint32_t fib(uint32_t n) {
    uint32_t a = 0u, b = 1u;
    for (uint32_t i = 0u; i < n; ++i) { uint32_t t = a + b; a = b; b = t; }
    return a;
}

int main(void) {
    for (uint32_t i = 0u; i < (uint32_t)(sizeof tag - 1u); ++i) uart_putc(tag[i]);

    sram_pad[0] = fib(20u);

    /* arithmetic loops: large enough for >50K cycles via JIT hot path */
    uint32_t s = 0u;
    for (uint32_t i = 0u; i < 4096u; ++i) {
        s += i;
        s ^= (i << 1);
        s &= 0xFFFFFFu;
        s -= (i >> 1);
        sram_pad[i & 63u] = s;
    }

    s = 0u;
    for (uint32_t i = 0u; i < 4096u; ++i) {
        s += i * 3u;
        s ^= (i << 2);
        s &= 0xFFFFFFu;
        s -= (i >> 2);
        sram_pad[i & 63u] ^= s;
    }

    /* read SR to exercise UART model */
    (void)UART_SR;

    while (1) { __asm__ volatile ("nop"); }
    return 0;
}
