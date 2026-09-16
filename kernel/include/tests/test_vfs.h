#ifndef TEST_FS_H
#define TEST_FS_H

#include "lib/printk.h"

uint8_t fs_vfs_root_init_test(void);
uint8_t fs_vfs_add_child_test(void);
uint8_t fs_vfs_find_child_test(void);
uint8_t fs_vfs_lookup_absolute_test(void);

static inline void run_vfs_tests(void) {
    uint8_t failed_count = 0;

    /* intialize the file systems root node object */
    if (fs_vfs_root_init_test() == 0) {
        KLOG_INFO("TEST", "Failed: VFS root intialize test.\n");
        failed_count++;
    }

    /* add two node objects runder the root, one file and one dir */
    if (fs_vfs_add_child_test() == 0) {
        KLOG_INFO("TEST", "Failed: VFS add child under root test.\n");
        failed_count++;
    }

    /* add two node objects runder the root and try variations of find */
    if (fs_vfs_find_child_test() == 0) {
        KLOG_INFO("TEST", "Failed: VFS find child test.\n");
        failed_count++;
    }

    /* add file objects in vfs tree and lookup using absolute path */
    if (fs_vfs_lookup_absolute_test() == 0) {
        KLOG_INFO("TEST", "Failed: VFS lookup absolute path test.\n");
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "VFS tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All VFS tests passed.\n");
    }
}

#endif /* TEST_FS_H */
