#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>

/**
 * Counting semaphore built on the channel wait primitive
 * The semaphore's OWN ADDRESS is the wait channel. waiters park on it via
 * proc_wait_prepare_on(PROC_WAIT_OBJECT, sem); a poster wakes them with
 * proc_wakeup_all_on(sem). It reuses the wait info (PROC_WAIT_OBJECT).
 *
 * @count - number of available units. Accessed inside irq_save/irq_restore.
 *          count > 0 -> sem_wait proceeds
 *          count = 0 -> blocks
 */
typedef struct semaphore {
    int32_t count;
} semaphore_t;

/**
 * @sem_init - Initialize a semaphore to @initial available units
 *
 * @s - reference to the initialized semaphore
 * @initial - count of units assigned to semaphore s
 */
void sem_init(semaphore_t *s, int32_t initial);

/**
 * @sem_wait: The classic "check condition -> park under same lock"
 * this pattern handles the lost  wakeup race. Interruprs are held across count
 * checks AND the wait-arm (proc_wait_prepare_on), so a poster - even running
 * from an IRQ - cannot slip between "saw count == 0" and "parked on the
 * channel".
 * We loop: on wake we re-check, because wake_all will wake up every waiter on
 * this semaphore s but only some will find a unit.
 */
void sem_wait(semaphore_t *s);

/**
 * @sem_post - add a unit to wake up waiters. Safe from IRQ context
 */
void sem_post(semaphore_t *s);

#endif /* SEMAPHORE_H */
