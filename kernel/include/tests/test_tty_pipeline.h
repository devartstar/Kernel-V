#ifndef TEST_TTY_PIPELINE
#define TEST_TTY_PIPELINE

#include "lib/printk.h"

uint8_t tty_pipeline_newline(void);
uint8_t tty_pipeline_no_newline(void);
uint8_t tty_pipeline_standalone_newline(void);
uint8_t tty_pipeline_empty_passthrough(void);

static inline void run_tty_pipeline_tests(void) {
    uint8_t failed = 0;

    if (tty_pipeline_newline() == 0) {
        failed++;
    }

    if (tty_pipeline_no_newline() == 0) {
        failed++;
    }

    if (tty_pipeline_standalone_newline() == 0) {
        failed++;
    }

    if (tty_pipeline_empty_passthrough() == 0) {
        failed++;
    }

    if (failed > 0) {
        KLOG_ERROR("TEST", "TTY_PIPELINE failed count %u.\n", failed);
    } else {
        KLOG_INFO("TEST", "All TTY_PIPELINE tests passed.\n");
    }
}

#endif /* TEST_TTY_PIPELINE */
