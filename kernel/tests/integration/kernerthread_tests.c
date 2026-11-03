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
static volatile uint32_t g_thread_start_tick, g_thread_stop_tick;
static semaphore_t g_post_sem;

/* ------ thread startup routine ------ */
static int run_test_thread1(void *arg) {
    (void)arg;
    g_thread_start_tick = ktime_ticks();

    while (!kthread_should_stop()) {
        proc_schedule_timeout(1);
    }

    g_thread_stop_tick = ktime_ticks();
    return TEST1_EXPECTED_CODE;
}

static int run_test_thread2(void *arg) {
    (void)arg;

    g_thread_start_tick = ktime_ticks();
    g_thread_stop_tick = ktime_ticks();

    return TEST2_EXPECTED_CODE;
}

static int run_test_thread3(void *arg) {
    (void)arg;
    g_thread_start_tick = ktime_ticks();

    while (1) {
        /* semaphore init by test process and running thread waits on it. */
        sem_wait_timeout(&g_post_sem, 5);

        /* if the thread is already marked for stopping, then exit */
        if (kthread_should_stop()) {
            break;
        }
    }

    g_thread_stop_tick = ktime_ticks();
    return TEST3_EXPECTED_CODE;
}

/* ------ test routines ------ */
static int test_stop_running_thread() {
    /* create and run a kernel thread */
    pcb_t *p = kthread_run(run_test_thread1, NULL, "kthread1");

    /* sleep the test processs for 10 ticks */
    proc_schedule_timeout(10);

    /* stop the kernel thread */
    return kthread_stop(p);
}

static int test_stop_completed_thread() {
    /* create and run a kernel thread */
    pcb_t *p = kthread_run(run_test_thread2, NULL, "kthread2");

    /* sleep the test processs for 5 ticks */
    proc_schedule_timeout(5);

    /* stop the kernel thread */
    return kthread_stop(p);
}

static int test_stop_waiting_thread() {
    /* initialize a semaphore the thread will be waiting on */
    sem_init(&g_post_sem, 0);

    /* create and run a kernel thread */
    pcb_t *p = kthread_run(run_test_thread3, NULL, "kthread3");

    /* sleep the test processs for 5 ticks */
    proc_schedule_timeout(5);

    /* stop the kernel thread */
    return kthread_stop(p);
}

/* ------ testcases ------ */
void create_kthread_test_process(void) {
    t_pass = t_fail = 0;
    uint32_t test_result, lat;

    /* Test1 : Stop a thread running in loop.
     * @kthread_stop should mark the running thread as stopping and the thread
     * should gracefully handle it and return the desired code.
     * Exit -> @run_test_thread -> result -> kthread_trampoline -> result */
    g_thread_start_tick = g_thread_stop_tick = 0;
    test_result = test_stop_running_thread();
    lat = g_thread_stop_tick - g_thread_start_tick;
    TCHECK(lat > 0, "Running Thread to run for desired time");
    TCHECK(test_result == TEST1_EXPECTED_CODE,
           "Running Thread stop return expected result");

    /* Test2: Stopping a thread which has already returned (completed execution)
     * by itself. */
    g_thread_start_tick = g_thread_stop_tick = 0;
    test_result = test_stop_completed_thread();
    lat = g_thread_stop_tick - g_thread_start_tick;
    TCHECK(lat == 0, "Stopped Thread to run for desired time");
    TCHECK(test_result == TEST2_EXPECTED_CODE,
           "Stopped Thread stopped returned expected result");

    /* Test3: Stop the thread waiting on a semaphore.
     * @kthread_stop would wake wake the waiting thread mark it as stopped
     * Thread should gracefully exit with expectedd results */
    g_thread_start_tick = g_thread_stop_tick = 0;
    test_result = test_stop_waiting_thread();
    lat = g_thread_stop_tick - g_thread_start_tick;
    KLOG_VERBOSE("KTHREAD_TEST", "Waiting thread runs ticks: %d\n", lat);
    TCHECK(lat < 20, "Waiting Thread to stop in expected time");
    TCHECK(test_result == TEST3_EXPECTED_CODE, "Waiting Thread stop to retuen "
                                               "expected code");

    KLOG_INFO("KTHREAD_TEST", "kernel thread test: passed: %d, failed: %d.\n",
              t_pass, t_fail);
}
