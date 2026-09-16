#ifndef TEST_DEVICES_H
#define TEST_DEVICES_H

#include "lib/printk.h"
#include "stdint.h"

uint8_t device_pci_driver_match_test(void);
uint8_t device_pci_driver_api_test(void);
uint8_t device_pci_driver_registry_bind_test(void);
uint8_t device_pci_device_driver_test(void);
uint8_t device_pci_device_registry_materialize_test(void);
uint8_t device_pci_device_registry_lookup_test(void);
uint8_t device_pci_driver_match_priority_test(void);
uint8_t device_pci_registry_lifetime_links_test(void);

static inline void run_devices_tests(void) {
    uint8_t failed_count = 0;

    /* test driver <> device attacment criteria : same vendor/device id */
    if (device_pci_driver_match_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver match test passed.\n");
    } else {
        failed_count++;
    }

    /* test apis to retrieve usabel information from device object */
    if (device_pci_driver_api_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver api test passed.\n");
    } else {
        failed_count++;
    }

    /* create record reg and enrich it. Create a device reg from it */
    if (device_pci_device_registry_materialize_test()) {
        KLOG_INFO("TEST",
                  "DEVICE pci device registry materialize test passed.\n");
    } else {
        failed_count++;
    }

    /* create a record reg and enrich it. create a device reg from it. create a
     * driver reg. and add a test driver entry and try to bind with the device
     */
    if (device_pci_driver_registry_bind_test()) {
        KLOG_INFO("TEST", "DEVICE pci driver registry bind test passed.\n");
    } else {
        failed_count++;
    }

    /* create a record reg and enrich it. create a device reg from it. create a
     * driver reg. and add a test driver entry and bind with the device then
     * find a device object from device reg based on bdf endpoint
     */
    if (device_pci_device_driver_test()) {
        KLOG_INFO("TEST", "DEVICE pci device <> driver link test passed.\n");
    } else {
        failed_count++;
    }

    if (device_pci_device_registry_lookup_test()) {
        KLOG_INFO("TEST", "DEVICE pci device registry lookup test passed.\n");
    } else {
        failed_count++;
    }

    if (device_pci_driver_match_priority_test()) {
        KLOG_INFO("TEST",
                  "DEVICE pci device match driver priority test passed.\n");
    } else {
        failed_count++;
    }

    if (device_pci_registry_lifetime_links_test()) {
        KLOG_INFO(
            "TEST",
            "DEVICE pci device and record registry linkage test passed.\n");
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
