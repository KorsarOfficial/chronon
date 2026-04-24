/* Minimal C firmware for Cortex-M3.
   Computes Fibonacci in a loop. After main, value is in r0. */

static int fib(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        int t = a + b;
        a = b;
        b = t;
    }
    return a;
}

int main(void) {
    volatile int result = fib(10); /* fib(10) = 55 */
    /* Halt with UDF so emulator stops; result remains in last reg. */
    __asm__ volatile (
        "mov r0, %0\n"
        ".short 0xDEFE\n"  /* UDF */
        :
        : "r"(result)
        : "r0"
    );
    return 0;
}
