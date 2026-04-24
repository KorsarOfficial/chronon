/* Test UART output, division, multiplication.
   Prints "42/7=6 mul=294\n" then halts. */

#define UART_DR  (*(volatile unsigned int*)0x40004000)

static void putch(char c) { UART_DR = (unsigned)c; }

static void puts_(const char* s) {
    while (*s) putch(*s++);
}

static void put_uint(unsigned v) {
    char buf[12]; int n = 0;
    if (!v) { putch('0'); return; }
    while (v) { buf[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putch(buf[n]);
}

int main(void) {
    unsigned a = 42, b = 7;
    unsigned q = a / b;        /* UDIV -> 6 */
    unsigned p = a * b;        /* MUL  -> 294 */

    puts_("div=");  put_uint(q);
    puts_(" mul="); put_uint(p);
    putch('\n');

    __asm__ volatile (
        "mov r0, %0\n"
        "mov r1, %1\n"
        ".short 0xDEFE\n"
        :
        : "r"(q), "r"(p)
        : "r0", "r1"
    );
    return 0;
}
