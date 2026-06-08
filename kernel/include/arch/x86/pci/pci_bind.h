#ifndef PCI_BIND_H
#define PCI_BIND_H

#include "arch/x86/pci/pci_device_registry.h"
#include "arch/x86/pci/pci_driver_registry.h"
#include "core/device.h"

typedef enum bind_device_result {
    PCI_BIND_FAILED = 0,
    PCI_BIND_PASSED,
    PCI_BIND_FAILED_NOT_DISCOVERED,
    PCI_BIND_FAILED_PROBE_FAILED,
    PCI_BIND_FAILED_NO_MATCHING_DRIVER,
} bind_device_result_t;

/**
 * pci_bind_device - for a given device lookup the driver registry and based on
 * matching critera (eg. same vendor/device id) assign driver to the device.
 * @reg - Ref. to the registry of drivers.
 * @dev - Ref. to the device object
 *
 * @return pci_device_result_t - result state of attaching driver.
 */
bind_device_result_t pci_bind_device(pci_driver_registry_t *reg,
                                     pci_device_t *dev);

/*
 * pci_probe_and_bind_all - for all devices and for all drivers, bind them all
 * based on matching criteria.
 * @device_reg - Ref. to the registry of device objects.
 * @driver_reg - Ref. to the registry of driver objects.
 *
 * @return - count of number of devices got attached to drivers.
 */
uint8_t pci_probe_and_bind_all(pci_device_registry_t *device_reg,
                               pci_driver_registry_t *driver_reg);

#endif /* PCI_BIND_H */
