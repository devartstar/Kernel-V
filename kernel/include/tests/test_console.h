#ifndef TEST_CONSOLE_H
#define TEST_CONSOLE_H

#include "lib/printk.h"

uint8_t console_buffer_input_test(void);

static inline void run_console_tests(void) {
    uint8_t failed_count = 0;

    /* */
    if (console_buffer_input_test() == 0) {
        KLOG_INFO("TEST", "Failed: CONSOLE intialize test.\n");
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "CONSOLE test failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All CONSOLE tests passed.\n");
    }
}

#endif /* TEST_FS_H */
