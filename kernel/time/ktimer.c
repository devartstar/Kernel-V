#include "time/ktimer.h"
#include "lib/printk.h"
#include <stddef.h>

#define KTIMER_WHEEL_SIZE 256u
#define KTIMER_WHEEL_MASK (KTIMER_WHEEL_SIZE - 1)

/**
 * g_timer_wheel - Hashed timer wheel. bucket = timer & KTIMER_WHEEL_MASK
 * A timer more than KTIMER_WHEEL_SIZE ticks away will be in a separate bucket
 * and is skipped for check if timer has expired.
 * Each check iterates through only one bucker. Add/Cancel in O(1)
 */
static ktimer_t *g_wheel[KTIMER_WHEEL_SIZE];

/**
 * g_next_tick - The next tick for which we have not processed yet.
 * Helps to catchup if ticks are ever processed late
 */
static uint32_t g_next_tick;

/* ============================
 * WHEEL OPERATIONS START
 * ============================ */

static void wheel_insert(ktimer_t *t) {
    /* get the first element in the bucket */
    ktimer_t **head = &g_wheel[t->expiry & KTIMER_WHEEL_MASK];

    /* assign the new timer as the HEAD of the bucket */
    t->prev = NULL;
    t->next = *head;

    /* update the prev link for the previous head */
    if (*head) {
        (*head)->prev = t;
    }

    *head = t;

    /* timer is armed, mark pending as true */
    t->pending = true;
}

static void wheel_unlink(ktimer_t *t) {
    if (t->prev) {
        t->prev->next = t->next;
    } else {
        /* unlinked element is the head of the bucket */
        g_wheel[t->expiry & KTIMER_WHEEL_MASK] = t->next;
    }

    if (t->next) {
        t->next->prev = t->prev;
    }

    /* unlink the element */
    t->prev = t->next = NULL;
    t->pending = true;
}

/* =============================
 * WHEEL OPERATIONS COMPLETE
 * ============================= */

void ktimer_init(ktimer_t *k, ktimer_fn_t fn, void *args) {
    k->fn = fn;
    k->args = args;
    k->expiry = 0;
    k->prev = k->next = NULL;
    k->pending = false;
}

void ktimer_add(ktimer_t *t, uint32_t delay_ticks) {
    /* delay 0 -> 1 because firing from inside ktimer_add would run the
     * callback from callers contextand break the flow for calback to run only
     * from IRQ context */
    if (delay_ticks == 0) {
        delay_ticks = 1;
    }

    /* start critical section */
    uint32_t flags = irq_save();

    if (t->pending) {
        /* disarm the previous timer instance */
        wheel_unlink(t);
    }

    t->expiry = tick_count + delay_ticks;
    wheel_insert(t);

    /* end critical section */
    irq_restore(flags);
}

bool ktimer_cancel(ktimer_t *t) {
    /* start critical section */
    uint32_t flags = irq_save();

    bool was_pending = t->pending;
    if (was_pending) {
        wheel_unlink(t);
    }

    /* end critical section */
    irq_restore(flags);

    /* Single CPU + callbacks running with IRQ off.
     * Once timer is cancelled -> callback will not run */
    return was_pending;
}

static void run_bucket(uint32_t wheel_idx, uint32_t now) {
again:
    if (wheel_idx >= KTIMER_WHEEL_SIZE) {
        KLOG_ERROR("KTIMER", "Invalid bucket index %u for timer loockup.\n",
                   wheel_idx);
        return;
    }
    /* each time we find a expired timer, callback may update the bucket, so we
     * need to rescan the bucket from start. hence the anchor - again: */
    for (ktimer_t *t = g_wheel[wheel_idx]; t; t = t->next) {
        if (time_after_eq(now, t->expiry)) {
            ktimer_fn_t fn = t->fn;
            void *args = t->args;

            /* unlink the timer before invoking callback */
            wheel_unlink(t);

            if (fn) {
                fn(args);
            }

            goto again;
        }
    }
}

void ktimer_run_expired(uint32_t now) {
    /* Running with IRQ off. process all timer which has expired */
    while (time_after_eq(now, g_next_tick)) {
        run_bucket(g_next_tick & KTIMER_WHEEL_MASK, now);
        g_next_tick++;
    }
}
