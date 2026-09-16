#ifndef MUTEX_H
#define MUTEX_H

#include "proc/semaphore.h"

/* Forward decleration - avoids pulling all of proc.h into this header */
struct pcb;

/**
 * Mutual exclusion lock: a binary semaphore plus the owner identity.
 * Ownership lets us detect misuse (eg. unlock by non-owner, recursive self-lock
 * which a bear semaphore silently allows.
 *
 * @sem - underlying bear semaphore (1=unlocked, 0=locked)
 * @owner - process holding the lock or NULL when free
 */
typedef struct mutex {
    semaphore_t sem;
    struct pcb *owner;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */
