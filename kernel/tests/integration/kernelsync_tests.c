#include "lib/printk.h"
#include "proc/mutex.h"
#include "proc/proc.h"
#include "proc/semaphore.h"
#include "tests/proc_tests.h"
#include <stddef.h>

static semaphore_t sync_sem;
static mutex_t count_mutex;
static volatile uint32_t shared_counter;
static volatile uint32_t sync_remaining; /* producer + consumer -> one each */

static void sync_finished() { sync_remaining--; }

/** Consumer Process implementation */
static void consumer_proc(void *arg) {
    (void)arg;

    /* --- SEMAPHORE TEST --- */
    /* wait for the producer to release the semaphore */
    KLOG_VERBOSE("SYNC", "[consumer] about to sem_wait (should block).\n");
    sem_wait(&sync_sem);
    KLOG_VERBOSE("SYNC", "[consumer] woken by producer's sem_post.\n");

    /* --- MUTEX TEST --- */
    /* Increment a shared counter by two process under a mutex */
    for (uint8_t i = 0; i < 5; i++) {
        mutex_lock(&count_mutex);
        int seen = shared_counter;

        yield();

        shared_counter = seen + 1;
        mutex_unlock(&count_mutex);
    }

    KLOG_VERBOSE("SYNC", "[consumer] counter now = %d.\n", shared_counter);
    sync_finished();
}

/** Producer Process implementation */
static void producer_proc(void *arg) {
    (void)arg;

    /* --- SEMAPHORE TEST --- */
    /* sleeps a consumer and verify wakeup when semaphore is available */

    /* yields couple to times to ensure that consumer enters the wait state */
    for (uint8_t i = 0; i < 3; i++) {
        KLOG_VERBOSE("SYNC", "[producer] working %u/3 (consumer parked).\n", i);
        yield();
    }

    KLOG_VERBOSE("SYNC", "[producer] sem_post -> wakes up waiting consumer.\n");
    sem_post(&sync_sem);

    /* --- MUTEX TEST --- */
    /* Increment a shared counter by two process under a mutex */
    for (uint8_t i = 0; i < 5; i++) {
        mutex_lock(&count_mutex);
        int seen = shared_counter;

        yield();

        shared_counter = seen + 1;
        mutex_unlock(&count_mutex);
    }

    sync_finished();
}

void create_sync_test_process() {
    /* initialize the syncronization objects */
    sem_init(&sync_sem, 0);
    mutex_init(&count_mutex);

    shared_counter = 0;
    sync_remaining = 2;

    /* create 2 process - consumer FIRST so it runs and blocks before the
     * producer posts. */
    pcb_t *c = proc_create(consumer_proc, NULL, "consumer");
    pcb_t *p = proc_create(producer_proc, NULL, "producer");

    /* set as kernel process */
    proc_set_type(c, PROC_TYPE_KERNEL);
    proc_set_type(p, PROC_TYPE_KERNEL);

    /* mark both process ready to run */
    proc_mark_ready(c);
    proc_mark_ready(p);

    while (sync_remaining > 0) {
        uint32_t eflags;
        __asm__ __volatile__("pushf; pop %0" : "=r"(eflags));
        if (!(eflags & 0x200)) {
            /* interrupts enabled - clear interrupts */
            __asm__ __volatile__("sti");
        }
        __asm__ __volatile__("hlt");
    }

    /* Both producer and consumer should increment shared counter 5 times */
    if (shared_counter == 10) {
        KLOG_INFO("SYNC", "[PASS] mutual exclusion held: counter = 10.\n");
    } else {
        KLOG_ERROR("SYNC", "[FAIL] race! counter = %d (expected = 10).\n",
                   shared_counter);
    }
}
