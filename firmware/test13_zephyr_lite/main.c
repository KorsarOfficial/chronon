/* Zephyr-lite demo: 2 threads + k_sleep round-robin (no sem).
   Each thread increments its counter and yields via k_sleep(1).
   Halts when both counters reach 5. */
#include "zephyr_lite.h"

#define UART_DR (*(volatile unsigned*)0x40004000)
static void putch(char c) { UART_DR = (unsigned)c; }

static volatile unsigned ca = 0;
static volatile unsigned cb = 0;

static k_thread_t t_a, t_b;
static uint32_t stack_a[256], stack_b[256];

static void thread_a(void* p) {
    (void)p;
    for (;;) {
        ca++;
        putch('A');
        if (ca >= 5 && cb >= 5) {
            __asm__ volatile (
                "mov r0, %0\n"
                "mov r1, %1\n"
                ".short 0xDEFE\n"
                :: "r"(ca), "r"(cb) : "r0","r1"
            );
        }
        k_sleep(1);
    }
}

static void thread_b(void* p) {
    (void)p;
    for (;;) {
        cb++;
        putch('B');
        k_sleep(1);
    }
}

int main(void) {
    k_init();
    k_thread_create(&t_a, stack_a, sizeof(stack_a), thread_a, 0, 1);
    k_thread_create(&t_b, stack_b, sizeof(stack_b), thread_b, 0, 1);
    k_start();
    return 0;
}
