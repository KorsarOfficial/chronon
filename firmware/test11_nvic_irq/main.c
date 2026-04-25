/* NVIC IRQ test: enable IRQ0 + IRQ1, set both pending via NVIC_ISPR0,
   verify handlers fired in priority order. IRQ0 has priority 0x20 (high),
   IRQ1 priority 0x40 (low) — IRQ0 should run first. */

#define NVIC_ISER0 (*(volatile unsigned*)0xE000E100)
#define NVIC_ISPR0 (*(volatile unsigned*)0xE000E200)
#define NVIC_IPR0  (*(volatile unsigned*)0xE000E300) /* byte-addressable */

volatile unsigned irq0_count = 0;
volatile unsigned irq1_count = 0;

int main(void) {
    /* Set priorities: IRQ0=0x20, IRQ1=0x40 (lower numerical = higher prio) */
    *(volatile unsigned char*)0xE000E300 = 0x20;
    *(volatile unsigned char*)0xE000E301 = 0x40;
    /* Enable IRQ0 and IRQ1 */
    NVIC_ISER0 = (1u << 0) | (1u << 1);
    /* Pend both */
    NVIC_ISPR0 = (1u << 0) | (1u << 1);

    /* Wait briefly for IRQs to fire */
    for (volatile int i = 0; i < 100; ++i) {}

    __asm__ volatile (
        "mov r0, %0\n"
        "mov r1, %1\n"
        ".short 0xDEFE\n"
        :: "r"(irq0_count), "r"(irq1_count)
        : "r0","r1"
    );
    return 0;
}
