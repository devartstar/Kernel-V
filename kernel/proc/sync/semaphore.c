#include "proc/semaphore.h"
#include "arch/x86/interrupt.h"
#include "proc/proc.h"
#include "time/ktimer.h"

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

bool sem_wait_timeout(semaphore_t *s, uint32_t ticks) {
    /* start the critical section */
    irq_flags_t flags = irq_save();
    uint32_t deadline = ktime_ticks() + ticks;

    while (s->count == 0) {
        /* check if the ticks have expired, exit */
        uint32_t now = ktime_ticks();
        if (!time_after(deadline, now)) {
            /* end the critical section */
            irq_restore(flags);
            return false;
        }

        /* mark the process waiting for desired timeout before retry. */
        proc_wait_prepare_on(PROC_WAIT_OBJECT, s);
        proc_park_timeout(deadline - now, flags);

        /* @proc_park_timeout will end the critical section */
        flags = irq_save();
    }

    s->count--;
    irq_restore(flags);
    return true;
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
