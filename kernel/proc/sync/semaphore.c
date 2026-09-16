#include "proc/semaphore.h"
#include "arch/x86/interrupt.h"
#include "proc/proc.h"

void sem_init(semaphore_t *s, int32_t initial) { s->count = initial; }

void sem_wait(semaphore_t *s) {
    /* start the critical section */
    irq_flags_t flags = irq_save();

    while (s->count == 0) {
        /* Arm the wait on THIS semaphore's address, while interrupts is off */
        proc_wait_prepare_on(PROC_WAIT_OBJECT, s);

        /* Release the critical section, then yeild to another process.
         * A poster which runs now will find us parked on channel and wake up */
        irq_restore(flags);
        yield();

        /* After this process is woken up - Some other process acquired the lock
         * Reschedule and recheck if current can acquire the lock. */
        flags = irq_save();
    }

    /* process acquired the lock */
    s->count--;
    irq_restore(flags);
}

void sem_post(semaphore_t *s) {
    irq_flags_t flags = irq_save();

    /* add a unit to wake up waiters */
    s->count++;
    /* sem_post` wakes **all** waiters (the ported API only has
     * `proc_wakeup_all_on(channel)`). If N are blocked and one unit arrives,
     * all N wake, one takes it (`count-- → 0`), the other N−1 re-check and
     * re-park. Correct, but O(N) wasted wakeups. */
    proc_wakeup_all_on(s);

    irq_restore(flags);
}
