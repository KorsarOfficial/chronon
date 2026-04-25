/* ICMP echo through emulated Ethernet MAC. Builds ping request, sends via
   the eth peripheral, reads back the looped echo reply, validates type=0. */
#include <stdint.h>

#define ETH_TX_ADDR  (*(volatile unsigned*)0x40028004)
#define ETH_TX_LEN   (*(volatile unsigned*)0x40028008)
#define ETH_RX_ADDR  (*(volatile unsigned*)0x4002800C)
#define ETH_RX_LEN   (*(volatile unsigned*)0x40028010)
#define ETH_STATUS   (*(volatile unsigned*)0x40028014)

#define UART_DR (*(volatile unsigned*)0x40004000)
static void putch(char c) { UART_DR = (unsigned)c; }
static void put_u(unsigned v) {
    char b[12]; int n = 0;
    if (!v) { putch('0'); return; }
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putch(b[n]);
}

static uint8_t tx_buf[64];
static volatile uint8_t rx_buf[64];

int main(void) {
    /* Eth dst/src MAC, type=0x0800 (IPv4) */
    static const uint8_t hdr[14] = {
        0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,
        0x11,0x22,0x33,0x44,0x55,0x66,
        0x08, 0x00
    };
    for (int i = 0; i < 14; ++i) tx_buf[i] = hdr[i];
    /* IPv4 header (20 bytes): version=4 IHL=5, total len=28 (IP+ICMP=8) */
    tx_buf[14] = 0x45; tx_buf[15] = 0x00;
    tx_buf[16] = 0x00; tx_buf[17] = 0x1C;
    tx_buf[18] = 0x00; tx_buf[19] = 0x01;
    tx_buf[20] = 0x40; tx_buf[21] = 0x00;
    tx_buf[22] = 0x40; tx_buf[23] = 0x01; /* TTL=64, proto=ICMP */
    tx_buf[24] = 0xB7; tx_buf[25] = 0xA9; /* checksum (not validated by us) */
    /* src 10.0.0.1, dst 10.0.0.2 */
    tx_buf[26] = 10; tx_buf[27] = 0; tx_buf[28] = 0; tx_buf[29] = 1;
    tx_buf[30] = 10; tx_buf[31] = 0; tx_buf[32] = 0; tx_buf[33] = 2;
    /* ICMP echo request: type=8, code=0, csum, id, seq */
    tx_buf[34] = 8;  tx_buf[35] = 0;
    tx_buf[36] = 0;  tx_buf[37] = 0;
    tx_buf[38] = 0;  tx_buf[39] = 0x42;
    tx_buf[40] = 0;  tx_buf[41] = 0x01;

    ETH_RX_ADDR = (unsigned)rx_buf;
    ETH_TX_ADDR = (unsigned)tx_buf;
    ETH_TX_LEN  = 42;

    /* The peripheral synchronously echoes; check ICMP reply type = 0. */
    /* Force reads through volatile to defeat optimizer. */
    unsigned b0 = rx_buf[0];
    unsigned b5 = rx_buf[5];
    unsigned t  = rx_buf[34];
    unsigned ln = ETH_RX_LEN;
    unsigned ok = (b0 == 0x11 && b5 == 0x66 && t == 0 && ln == 42) ? 1 : 0;

    putch('R'); putch('X'); putch('='); put_u(ln);
    putch(' '); putch('T'); putch('='); put_u(t);
    putch(' '); putch('M'); putch('='); put_u(b0);
    putch(' '); putch('O'); putch('K'); putch('='); put_u(ok);
    putch('\n');

    register unsigned r0_v __asm__("r0") = ok;
    register unsigned r1_v __asm__("r1") = ln;
    register unsigned r2_v __asm__("r2") = b0;
    __asm__ volatile (".short 0xDEFE" :: "r"(r0_v), "r"(r1_v), "r"(r2_v));
    return 0;
}
