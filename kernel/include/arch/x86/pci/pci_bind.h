#ifndef PCI_BIND_H
#define PCI_BIND_H

#include "arch/x86/pci/pci_device_registry.h"
#include "arch/x86/pci/pci_driver_registry.h"
#include "core/device.h"

/**
 * bind_device_result - possible results of binding a device
 */
typedef enum bind_device_result {
    /* failed to invoke bind */
    PCI_BIND_FAILED = 0,

    /* successfully bound driver to a device */
    PCI_BIND_PASSED,

    /* failed to bind because the device is not yet discovered
     * a device is marked discovered when it is initialized */
    PCI_BIND_FAILED_NOT_DISCOVERED,

    /* failed to bind device beacuse driver probe routine failed */
    PCI_BIND_FAILED_PROBE_FAILED,

    /* failed to bind device becase no matching driver found to bind */
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

/**
 * pci_bind_device_to_best_driver - for a given device lookup the driver
 * registry. for each driver in the registry get the best matching score among
 * all the rules defined by the driver. compare the best scores from each driver
 * of registry and bind the device with the driver with highest matching score.
 * @reg - Ref. to the registry of drivers.
 * @dev - Ref. to the device object
 *
 * @return pci_device_result_t - result state of attaching driver.
 */
bind_device_result_t pci_bind_device_to_best_driver(pci_driver_registry_t *reg,
                                                    pci_device_t *dev);
/*
 * pci_probe_and_bind_all - for all devices and for all drivers, bind them
 * all based on matching criteria.
 * @device_reg - Ref. to the registry of device objects.
 * @driver_reg - Ref. to the registry of driver objects.
 *
 * @return - count of number of devices got attached to drivers.
 */
uint8_t pci_probe_and_bind_all(pci_device_registry_t *device_reg,
                               pci_driver_registry_t *driver_reg);

#endif /* PCI_BIND_H */
