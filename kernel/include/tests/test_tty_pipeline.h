#ifndef TEST_TTY_PIPELINE_H
#define TEST_TTY_PIPELINE_H

#include "lib/printk.h"
#include <stdint.h>

/* returns the number of FAILED cases (0 == all passed) */
uint32_t tty_run_output_pipeline_cases(void);
uint32_t tty_run_input_pipeline_cases(void);
uint32_t tty_run_signal_pipeline_cases(void);

static inline void run_tty_pipeline_tests(void) {
    uint32_t failed = 0;
    failed += tty_run_output_pipeline_cases();
    failed += tty_run_input_pipeline_cases();
    failed += tty_run_signal_pipeline_cases();

    if (failed) {
        KLOG_ERROR("TEST", "TTY_PIPELINE failed count %u.\n", failed);
    } else {
        KLOG_INFO("TEST", "All TTY_PIPELINE tests passed.\n");
    }
}

#endif /* TEST_TTY_PIPELINE_H */
