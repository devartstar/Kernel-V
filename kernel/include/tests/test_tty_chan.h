#ifndef TEST_TTY_CHAN_H
#define TEST_TTY_CHAN_H

#include "lib/printk.h"

uint8_t tty_chan_basic_test(void);
uint8_t tty_chan_rollback_test(void);
uint8_t tty_chan_full_test(void);
uint8_t tty_chan_wraparound_test(void);
uint8_t tty_chan_partial_read_test(void);

static inline void run_tty_chan_tests(void) {
    uint8_t failed = 0;
    if (tty_chan_basic_test() == 0)
        failed++;
    if (tty_chan_rollback_test() == 0)
        failed++;
    if (tty_chan_full_test() == 0)
        failed++;
    if (tty_chan_wraparound_test() == 0)
        failed++;
    if (tty_chan_partial_read_test() == 0)
        failed++;

    if (failed > 0) {
        KLOG_ERROR("TEST", "TTY_CHAN failed count %u.\n", failed);
    } else {
        KLOG_INFO("TEST", "All TTY_CHAN tests passed.\n");
    }
}

#endif /* TEST_TTY_CHAN_H */
