#ifndef PCI_DEVICES_H
#define PCI_DEVICES_H

#include "arch/x86/pci/pci_registry.h"
#include "core/device.h"

/**
 * pci_device - PCI device object extending the core device obejct
 * @device - the core generic kernel device object
 * @record - reference to the PCI function record.
 *   The content of this record is owned by PCI registry.
 */
typedef struct pci_device {
    device_t device;
    pci_function_record_t *record;
} pci_device_t;

/**
 * pci_device_make_name - name the device based on the rule - pci:endpoint
 * @buf - buffer where the name will be generated and stored.
 * @bize - max size of the buffer for name
 * @rec - reference to the function record for bdf value in name
 */
void pci_device_make_name(char *buf, uint32_t buf_size,
                          const pci_function_record_t *rec);

uint8_t pci_device_init(pci_device_t *pdevice, pci_function_record_t *rec,
                        device_t *pparent_device);

#endif /* PCI_DEVICES_H */
