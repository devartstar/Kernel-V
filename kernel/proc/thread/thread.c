#include "mm/kmalloc.h"
#include "proc/kthread.h"
#include "proc/semaphore.h"

/* Thread Control Block(TCB): outlives the PCB (the reaper frees the PCB not
 * TCB)
 */
typedef struct kthread {
    kthread_fn_t fn;
    void *args;
    volatile bool should_stop;
    int result;
    /* @exited semaphore is posted once the thread routine @fn returns */
    semaphore_t exited;
} kthread_t;

/**
 * @kthread_trampoline is the entry routine for every thread
 */
static void kthread_trampoline(void *k) {
    kthread_t *t = k;

    /* Case: Thread stoped before it could even run
     * Check before invoking the thread startup routine.
     */
    t->result = t->should_stop ? KTHREAD_ERR_NEVER_RAN : t->fn(t->args);
    sem_post(&t->exited);

    /* !IMP! Do not touch thread after this: stopper may kfree it immediately.
     * Resumes from thread_entry_wrapper -> proc_exit
     */
}

pcb_t *kthread_create(kthread_fn_t fn, void *arg, const char *name) {
    kthread_t *k = kmalloc(sizeof(*k));
    if (!k) {
        return NULL;
    }

    /* Initialize the TCB */
    k->fn = fn;
    k->args = arg;
    k->should_stop = false;
    k->result = 0;
    sem_init(&k->exited, 0);

    pcb_t *p = proc_create(kthread_trampoline, k, name);
    if (!p) {
        kfree(k);
        return NULL;
    }

    proc_set_type(p, PROC_TYPE_KERNEL);
    p->kthread = k;

    /* Block reclaiming unless thread stop marks it reclaimable.
     * 1. Thread stopped by itself.
     * 2. Underlying PCB is marked reclaimable.
     * 3. Idle reaper cleans up the PCB.
     * Thread Stop routine might be invoked later has no ref to PCB.
     */
    p->reap_blocked = 1;

    return p;
}

pcb_t *kthread_run(kthread_fn_t fn, void *arg, const char *name) {
    pcb_t *p = kthread_create(fn, arg, name);
    if (p) {
        proc_mark_ready(p);
    }
    return p;
}

bool kthread_should_stop(void) {
    kthread_t *k = current_proc ? current_proc->kthread : NULL;
    return k && k->should_stop;
}

int kthread_stop(pcb_t *p) {
    kthread_t *k = p->kthread;
    if (!k) {
        panik("kthread_stop: not a kernel thread\n");
    }

    k->should_stop = true;

    /* Case1: Thread was marked stoped before it was scheduled to run. */
    if (p->state == PROC_NEW) {
        proc_mark_ready(p);
    }

    /* Case2: Thread was running before it was marked stop */
    proc_wakeup(p);

    sem_wait(&k->exited);

    /* Mark the pcb reclaimable */
    p->reap_blocked = 0;

    int result = k->result;
    kfree(k);

    return result;
}
