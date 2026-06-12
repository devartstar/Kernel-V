#include "arch/x86/pci/pci_driver_registry.h"

void pci_driver_registry_init(pci_driver_registry_t *reg) {
    /* check for valid refernce of registry */
    if (!reg) {
        KLOG_ERROR("PCI_DRIVER", "Initializing driver registry failed. Invalid "
                                 "reference to driver registry.\n");
        return;
    }

    /* zero the entire registry block */
    memset(reg, 0, sizeof(*reg));
}

uint8_t pci_driver_registry_add(pci_driver_registry_t *reg,
                                const pci_driver_t *driver) {
    /* check validity of the input args */
    if (!reg || !driver || !driver->probe || !driver->name) {
        KLOG_ERROR("PCI_DRIVER",
                   "Registry addition failed. invalid arguments.\n");

        return 0;
    }

    /* error if maximum number of drivers has already been added */
    if (reg->count >= PCI_MAX_DRIVERS) {
        KLOG_ERROR(
            "PIC_DRIVER",
            "Registry addition failed. Driver registration limit reached.\n");

        return 0;
    }

    /* check if driver already has been registered */
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->drivers[i] == driver) {
            KLOG_ERROR(
                "PCI_DRIVER",
                "Driver registration failed. Driver is already registered.\n");
            return 0;
        }
    }

    /* Register the driver */
    reg->drivers[reg->count++] = driver;
    return 1;
}
