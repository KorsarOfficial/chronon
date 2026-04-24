/* Test NVIC + SysTick: configure SysTick to fire every 50 cycles,
   let 5 ticks accumulate, then read tick_count and halt. */

#define STK_CSR (*(volatile unsigned*)0xE000E010)
#define STK_RVR (*(volatile unsigned*)0xE000E014)
#define STK_CVR (*(volatile unsigned*)0xE000E018)

volatile unsigned tick_count = 0;

int main(void) {
    STK_CVR = 0;       /* clear */
    STK_RVR = 50;      /* reload every 50 cycles */
    STK_CSR = 0x7;     /* ENABLE | TICKINT | CLKSOURCE */

    /* Busy-wait until we see >= 5 ticks. */
    while (tick_count < 5) { }

    unsigned ticks = tick_count;
    __asm__ volatile (
        "mov r0, %0\n"
        ".short 0xDEFE\n"
        :
        : "r"(ticks)
        : "r0"
    );
    return 0;
}
