#ifndef TEST_TTY_MULTI_H
#define TEST_TTY_MULTI_H

#include "lib/printk.h"
#include <stdint.h>

/* returns the number of FAILED checks (0 == all passed) */
uint32_t tty_run_multi_session_cases(void);

static inline void run_tty_multi_tests(void) {
    uint32_t failed = tty_run_multi_session_cases();

    if (failed) {
        KLOG_ERROR("TEST", "TTY_MULTI failed count %u.\n", failed);
    } else {
        KLOG_INFO("TEST", "All TTY_MULTI tests passed.\n");
    }
}

#endif /* TEST_TTY_MULTI_H */
