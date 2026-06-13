#ifndef TEST_FS_H
#define TEST_FS_H

#include "lib/printk.h"

uint8_t fs_vfs_root_init_test(void);

static inline void run_vfs_tests(void) {
    uint8_t failed_count = 0;

    /* intialize the file systems root node object */
    if (fs_vfs_root_init_test()) {
        KLOG_INFO("TEST", "VFS root intialize test passed.\n");
    } else {
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "VFS tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All VFS tests passed.\n");
    }
}

#endif /* TEST_FS_H */
