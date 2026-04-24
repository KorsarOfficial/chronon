/* Mini-RTOS: two tasks, preemptively scheduled by SysTick+PendSV.
   Each task increments its own counter. After we observe enough context
   switches, report counters and halt. */

#define STK_CSR (*(volatile unsigned*)0xE000E010)
#define STK_RVR (*(volatile unsigned*)0xE000E014)
#define STK_CVR (*(volatile unsigned*)0xE000E018)

typedef struct task_s {
    unsigned sp;        /* current PSP (top of saved frame) */
    unsigned stack[128];/* per-task stack */
} task_t;

task_t task0, task1;
task_t* current_task;
task_t* next_task;

volatile unsigned counter0 = 0;
volatile unsigned counter1 = 0;

/* Exception-frame layout (LIFO indices from top of stack):
   [-1] xPSR      <- top
   [-2] PC
   [-3] LR
   [-4] R12
   [-5] R3
   [-6] R2
   [-7] R1
   [-8] R0
   Below that we push R4-R11 (8 more words) in PendSV. */

static void task_init(task_t* t, void (*entry)(void)) {
    unsigned* sp = &t->stack[128];    /* full-descending */
    /* Initial exception frame */
    *(--sp) = 0x01000000u;             /* xPSR: Thumb bit */
    *(--sp) = (unsigned)entry | 1u;   /* PC */
    *(--sp) = 0xFFFFFFFDu;             /* LR (unused) */
    *(--sp) = 12;                      /* R12 */
    *(--sp) = 3;                       /* R3  */
    *(--sp) = 2;                       /* R2  */
    *(--sp) = 1;                       /* R1  */
    *(--sp) = 0;                       /* R0  */
    /* R4..R11 pre-pushed below (values don't matter) */
    for (int i = 0; i < 8; ++i) *(--sp) = 0xDEAD0000u | i;
    t->sp = (unsigned)sp;
}

#define UART_DR (*(volatile unsigned*)0x40004000)

static void task0_entry(void) {
    for (;;) {
        counter0++;
        if ((counter0 & 0x1F) == 0) UART_DR = 'A';
    }
}

static void task1_entry(void) {
    for (;;) {
        counter1++;
        if ((counter1 & 0x1F) == 0) UART_DR = 'B';
    }
}

/* Watchdog task via SysTick counter: halt after N ticks. */
volatile unsigned systick_count = 0;

int main(void) {
    task_init(&task0, task0_entry);
    task_init(&task1, task1_entry);

    current_task = &task0;
    next_task    = &task1;

    STK_CVR = 0;
    STK_RVR = 500;
    STK_CSR = 0x7;

    /* Kick off first task via SVC. */
    __asm__ volatile ("svc #0");

    /* Never reach here — scheduler loop runs tasks forever.
       For the test: we want tasks to accumulate counts. But they loop
       forever, so this is unreachable. The check happens through a
       separate "watchdog" — let SysTick itself halt after N ticks. */
    while (1) { }
    return 0;
}
