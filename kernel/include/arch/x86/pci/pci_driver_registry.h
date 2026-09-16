#ifndef PCI_DRIVER_REGISTRY_H
#define PCI_DRIVER_REGISTRY_H

#include "arch/x86/pci/pci_driver.h"

#define PCI_MAX_DRIVERS 16

/**
 * pci_driver_registry - registry structure to hold information about PCI
 * drivers
 * @drivers - array of pointer to structs holding driver info.
 * @count - number of drivers present
 */
typedef struct pci_driver_registry {
    const pci_driver_t *drivers[PCI_MAX_DRIVERS];
    uint32_t count;
} pci_driver_registry_t;

/**
 * pci_driver_registry_init - Initialize the registry for drivers
 * @reg - Ref. to the driver registry
 */
void pci_driver_registry_init(pci_driver_registry_t *reg);

/**
 * pci_driver_registry_add - add a driver to the registry
 * @reg - Ref. to the driver registry to add driver.
 * @driver - Ref. to the driver object to add in registry.
 *
 * @return 1 if driver added to registry successfully else 0
 */
uint8_t pci_driver_registry_add(pci_driver_registry_t *reg,
                                const pci_driver_t *driver);

#endif /* PCI_DRIVER_REGISTRY_H */
