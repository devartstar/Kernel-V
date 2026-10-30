#include "lib/printk.h"
#include "proc/proc.h"
#include "proc/semaphore.h"
#include "tests/proc_tests.h"
#include "time/ktimer.h"
#include <stdbool.h>
#include <stdint.h>

static int t_pass, t_fail;

#define T_CHECK(cond, msg)                                                     \
    do {                                                                       \
        if (cond) {                                                            \
            t_pass++;                                                          \
            KLOG_INFO("TIMER_TEST", "[PASS] %s.\n", msg);                      \
        } else {                                                               \
            t_fail++;                                                          \
            KLOG_ERROR("TIMER_TEST", "[FAIL] %s.\n", msg);                     \
        }                                                                      \
    } while (0)

/* ------ callback state (written from hard IRQ: keep trivial) ----- */
static volatile uint32_t g_fired_count;
static volatile uint32_t g_fired_tick;

static void record_fire(void *arg) {
    (void)arg;
    g_fired_count++;
    g_fired_tick = ktime_ticks();
}

/* ------ helper thread: sleeps, then posts the semaphore ------ */
static semaphore_t g_post_sem;
static void delayed_poster(void *arg) {
    (void)arg;

    /* process is woken up post 5 ticks */
    proc_schedule_timeout(5);

    sem_post(&g_post_sem);
}

/* ------ testcases ------ */
void create_timer_test_process(void) {
    t_pass = t_fail = 0;
    ktimer_t t;
    uint32_t lat = 0;

    /** Case 1.
     * Fire once on time (deadline is exact, allow 2 ticks slack)
     */
    g_fired_count = 0;
    ktimer_init(&t, record_fire, NULL);

    uint32_t armed_tick = ktime_ticks();
    ktimer_add(&t, 10);

    /* sleep the process for more than timer deadline to make sure timer fires
     * and wakes up the process before scheduler reschedules it. */
    proc_schedule_timeout(20);

    lat = g_fired_tick - armed_tick;
    T_CHECK(g_fired_count == 1, "timer fired exactly once");
    T_CHECK(lat >= 10 && lat <= 12, "timer fired within tolerance of 2 ticks");

    /** Case 2.
     * Cancel before expiry; never fires; second cancel reports non-pending
     */
    g_fired_count = 0;
    ktimer_add(&t, 10);

    T_CHECK(ktimer_cancel(&t) == true, "Cancel of pending timer retuns true");

    /* schedule process to timeout after 20 ticks - ideally the canceled timer
     * supposed to be fired after 10 ticks should not fire again */
    proc_schedule_timeout(20);
    T_CHECK(g_fired_count == 0, "Cancelled timer never fired");
    T_CHECK(ktimer_cancel(&t) == false, "Cancel of idle timer returns false");

    /** Case 3.
     * Re-arm timer. A later deadline is replaced by an earlier one.
     */
    g_fired_count = 0;
    armed_tick = ktime_ticks();

    ktimer_add(&t, 50);
    ktimer_add(&t, 5);

    /* schedule process to timeout after 20 ticks - ideally the timer should be
     * updated with earlier timeout and have fired after 5 ticks */
    proc_schedule_timeout(20);

    lat = g_fired_tick - armed_tick;
    T_CHECK(g_fired_count == 1,
            "re-armed timer fired once (later deadline dropped)");
    T_CHECK(lat >= 5 && lat <= 7, "re-armed timer used the new deadline");

    /** Case 4.
     * Sleep returns 0 (timed out) after at least the required time.
     */
    uint32_t start = ktime_ticks();
    uint32_t rem = proc_schedule_timeout(10);
    T_CHECK(rem == 0 && (ktime_ticks() - start) >= 10,
            "schedule_timeout slept >= 10 ticks");

    /** Case 5.
     * Timed wait on an empty semaphore times out deterministically
     */
    semaphore_t s;
    sem_init(&s, 0);

    start = ktime_ticks();
    bool got = sem_wait_timeout(&s, 10);

    lat = ktime_ticks() - start;
    T_CHECK(!got, "sem_wait_timeout on empty sem returned false");
    T_CHECK(lat >= 10, "sem_wait_timeout never returned early");
    /* Upper bound covers wake-up scheduling + VERBOSE serial logging in
       yield(), which costs ~1 tick per line at 100 Hz. Not timer error. */
    T_CHECK(lat <= 10 + 10, "sem_wait_timeout returned within slack");

    /** Case 6.
     * Try lock (ticks == 0) */
    T_CHECK(!sem_wait_timeout(&s, 0), "try on empty sem fails immediately");
    sem_post(&s);
    T_CHECK(sem_wait_timeout(&s, 0), "try on available sem succeeds");

    /** Case 7.
     * Woken by a post BEFORE the deadline (timeout vs event race)
     */
    sem_init(&g_post_sem, 0);

    pcb_t *poster = proc_create(delayed_poster, NULL, "poster");
    proc_set_type(poster, PROC_TYPE_KERNEL);
    proc_mark_ready(poster);

    start = ktime_ticks();

    /* poster process when scheduled will schedule an wait event of 5 ticks and
     * acquire the semaphore. The event should win
     */
    got = sem_wait_timeout(&g_post_sem, 100);

    lat = ktime_ticks() - start;
    T_CHECK(got, "sem_wait_timeout acquired when posted before deadline");
    T_CHECK(lat < 100, "woken early by post, not by the 100th tick");

    KLOG_INFO("TIMER_TEST", "timer tests: passed: %d, failed: %d.\n", t_pass,
              t_fail);
}
