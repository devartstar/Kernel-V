#include "proc/mutex.h"
#include "core/panik.h"
#include "proc/proc.h"
#include <stddef.h>

void mutex_init(mutex_t *m) {
    m->sem.count = 1;
    m->owner = NULL;
}

void mutex_lock(mutex_t *m) {
    /* Self-deadlock guard: relocking an already held lock would block us
     * forever waiting on ourselved. Mutex is non-recursive by design */
    if (m->owner == current_proc) {
        panik("mutex_lock: recursive lock by owner.\n");
    }

    /* block the current process until lock is free to be acquired */
    sem_wait(&m->sem);

    /* Current process has acquired the lock and is owner of it now */
    m->owner = current_proc;
}

void mutex_unlock(mutex_t *m) {
    /* Only mutex holder can release the lock
     * Releasing the lock from someone else corrupts mutual exclusion property
     */
    if (m->owner != current_proc) {
        panik("mutex_unlock: caller is not lock owner.\n");
    }

    m->owner = NULL;

    /* Releasing the lock will wake up all process waiting to acquire */
    sem_post(&m->sem);
}
