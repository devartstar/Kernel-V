#ifndef TEST_SYSCALL_H
#define TEST_SYSCALL_H

#include "lib/printk.h"
#include <stdint.h>

/*
 * Syscall test suite entry points.
 *
 * Definitions live in kernel/tests/unit/test_syscall.c. Each test returns
 * 1 on success and 0 on failure, mirroring the convention used by the other
 * unit-test modules (see test_fd.h).
 */
uint8_t syscall_table_init_test(void);
uint8_t syscall_dispatch_unregistered_test(void);
uint8_t syscall_dispatch_out_of_range_test(void);
uint8_t syscall_dispatch_marshal_test(void);
uint8_t syscall_write_bad_fd_test(void);

static inline void run_syscall_tests(void) {
    uint8_t failed_count = 0;

    if (syscall_table_init_test() == 0) {
        KLOG_INFO("TEST", "Failed: syscall table init test.\n");
        failed_count++;
    }

    if (syscall_dispatch_unregistered_test() == 0) {
        KLOG_INFO("TEST", "Failed: syscall unregistered dispatch test.\n");
        failed_count++;
    }

    if (syscall_dispatch_out_of_range_test() == 0) {
        KLOG_INFO("TEST", "Failed: syscall out-of-range dispatch test.\n");
        failed_count++;
    }

    if (syscall_dispatch_marshal_test() == 0) {
        KLOG_INFO("TEST", "Failed: syscall argument marshaling test.\n");
        failed_count++;
    }

    if (syscall_write_bad_fd_test() == 0) {
        KLOG_INFO("TEST", "Failed: syscall write bad-fd test.\n");
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "Syscall tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All syscall tests passed.\n");
    }
}

#endif /* TEST_SYSCALL_H */
