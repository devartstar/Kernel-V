#ifndef TEST_RAMFS_H
#define TEST_RAMFS_H

#include "lib/printk.h"

uint8_t ramfs_read_test(void);
uint8_t ramfs_write_test(void);
uint8_t ramfs_create_file_test(void);
uint8_t ramfs_initial_vfs_tree_test(void);

static inline void run_ramfs_tests(void) {
    uint8_t failed_count = 0;

    KLOG_VERBOSE("TEST_RAMFS", "read_test Starting ...\n");
    if (ramfs_read_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Read test.\n");
        failed_count++;
    }
    KLOG_VERBOSE("TEST_RAMFS", "read_test Completed ...\n");

    KLOG_VERBOSE("TEST_RAMFS", "write_test Starting ...\n");
    if (ramfs_write_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Write test.\n");
        failed_count++;
    }
    KLOG_VERBOSE("TEST_RAMFS", "write_test Completed ...\n");

    KLOG_VERBOSE("TEST_RAMFS", "create_file_test Starting ...\n");
    if (ramfs_create_file_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Create test.\n");
        failed_count++;
    }
    KLOG_VERBOSE("TEST_RAMFS", "create_file_test Completed ...\n");

    KLOG_VERBOSE("TEST_RAMFS", "initail_vfs_tree_test Starting ...\n");
    if (ramfs_initial_vfs_tree_test() == 0) {
        KLOG_INFO("TEST", "Failed: RAMFS Create Initial VFS Tree test.\n");
        failed_count++;
    }
    KLOG_VERBOSE("TEST_RAMFS", "initail_vfs_tree_test Completed ...\n");

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "RAMFS tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All RAMFS tests passed.\n");
    }
}

#endif /* TEST_RAMFS_H */
