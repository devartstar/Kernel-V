#ifndef TEST_DEVICES_H
#define TEST_DEVICES_H

#include "lib/printk.h"
#include "stdint.h"

uint8_t device_pci_driver_match_test(void);
uint8_t device_pci_driver_registry_bind_test(void);
uint8_t device_pci_driver_api_test(void);
uint8_t device_pci_device_driver_test(void);

static inline void run_devices_tests(void) {
    uint8_t failed_count = 0;

    if (device_pci_driver_match_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver match test passed.\n");
    } else {
        failed_count++;
    }

    if (device_pci_driver_registry_bind_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver registry bind test passed.\n");
    } else {
        failed_count++;
    }

    if (device_pci_driver_api_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver api test passed.\n");
    } else {
        failed_count++;
    }

    if (device_pci_device_driver_test()) {
        KLOG_INFO("TEST", "DEVICE pci device <> driver link test passed.\n");
    } else {
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "Devices tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All Devices tests passed.\n");
    }
}

#endif /* TEST_DEVICES_H */
