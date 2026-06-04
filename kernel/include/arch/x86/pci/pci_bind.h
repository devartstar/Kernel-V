#ifndef PCI_BIND_H
#define PCI_BIND_H

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
 * pci_bind_state - the state of the driver attached to a function record.
 * @driver - ref. to the driver assigned to the associated device with the
 * record.
 * @driver_data - ref. to the data stored by the driver.
 * @bound - 1 if the device associated with the record is bounded.
 * @probe_failed - 1 if probing the driver failed.
 */
typedef struct pci_bind_state {
    const pci_driver_t *driver;
    void *driver_data;
    uint8_t bound;
    uint8_t probe_failed;
} pci_bind_state_t;

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
 * @record_reg - Ref. to the registry of device function records.
 * @driver_reg - Ref. to the registry of driver.
 *
 * @return - count of number of devices got attached to drivers.
 */
uint8_t pci_probe_and_bind_all(pci_record_registry_t *record_reg,
                               pci_driver_registry_t *driver_reg);

#endif /* PCI_BIND_H */
