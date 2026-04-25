/* Zephyr-lite kernel impl. Uses SysTick + PendSV like FreeRTOS. */
#include "zephyr_lite.h"

#define STK_CSR (*(volatile unsigned*)0xE000E010)
#define STK_RVR (*(volatile unsigned*)0xE000E014)
#define STK_CVR (*(volatile unsigned*)0xE000E018)
#define ICSR    (*(volatile unsigned*)0xE000ED04)
#define PENDSVSET (1u << 28)

static k_thread_t* threads[K_MAX_THREADS];
static int n_threads = 0;
k_thread_t* k_current;
k_thread_t* k_next;

void k_init(void) { n_threads = 0; }

int k_thread_create(k_thread_t* t, uint32_t* stack, uint32_t stack_size,
                    k_thread_entry_t entry, void* arg, uint8_t prio) {
    if (n_threads >= K_MAX_THREADS) return -1;
    uint32_t* sp = stack + (stack_size / 4);
    /* Initial exception frame */
    *(--sp) = 0x01000000u;            /* xPSR */
    *(--sp) = (uint32_t)entry | 1u;    /* PC */
    *(--sp) = 0xFFFFFFFDu;             /* LR */
    *(--sp) = 12;                      /* R12 */
    *(--sp) = 3;                       /* R3 */
    *(--sp) = 2;                       /* R2 */
    *(--sp) = 1;                       /* R1 */
    *(--sp) = (uint32_t)arg;           /* R0 = arg */
    /* R4-R11 (placeholders) */
    for (int i = 0; i < 8; ++i) *(--sp) = 0;
    t->sp = sp;
    t->stack_lo = (uint32_t)stack;
    t->prio = prio;
    t->state = 0;
    t->delay = 0;
    t->blocked_on = 0;
    threads[n_threads++] = t;
    return 0;
}

static k_thread_t* pick_next(k_thread_t* exclude) {
    /* Round-robin among ready threads (state==0). */
    int start = 0;
    for (int i = 0; i < n_threads; ++i) if (threads[i] == exclude) start = i + 1;
    for (int j = 0; j < n_threads; ++j) {
        int idx = (start + j) % n_threads;
        if (threads[idx]->state == 0) return threads[idx];
    }
    /* Idle: re-run current if it is ready, else first ready. */
    for (int i = 0; i < n_threads; ++i) if (threads[i]->state == 0) return threads[i];
    return threads[0];
}

static void schedule(void) {
    k_next = pick_next(k_current);
    if (k_next != k_current) ICSR = PENDSVSET;
}

void k_yield(void) { schedule(); __asm__ volatile ("dsb \n isb"); }

void k_sleep(uint32_t ticks) {
    k_current->delay = ticks;
    k_current->state = 1;
    schedule();
    while (k_current->state == 1) { __asm__ volatile ("wfi"); }
}

void k_sem_init(k_sem_t* s, int initial, int max) {
    s->count = initial; s->max = max;
}

int k_sem_take(k_sem_t* s) {
    while (1) {
        __asm__ volatile ("cpsid i");
        if (s->count > 0) { s->count--; __asm__ volatile ("cpsie i"); return 0; }
        k_current->state = 2;
        k_current->blocked_on = s;
        __asm__ volatile ("cpsie i");
        schedule();
        while (k_current->state == 2) { __asm__ volatile ("wfi"); }
    }
}

void k_sem_give(k_sem_t* s) {
    __asm__ volatile ("cpsid i");
    if (s->count < s->max) s->count++;
    /* Wake one waiter */
    for (int i = 0; i < n_threads; ++i) {
        if (threads[i]->state == 2 && threads[i]->blocked_on == s) {
            threads[i]->state = 0;
            threads[i]->blocked_on = 0;
            break;
        }
    }
    __asm__ volatile ("cpsie i");
    schedule();
}

/* SysTick handler — implemented in startup as alias to systick_tick_c */
void systick_tick_c(void) {
    for (int i = 0; i < n_threads; ++i) {
        k_thread_t* t = threads[i];
        if (t->state == 1) {
            if (t->delay > 0) t->delay--;
            if (t->delay == 0) t->state = 0;
        }
    }
    schedule();
}

void k_start(void) {
    STK_CVR = 0;
    STK_RVR = 200;
    STK_CSR = 0x7;
    k_current = threads[0];
    k_next    = threads[0];
    __asm__ volatile ("svc #0");
    while (1) {}
}
