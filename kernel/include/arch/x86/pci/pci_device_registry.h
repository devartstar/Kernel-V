#ifndef PCI_DEVICE_REGISTRY_H
#define PCI_DEVICE_REGISTRY_H

#include "arch/x86/pci/pci_devices.h"

#define PCI_MAX_DEVICE_OBJECTS PCI_MAX_FUNCTIONS_BUS0

/**
 * pci_device_registry - a registry for device objects initialized.
 * device - array of device objects. Storing data as device_registry stores the
 * ownership of the driver object instead of referencing the ownership from
 * elsewhere.
 * count - number of device objects in the registry.
 */
typedef struct pci_device_registry {
    pci_device_t devices[PCI_MAX_DEVICE_OBJECTS];
    uint8_t count;
} pci_device_registry_t;

/**
 * pci_device_registry_init - initialize the device registry
 * reg - ref. to the registry object
 *
 * @return void
 */
void pci_device_registry_init(pci_device_registry_t *reg);

/**
 * pci_device_registry_add - intialize a device object and add to the device
 * registry. NOTE: the device object still not contain any valid information.
 * @reg - ref. to the device registry to add the device.
 * @rec - ref. to the function record for the device.
 * @parent - ref. to the parent device of the device being added to the
 * registry.
 *
 * @return 1 if adding device to registry is success else 0.
 */
uint8_t pci_device_registry_add(pci_device_registry_t *reg,
                                pci_function_record_t *rec, device_t *parent);

/**
 * pci_device_registry_find_bdf - find a device object from the registry for
 * given bdf.
 * @reg - ref. to the device registry to find the device object.
 * @bdf - device endpoint for which we are finding device object
 *
 * @pci_device_t ref. to the device object found else null.
 */
pci_device_t *pci_device_registry_find_bdf(pci_device_registry_t *reg,
                                           pci_bdf_t bdf);

/**
 * pci_device_registry_materialize_from_record_registry
 */
uint8_t pci_device_registry_materialize_from_record_registry(
    pci_device_registry_t *dev_reg, pci_record_registry_t *rec_reg,
    device_t *parent);

#endif /* PCI_DEVICE_REGISTRY_H */
