#ifndef TEST_DEVFS_H
#define TEST_DEVFS_H

#include "lib/printk.h"
#include "stdint.h"

uint8_t devfs_create_chardev_test(void);

static inline void run_devfs_tests() {
    uint32_t failed_count = 0;

    if (!devfs_create_chardev_test()) {
        failed_count++;
        KLOG_ERROR("TEST", "Failed: create_chardev_test.\n");
    }

    if (failed_count != 0) {
        KLOG_ERROR("TEST_DEVFS", "devfs test failed.\n");
    }
}

#endif /* TEST_DEVFS_H */
