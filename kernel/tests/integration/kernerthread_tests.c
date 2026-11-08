#include "core/panik.h"
#include "lib/printk.h"
#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/semaphore.h"
#include "stddef.h"
#include "stdint.h"
#include "tests/proc_tests.h"
#include "time/ktimer.h"

static int t_pass, t_fail;

#define TEST1_EXPECTED_CODE 42
#define TEST2_EXPECTED_CODE 7
#define TEST3_EXPECTED_CODE 15
/* Hang detection, not a scheduler latency guarantee. Allow verbose logging
 * and other runnable processes to delay both the worker and its joiner. */
#define TEST_WATCHDOG_TICKS msecs_to_ticks(10000)
#define SEM_WAIT_TICKS msecs_to_ticks(1000)
#define TCHECK(cond, msg)                                                      \
    do {                                                                       \
        if (cond) {                                                            \
            t_pass++;                                                          \
            KLOG_INFO("THREAD_TEST", "[PASS] %s.\n", msg);                     \
        } else {                                                               \
            t_fail++;                                                          \
            KLOG_ERROR("THREAD_TEST", "[FAIL] %s.\n", msg);                    \
        }                                                                      \
    } while (0)

/* ------ thread state ------ */
static semaphore_t g_started;
static semaphore_t g_post_sem;
static const char *g_wait_phase;

static void test_watchdog(void *arg) {
    const char *name = arg;
    KLOG_ERROR("THREAD_TEST", "[FAIL] %s timed out during %s\n", name,
               g_wait_phase);
    /* A stuck join cannot be safely abandoned: the worker may still use the
     * shared fixture. Stop the test run rather than start another case. */
    panik("kernel thread test watchdog: %s (%s)", name, g_wait_phase);
}

static pcb_t *start_test_thread(kthread_fn_t fn, const char *name,
                               ktimer_t *watchdog) {
    sem_init(&g_started, 0);
    g_wait_phase = "startup";
    ktimer_init(watchdog, test_watchdog, (void *)name);
    ktimer_add(watchdog, TEST_WATCHDOG_TICKS);

    pcb_t *p = kthread_run(fn, NULL, name);
    TCHECK(p != NULL, "Kernel thread created");
    if (!p) {
        ktimer_cancel(watchdog);
        return NULL;
    }

    /* Do not request stop before the worker has entered its routine. */
    sem_wait(&g_started);
    return p;
}

static void wait_for_state(pcb_t *p, proc_state_t state, void *channel) {
    for (;;) {
        /* These tests run on a single CPU. Snapshot scheduler-owned fields
         * with interrupts masked; never log or sleep inside the guard.
         * kthread_create pins the PCB until kthread_stop releases it. */
        irq_flags_t flags = irq_save();
        bool reached = p->state == state &&
                       (state != PROC_WAITING ||
                        p->wait_info.wait_channel == channel);
        irq_restore(flags);
        if (reached) {
            return;
        }
        proc_schedule_timeout(1);
    }
}

static int stop_test_thread(pcb_t *p, ktimer_t *watchdog) {
    g_wait_phase = "stop/join";
    /* Give stop/join its own watchdog budget, independent of startup. */
    ktimer_add(watchdog, TEST_WATCHDOG_TICKS);
    uint32_t requested = ktime_ticks();
    int result = kthread_stop(p);
    uint32_t elapsed = ktime_ticks() - requested;
    ktimer_cancel(watchdog);
    /* p may be reclaimed after kthread_stop: do not dereference it here. */
    KLOG_INFO("KTHREAD_TEST", "Stop/join completed in %u ticks\n", elapsed);
    return result;
}

/* ------ thread startup routine ------ */
static int run_test_thread1(void *arg) {
    (void)arg;
    sem_post(&g_started);

    while (!kthread_should_stop()) {
        proc_schedule_timeout(1);
    }

    return TEST1_EXPECTED_CODE;
}

static int run_test_thread2(void *arg) {
    (void)arg;

    sem_post(&g_started);
    return TEST2_EXPECTED_CODE;
}

static int run_test_thread3(void *arg) {
    (void)arg;
    sem_post(&g_started);

    while (1) {
        /* A wakeup alone does not cancel sem_wait_timeout. It may re-park
         * until this deadline before the worker can check should_stop. */
        sem_wait_timeout(&g_post_sem, SEM_WAIT_TICKS);

        if (kthread_should_stop()) {
            break;
        }
    }

    return TEST3_EXPECTED_CODE;
}

/* ------ test routines ------ */
static void test_stop_running_thread(void) {
    ktimer_t watchdog;
    pcb_t *p = start_test_thread(run_test_thread1, "kthread1", &watchdog);
    if (!p) {
        return;
    }
    KLOG_INFO("KTHREAD_TEST", "Looping thread entered its routine before stop\n");
    int result = stop_test_thread(p, &watchdog);
    TCHECK(result == TEST1_EXPECTED_CODE,
           "Running thread stopped with expected result");
}

static void test_stop_completed_thread(void) {
    ktimer_t watchdog;
    pcb_t *p = start_test_thread(run_test_thread2, "kthread2", &watchdog);
    if (!p) {
        return;
    }
    g_wait_phase = "natural exit";
    wait_for_state(p, PROC_TERMINATED, NULL);
    KLOG_INFO("KTHREAD_TEST", "Thread terminated before stop was requested\n");
    int result = stop_test_thread(p, &watchdog);
    TCHECK(result == TEST2_EXPECTED_CODE,
           "Completed thread preserved its result");
}

static void test_stop_waiting_thread(void) {
    /* initialize a semaphore the thread will be waiting on */
    sem_init(&g_post_sem, 0);

    ktimer_t watchdog;
    pcb_t *p = start_test_thread(run_test_thread3, "kthread3", &watchdog);
    if (!p) {
        return;
    }
    g_wait_phase = "semaphore wait";
    wait_for_state(p, PROC_WAITING, &g_post_sem);
    /* Avoid logging between observing the wait and requesting stop. The
     * timed semaphore can still expire; this is cooperative stop coverage,
     * not a claim that semaphore waits are interruptible by stop requests. */
    int result = stop_test_thread(p, &watchdog);
    KLOG_INFO("KTHREAD_TEST",
              "Thread observed waiting on the test semaphore before stop\n");
    TCHECK(result == TEST3_EXPECTED_CODE,
           "Waiting thread stopped with expected result");
}

/* ------ testcases ------ */
void create_kthread_test_process(void) {
    t_pass = t_fail = 0;
    test_stop_running_thread();
    test_stop_completed_thread();
    test_stop_waiting_thread();

    KLOG_INFO("KTHREAD_TEST", "kernel thread test: passed: %d, failed: %d.\n",
              t_pass, t_fail);
}
