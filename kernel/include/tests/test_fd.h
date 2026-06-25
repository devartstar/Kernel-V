#ifndef TEST_FD_H
#define TEST_FD_H

#include "fs/fd.h"
#include "lib/printk.h"

uint8_t fd_alloc_free_test(void);
uint8_t fd_get_test(void);
uint8_t fd_close_test(void);
uint8_t fd_reuse_test(void);

static inline void run_fd_tests(void) {
    uint8_t failed_count = 0;

    /* Initialize file-object allocator pool before exercising fd APIs. */
    fs_system_init();

    /* attach a file to a process and allocate file descriptor to it */
    if (fd_alloc_free_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD allocate test.\n");
        failed_count++;
    }

    /* attach a file to a process and allocate file descriptor to it */
    if (fd_get_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD allocate test.\n");
        failed_count++;
    }

    /* close an file attached to a process based on file descriptor */
    if (fd_close_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD close test.\n");
        failed_count++;
    }

    /* reuse a freed file descriptor of a process */
    if (fd_reuse_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD close test.\n");
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "FD tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All FD tests passed.\n");
    }
}

#endif /* TEST_FS_H */
