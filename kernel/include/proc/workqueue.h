#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <stdbool.h>
#include <stdint.h>

/* A unit of deferred work: a function + arguments.
 * function runs later in the worker kernel thread's context (interrupt
 * ENABLED). It may block, sleep, kmalloc, or take mutex.
 */
typedef void (*work_fn_t)(void *arg);

typedef struct work_item {
    work_fn_t fn;
    void *arg;
} work_item_t;

/* Initialize the global work queue and spaw its worker thread.
 * Initialized during boot.
 */
void workqueue_init(void);

/* Schedule @fn(arg) to run later in the worker thread.
 * IRQ-SAFE and never blocks. returns false when the work queue is full.
 * Interrupt handler will schedule the work - whick will be picked up by worker
 * thread to defer the work off the hot path.
 */
bool work_schedule(work_fn_t fn, void *arg);

/* Observability: number of work items dropped because the work queue is full */
uint32_t workqueue_dropped_count(void);

#endif /* WORKQUEUE_H */
