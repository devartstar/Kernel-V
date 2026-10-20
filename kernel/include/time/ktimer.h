#ifndef KTIMER_H
#define KTIMER_H

#include "time/timer.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * ktimer_fn_t - Timer callback. It runs inside IRQ context ans should be short
 * and non-blocking. Defer in heavy work to the scheduler.
 */
typedef void (*ktimer_fn_t)(void *args);

/**
 * ktimer - structure holding caller owner timer.
 * It is initialized with ktimer_init() before first use.
 * @expires - tick absolute deadline for the timer to expire.
 * @fn - callback for when the timer expires.
 * @args - reference of the argument passed to the timer callback.
 * @next - reference to the next timer object.
 * @prev - reference to the previous timer object.
 * @pending - true when timer is armed but not yet fired.
 */
typedef struct ktimer {
    uint32_t expiry;
    ktimer_fn_t fn;
    void *args;
    struct ktimer *next;
    struct ktimer *prev;
    bool pending;
} ktimer_t;

/* Tick comparision */
#define time_after(a, b) ((int32_t)(a - b) > 0)
#define time_after_eq(a, b) ((int32_t)(a - b) >= 0)

/* Convert milliseconds to ticks count */
#define msecs_to_ticks(ms)                                                     \
    ((uint32_t)(((uint64_t)(ms) * PIT_DEFAULT_HZ + 999u) / 1000))

/* ktime_ticks - Return the number of ticks since boot */
static inline uint32_t ktime_ticks(void) { return tick_count; };

/* ktimer_init - Initialize the ktimer struct with callback and arguments */
void ktimer_init(ktimer_t *k, ktimer_fn_t fn, void *args);

/* ktimer_add - Arm the ktimer to fire after @delay_ticks
 * @delay_ticks - number of timer ticks before the timer triggers.
 * when 0 value is 1. timer never fires syncronously
 */
void ktimer_add(ktimer_t *k, uint32_t delay_ticks);

/* ktimer_cancel - Disarm the ktimer if it was pending. Callback won't run. */
bool ktimer_cancel(ktimer_t *k);

/* ktimer_run_expired - Invoked from timer IRQ on every tick; fire
 * everythingthat is due.
 */
void ktimer_run_expired(uint32_t now);

#endif /* KTIMER_H */
