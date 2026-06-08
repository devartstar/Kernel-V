#include "arch/x86/pci/pci_bind.h"
#include "lib/string.h"

bind_device_result_t pci_bind_device(pci_driver_registry_t *reg,
                                     pci_device_t *dev) {

    if (!reg || !dev || !dev->record) {
        KLOG_ERROR(
            "PCI_BIND",
            "Failed to bind driver to the device. Invalid arguments passed.\n");
        return PCI_BIND_FAILED;
    }

    /* be can only bind driver to devices which are discovered */
    if (dev->device.state != DEVICE_STATE_DISCOVERED) {
        KLOG_ERROR(
            "PCI_BIND",
            "Failed to bind driver to device %s. Device not initalized.\n",
            dev->device.name);

        return PCI_BIND_FAILED_NOT_DISCOVERED;
    }

    /* iterate thru the registry to assign driver to the device */
    for (uint8_t i = 0; i < reg->count; i++) {
        const pci_driver_t *driver = reg->drivers[i];
        uint8_t probe_res;

        if (!driver) {
            KLOG_VERBOSE("PCI_BIND", "probe failed. Driver ref is invalid.\n");
            continue;
        }

        /* check if the driver detail matches with device detail */
        if (!pci_driver_matches(driver, dev)) {
            KLOG_VERBOSE(
                "PCI_BIND",
                "probe failed. No matching Device to bound to Driver .\n");
            continue;
        }

        probe_res = driver->probe(dev);

        /* Probing failed */
        if (!probe_res) {
            dev->device.state = DEVICE_STATE_PROBE_FAILED;
            dev->device.bound_driver = NULL;
            dev->device.driver_data = NULL;

            KLOG_ERROR(
                "PCI_BIND",
                "probe failed: driver = %s, device = %s, probe result = %d.\n",
                driver->name, dev->device.name, probe_res);
            return PCI_BIND_FAILED_PROBE_FAILED;
        }

        /* Probing success */
        dev->device.state = DEVICE_STATE_BOUND;
        dev->device.bound_driver = driver;

        KLOG_INFO(
            "PCI_BIND",
            "probe success:  driver = %s, device = %s, probe result = %d.\n",
            driver->name, dev->device.name, probe_res);

        return PCI_BIND_PASSED;
    }

    KLOG_ERROR("PCI_BIND", "Failed to find matching driver for device %s.\n",
               dev->device.name);
    return PCI_BIND_FAILED_NO_MATCHING_DRIVER;
}

uint8_t pci_probe_and_bind_all(pci_device_registry_t *device_reg,
                               pci_driver_registry_t *driver_reg) {
    uint16_t bound_count = 0;

    /* check for validity of int input arguments */
    if (!driver_reg || !device_reg) {
        KLOG_ERROR(
            "PCI_BIND",
            "Failed probing & binding all devices. Invalid input arguments.\n");
        return 0;
    }

    /* Iterate thru all the function records in the registry and invoke
     * pci_bind_device - takes input as pci_device_t and pci_driver_t */
    for (uint8_t i = 0; i < device_reg->count; i++) {
        pci_device_t *dev = &device_reg->devices[i];
        bind_device_result_t res;

        /* check if the record entry in device object is vlaid */
        if (!dev->record || !dev->record->present) {
            continue;
        }

        /* device state sould be discovered from when added to registry */
        if (dev->device.state != DEVICE_STATE_DISCOVERED) {
            continue;
        }

        /* bind the device to the matching driver from registry */
        res = pci_bind_device(driver_reg, dev);
        if (res == PCI_BIND_PASSED) {
            bound_count++;
        }
    }

    KLOG_INFO("PCI_BIND",
              "PCI Probe and Bound complete. Bound %u devices with drivers.\n",
              bound_count);
    return bound_count;
}
