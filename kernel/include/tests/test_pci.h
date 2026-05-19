#ifndef TEST_PCI_H
#define TEST_PCI_H

#include "arch/x86/pci/pci.h"
#include "arch/x86/pci/pci_cfg.h"
#include "arch/x86/pci/pci_dump.h"
#include "arch/x86/pci/pci_registry.h"

uint8_t pci_cfg_smoke_test(void);
uint8_t pci_cfg_extract_test(void);
uint8_t pci_cfg_decode_test(void);
uint8_t pci_probe_function_test(void);
uint8_t pci_probe_slot_test(void);
uint8_t pci_scan_bus0_test(void);
uint8_t pci_registry_bus0_test(void);
uint8_t pci_dump_registry_test(void);
uint8_t pci_basic_validation(void);

static inline void run_pci_tests(void) {
    uint8_t failed_count = 0;

    if (pci_cfg_smoke_test()) {
        KLOG_INFO("TEST", "PCI Config snoke test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_cfg_extract_test()) {
        KLOG_INFO("TEST", "PCI Config extract test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_cfg_decode_test()) {
        KLOG_INFO("TEST", "PCI Config decode test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_probe_function_test()) {
        KLOG_INFO("TEST", "PCI Config probe function test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_probe_slot_test()) {
        KLOG_INFO("TEST", "PCI Config probe slot test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_scan_bus0_test()) {
        KLOG_INFO("TEST", "PCI Config scan bus0 test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_registry_bus0_test()) {
        KLOG_INFO("TEST", "PCI Config reister bus0 test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_dump_registry_test()) {
        KLOG_INFO("TEST", "PCI Dump reistery of bus0 test passed.\n");
    } else {
        failed_count++;
    }

    if (pci_basic_validation()) {
        KLOG_INFO("TEST", "PCI Basic Validation test passed.\n");
    } else {
        failed_count++;
    }

    if (failed_count > 0) {
        KLOG_ERROR("TEST", "PCI tests failed count %u.\n", failed_count);
    } else {
        KLOG_INFO("TEST", "All PCI tests passed.\n");
    }
}

#endif TEST_PCI_H
