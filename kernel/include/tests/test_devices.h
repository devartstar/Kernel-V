#ifndef TEST_DEVICES_H
#define TEST_DEVICES_H

#include "lib/printk.h"
#include "stdint.h"

uint8_t device_pci_driver_match_test(void);

static inline void run_devices_tests(void) {
    uint8_t failed_count = 0;

    if (device_pci_driver_match_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver match test passed.\n");
    } else {
        failed_count++;
    }
}

#endif /* TEST_DEVICES_H */
