/* FreeRTOS IPC demo: producer task pushes 10 ints to queue, consumer
   sums them and halts when total is correct. Verifies xQueueSend/Receive,
   blocking semantics, and tick-driven scheduling. */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define UART_DR (*(volatile unsigned*)0x40004000)
static void putch(char c) { UART_DR = (unsigned)c; }

static QueueHandle_t q;
static SemaphoreHandle_t sem;
static volatile int sum = 0;
static volatile int produced = 0;

static void producer(void* p) {
    (void)p;
    for (int i = 1; i <= 10; ++i) {
        xQueueSend(q, &i, portMAX_DELAY);
        produced = i;
        putch('p');
    }
    vTaskDelete(NULL);
    for (;;) vTaskDelay(1);
}

static void consumer(void* p) {
    (void)p;
    int v;
    int local_sum = 0;
    for (int i = 0; i < 10; ++i) {
        if (xQueueReceive(q, &v, portMAX_DELAY) == pdTRUE) {
            local_sum += v;
            putch('c');
        }
    }
    sum = local_sum;
    /* Expected: 1+2+...+10 = 55 */
    __asm__ volatile (
        "mov r0, %0\n"
        "mov r1, %1\n"
        ".short 0xDEFE\n"
        :: "r"(local_sum), "r"(produced)
        : "r0","r1"
    );
    for (;;) vTaskDelay(1);
}

void vApplicationMallocFailedHook(void) {
    __asm__ volatile (".short 0xDEFE");
}

int main(void) {
    q = xQueueCreate(4, sizeof(int));
    if (!q) for (;;);
    xTaskCreate(producer, "P", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(consumer, "C", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    vTaskStartScheduler();
    for (;;);
}
