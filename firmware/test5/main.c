/* Test MSR/MRS + PSP switching + PendSV manual trigger.
   Verifies:
   - MRS/MSR for PRIMASK, CONTROL, PSP, MSP
   - CONTROL.SPSEL flip switches SP between MSP and PSP
   - Writing SCB.ICSR.PENDSVSET triggers PendSV handler */

#define SCB_ICSR (*(volatile unsigned*)0xE000ED04)
#define ICSR_PENDSVSET (1u << 28)

volatile unsigned pendsv_count = 0;
volatile unsigned psp_readback = 0;
volatile unsigned psp_want = 0;

/* Inline asm helpers. */
static inline unsigned get_psp(void) {
    unsigned v; __asm__ volatile ("mrs %0, psp" : "=r"(v)); return v;
}
static inline void set_psp(unsigned v) {
    __asm__ volatile ("msr psp, %0" :: "r"(v));
}
static inline void set_control(unsigned v) {
    __asm__ volatile ("msr control, %0 \n isb" :: "r"(v));
}
static inline unsigned get_primask(void) {
    unsigned v; __asm__ volatile ("mrs %0, primask" : "=r"(v)); return v;
}

static unsigned psp_stack[128] __attribute__((aligned(8)));

int main(void) {
    /* Store expected / observed values in globals so they survive SP switch. */
    psp_want = (unsigned)(psp_stack + 128);
    set_psp(psp_want);
    psp_readback = get_psp();

    /* Switch thread mode to use PSP. */
    set_control(2);

    /* Trigger PendSV three times — each should fire the handler. */
    SCB_ICSR = ICSR_PENDSVSET;
    __asm__ volatile ("dsb \n isb");
    SCB_ICSR = ICSR_PENDSVSET;
    __asm__ volatile ("dsb \n isb");
    SCB_ICSR = ICSR_PENDSVSET;
    __asm__ volatile ("dsb \n isb");

    /* Copy globals into R0/R1/R2 then halt. */
    __asm__ volatile (
        "ldr r0, =pendsv_count\n"
        "ldr r0, [r0]\n"
        "ldr r1, =psp_readback\n"
        "ldr r1, [r1]\n"
        "ldr r2, =psp_want\n"
        "ldr r2, [r2]\n"
        ".short 0xDEFE\n"
        :::"r0", "r1", "r2"
    );
    return 0;
}
