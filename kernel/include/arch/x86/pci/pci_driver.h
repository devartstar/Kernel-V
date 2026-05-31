#ifndef PCI_DRIVER_H
#define PCI_DRIVER_H

#include "arch/x86/pci/pci_devices.h"
#include <stdint.h>

/* pointer to driver probing function */
typedef uint8_t (*pci_driver_probe_fn)(pci_device_t *pci_dev);

/**
 * pci_driver - a pci driver object
 * @name - identifier of driver object, or logging
 * vendor_id - pci device vendor id this driver belongs to
 * device_id - pci device id this driver belongs to
 * probe - function to invoke when this driver is called
 */
typedef struct pci_driver {
    const char *name;
    uint16_t device_id;
    uint16_t vendor_id;
    pci_driver_probe_fn probe;
} pci_driver_t;

/**
 * pci_driver_matches - check if the vendor and device id of the driver matches
 * with that of the device
 * @pci_driver - Ref. to the driver object
 * @pci_device - Ref. to te device object
 *
 * @return 1 if it matches else 0
 */
uint8_t pci_driver_matches(const pci_driver_t *pci_driver,
                           const pci_device_t *pci_device);

#endif /* PCI_DRIVER_H */
