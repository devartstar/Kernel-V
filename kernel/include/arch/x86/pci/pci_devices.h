#ifndef PCI_DEVICES_H
#define PCI_DEVICES_H

#include "arch/x86/pci/pci_record_registry.h"
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

/**
 * pci_device_init - initialize a PCI device object.
 * Note: device object doesnt contain any valid information except device main.
 * @pdevice - reference to he device object to initliaze
 * @rec - reference to pci function recoed.
 * @pparent_device - reference to the parent device object.
 *
 * @return 1 for successful initialization else 0.
 */
uint8_t pci_device_init(pci_device_t *pdevice, pci_function_record_t *rec,
                        device_t *pparent_device);

/**
 * pci_device_set_driver_data - helper to set the ref of the driver data to the
 * device
 * @device - device object in which to set the driver info.
 * @dricer_data - ref. to the driver data to be set.
 */
static inline void pci_device_set_driver_data(pci_device_t *device,
                                              void *driver_data) {
    if (!device) {
        KLOG_ERROR(
            "PCI_DEVICE",
            "Failed ot set the driver data to device. Invalid input args.\n");
        return;
    }

    device->device.driver_data = driver_data;
}

/**
 * pci_device_get_driver_data - helper to get the ref to the driver data of a
 * device.
 * @device - device object of which we need driver data.
 */
static inline void *pci_device_get_driver_data(pci_device_t *device) {
    if (!device) {
        KLOG_ERROR(
            "PCI_DEVICE",
            "Failed ot get the driver data of device. Invalid input args.\n");
        return NULL;
    }

    return device->device.driver_data;
}

static inline void pci_device_state_match(const pci_device_t *device,
                                          void *ctx) {
    uint32_t *state_count = (uint32_t *)ctx;
    state_count[device->device.state]++;
}

#endif /* PCI_DEVICES_H */
