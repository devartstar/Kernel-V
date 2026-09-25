#include "proc/workqueue.h"
#include "arch/x86/irq.h"
#include "proc/proc.h"
#include "proc/semaphore.h"

/*
 * Fixed-size ring, deliberately static (not kmalloc'd). This is the RIGHT tool
 * for a top-half deferral queue, not a workaround:
 *   1. Bounded memory = backpressure. A flood of IRQs drops+counts instead of
 *      growing an unbounded queue into OOM. "Refuse and count" > "grow to
 * death".
 *   2. Work items are fixed-size {fn,arg} - dynamic allocation buys nothing.
 *   3. No alloc-failure path to handle inside an interrupt.
 *   4. Our kmalloc's slow path maps pages (kmem_get_page -> paging_map_page,
 *      which allocates+zeroes a page table). Doing that in IRQ context would be
 *      a disaster for latency/determinism, IRQ-safe or not.
 * Capacity is a power of two so head/tail wrap with a cheap mask.
 */
#define WORKQUEUE_CAPACITY 256u
#define WORKQUEUE_MASK (WORKQUEUE_CAPACITY - 1u)

/* workitem ring */
static work_item_t g_wq_ring[WORKQUEUE_CAPACITY];
static uint32_t g_wq_head;
static uint32_t g_wq_tail;
static uint32_t g_wq_dropped;

/* counting semaphore: one per work item queue, worker sleep ont it. */
static semaphore_t g_wq_sem;

/* Check if slot is available in the work item queue */
static bool wq_is_full(void) {
    return (uint32_t)(g_wq_tail - g_wq_head) >= WORKQUEUE_CAPACITY;
}

/* Schedule a work in the work queue.
 * @return true if scheduled successfully otherwise false
 */
bool work_schedule(work_fn_t fn, void *arg) {
    if (!fn) {
        return false;
    }

    /* START CRITICAL AREA
     * enqueue under IRQ protection. */
    irq_flags_t flags = irq_save();
    if (wq_is_full()) {
        g_wq_dropped++;
        irq_restore(flags);
        return false;
    }

    /* update the slot in the work queue with the work item */
    work_item_t *slot = &g_wq_ring[g_wq_tail & WORKQUEUE_MASK];
    slot->fn = fn;
    slot->arg = arg;
    g_wq_tail++;

    /* STOP CRITICAL AREA */
    irq_restore(flags);

    /* Wake up the worker. One port == one work item. so semaphore count mirrors
     * the number of pending items */
    sem_post(&g_wq_sem);
    return true;
}

uint32_t workqueue_dropped_count() { return g_wq_dropped; }

/* The Worker thread:
 * block the worker until the work exists, then run it with interrupt enabled -
 * so work item may sleep / block etc.
 */
static void workqueue_worker(void *arg) {
    (void)arg;

    for (;;) {
        /* block until atleast one item is available (count == 0 -> sleep) */
        sem_wait(&g_wq_sem);

        /* Pop one item under IRQ protection. Release before running
         * Copy the work item first so we can invoke the work fn outside IRQ. */
        irq_flags_t flags = irq_save();
        work_item_t item = g_wq_ring[g_wq_head & WORKQUEUE_MASK];
        g_wq_head++;
        irq_restore(flags);

        /* Run the work outside the critical section */
        if (item.fn) {
            item.fn(item.arg);
        }
    }
}

/* Workqueue Initialization
 * Initialize the workqueue and create a worker thread
 */
void workqueue_init(void) {
    g_wq_head = 0;
    g_wq_tail = 0;
    g_wq_dropped = 0;

    sem_init(&g_wq_sem, 0);

    pcb_t *worker = proc_create(workqueue_worker, NULL, "kworker");
    if (!worker) {
        panik("workqueue_init: failed to create worker thread.\n");
    }
    proc_set_type(worker, PROC_TYPE_KERNEL);
    proc_mark_ready(worker);
}
