#ifndef TEST_RAMFS_H
#define TEST_RAMFS_H

#include "lib/printk.h"

uint8_t ramfs_read_test(void);
uint8_t ramfs_write_test(void);
uint8_t ramfs_create_test(void);

static inline void run_ramfs_tests(void) {
    uint8_t failed_count = 0;

    if (ramfs_read_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Read test.\n");
        failed_count++;
    }

    if (ramfs_write_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Write test.\n");
        failed_count++;
    }

    if (ramfs_create_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Create test.\n");
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "RAMFS tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All RAMFS tests passed.\n");
    }
}

#endif /* TEST_RAMFS_H */
