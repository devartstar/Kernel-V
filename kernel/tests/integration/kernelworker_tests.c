#include "lib/printk.h"
#include "proc/proc.h"
#include "proc/semaphore.h"
#include "proc/workqueue.h"
#include "tests/proc_tests.h"

/* shared state across the tests */
static volatile int work_ran_count;
static volatile int work_order[8];
static volatile int work_order_idx;
static volatile semaphore_t work_done_sem;

/* A work item: records that it ran in what order, then (on last one) signals
 * the test. work item runs in kworker thread context - interrupts enabled.
 */
static void record_work(void *arg) {
    int id = (int)(uintptr_t)arg;

    KLOG_VERBOSE("WORKQ_TEST",
                 "[kworker] running work item id = %d (proc=%s)\n", id,
                 current_proc->name);

    if (work_order_idx < 8) {
        work_order[work_order_idx++] = id;
        KLOG_VERBOSE("WORKQ_TEST", "[kworker] picked up work with id = %d.\n",
                     id);
    }
    work_ran_count++;

    /* the 3rd item is the last we scheduler, should signal the wwaiting test */
    if (id == 3) {
        sem_post(&work_done_sem);
    }
}

void create_work_test_process(void) {
    work_ran_count = 0;
    work_order_idx = 0;
    sem_init(&work_done_sem, 0);

    KLOG_INFO("WORKQ_TEST", "scheduling 3 work items from %s.\n",
              current_proc->name);

    /* schedule 3 work items. they must run in kworker thread in order */
    uint8_t ok1 = work_schedule(record_work, (void *)(uintptr_t)1);
    uint8_t ok2 = work_schedule(record_work, (void *)(uintptr_t)2);
    uint8_t ok3 = work_schedule(record_work, (void *)(uintptr_t)3);

    /* check if all of the three are scheduled in the work queue */
    if (!ok1 || !ok2 || !ok3) {
        KLOG_ERROR("WORKQ_TEST", "Failed to sechedule one or more work.\n");
        return;
    }

    /* Block the test until kworker thread has run all the three work.
     * this also proves cross thread handoff. kernel test thread sleeps until
     * worker thread runs and wakes it up. */
    KLOG_VERBOSE("WORKQ_TEST",
                 "Test scheduled, blocking until kworker drains...\n");
    sem_wait(&work_done_sem);

    /* Verification: all three works ran in FIFO order 1 -> 2 -> 3 */
    uint8_t order_ok = (work_ran_count == 3 && work_order[0] == 1 &&
                        work_order[1] == 2 && work_order[2] == 3);

    if (order_ok) {
        KLOG_INFO(
            "WORKQ_TEST",
            "TEST PASSED - deferred work ran in kworker in FIFO order.\n");
    } else {
        KLOG_ERROR("WORKQ_TEST",
                   "TEST FAILED, work ran = %d in order = [%d -> %d -> %d].\n",
                   work_ran_count, work_order[0], work_order[1], work_order[2]);
    }
}
