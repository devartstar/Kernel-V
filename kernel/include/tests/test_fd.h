#ifndef TEST_FD_H
#define TEST_FD_H

#include "fs/fd.h"
#include "lib/printk.h"

uint8_t fd_alloc_free_test(void);
uint8_t fd_get_test(void);
uint8_t fd_close_test(void);
uint8_t fd_reuse_test(void);
uint8_t fd_close_all_test(void);
uint8_t fd_open_path_test(void);
uint8_t fd_read_test(void);
uint8_t fd_write_test(void);
uint8_t fd_seek_test(void);

uint8_t test_setup() {
    /* Initialize file-object allocator pool before exercising fd APIs. */
    vfs_system_init();

    /* seed the VFS root so /hello.txt exists for the lookup below */
    vfs_init();
    if (ramfs_seed_root() != VFS_OK) {
        KLOG_ERROR("FD_TEST",
                   "open_path test failed. failed to seed initial vfs tree.\n");
        return 0;
    }

    return 1;
}

static inline void run_fd_tests(void) {
    uint8_t failed_count = 0;
    uint8_t status;

    status = test_setup();
    if (!status) {
        KLOG_ERROR("TEST", "Skipped: FD test. Setup Failed.\n");
        return;
    }

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

    /* close all file descriptors of an associated process */
    if (fd_close_all_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD close all test.\n");
        failed_count++;
    }

    /* test opening a file using path and ref. to process */
    if (fd_open_path_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD close all test.\n");
        failed_count++;
    }

    /* test reading a file ref. by a process */
    if (fd_read_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD read test.\n");
        failed_count++;
    }

    /* test writing to a file ref. by a process */
    if (fd_write_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD write test.\n");
        failed_count++;
    }

    /* test updating offset of a file ref. by a process */
    if (fd_seek_test() == 0) {
        KLOG_INFO("TEST", "Failed: FD seek test.\n");
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "FD tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All FD tests passed.\n");
    }
}

#endif /* TEST_FS_H */
