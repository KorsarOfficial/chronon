/* Zephyr-lite: minimal cooperative kernel with k_sleep / k_thread / k_sem.
   Mirrors the Zephyr RTOS API (k_thread_create, k_sem_take, k_sem_give,
   k_sleep) but stays small enough to fit in this repo. */
#ifndef ZEPHYR_LITE_H
#define ZEPHYR_LITE_H

#include <stdint.h>

#define K_MAX_THREADS 4

typedef struct k_thread {
    uint32_t* sp;          /* saved PSP */
    uint32_t  stack_lo;
    uint32_t  delay;       /* ticks remaining when blocked on k_sleep */
    uint8_t   prio;
    uint8_t   state;       /* 0=ready 1=sleep 2=block_sem */
    void*     blocked_on;  /* k_sem ptr */
} k_thread_t;

typedef struct k_sem {
    int count;
    int max;
} k_sem_t;

typedef void (*k_thread_entry_t)(void*);

void k_init(void);
int  k_thread_create(k_thread_t* t, uint32_t* stack, uint32_t stack_size,
                     k_thread_entry_t entry, void* arg, uint8_t prio);
void k_start(void);
void k_sleep(uint32_t ticks);
void k_yield(void);

void k_sem_init(k_sem_t* s, int initial, int max);
int  k_sem_take(k_sem_t* s);     /* blocks until available */
void k_sem_give(k_sem_t* s);

#endif
