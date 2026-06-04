#include "arch/x86/pci/pci_driver.h"

uint8_t pci_driver_matches(const pci_driver_t *pci_driver,
                           const pci_device_t *pci_device) {
    /* check for valid reference to device and priver obect */
    if (!pci_driver || !pci_device || !pci_device->record) {
        KLOG_ERROR("PCI_DRIVER",
                   "maching driver failed. invalid arguemnts passed.\n");
        return 0;
    }

    KLOG_VERBOSE("PCI_DRIVER",
                 "\ndriver: vendor id = %04x, device id = %04x"
                 "\ndriver: vendor id = %04x, device id = %04x\n",
                 pci_driver->vendor_id, pci_driver->device_id,
                 pci_device->record->id.vendor_id,
                 pci_device->record->id.device_id);

    return (pci_driver->vendor_id == pci_device->record->id.vendor_id) &&
           (pci_driver->device_id == pci_device->record->id.device_id);
}
