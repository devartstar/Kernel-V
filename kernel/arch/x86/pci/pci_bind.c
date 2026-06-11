#include "arch/x86/pci/pci_bind.h"
#include "lib/string.h"

/* bind the first matching driver for the device */
bind_device_result_t pci_bind_device(pci_driver_registry_t *reg,
                                     pci_device_t *dev) {

    if (!reg || !dev || !dev->record) {
        KLOG_ERROR(
            "PCI_BIND",
            "Failed to bind driver to the device. Invalid arguments passed."
            "\n\tdriver registry = %p, device = %p.\n",
            reg, dev);
        return PCI_BIND_FAILED;
    }

    /* be can only bind driver to devices which are discovered */
    if (dev->device.state != DEVICE_STATE_DISCOVERED) {
        KLOG_ERROR("PCI_BIND",
                   "bind failed: device %s, state = %s, expected state = "
                   "discovered.\n",
                   dev->device.name, device_state_name(dev->device.state));

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

        /* check if driver matches for the device */
        if (!pci_driver_matches(driver, dev)) {
            KLOG_VERBOSE(
                "PCI_BIND",
                "probe failed. No matching Driver to bound to Device %s.\n",
                dev->device.name);
            continue;
        }

        dev->device.state = DEVICE_STATE_MATCHED;

        if (!driver->probe) {
            dev->device.state = DEVICE_STATE_PROBE_FAILED;
            KLOG_VERBOSE("PCI_BIND",
                         "probe failed. Invalid probe routine of Driver %s.\n",
                         driver->name);
            return PCI_BIND_FAILED_PROBE_FAILED;
        }

        dev->device.state = DEVICE_STATE_PROBING;

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

    dev->device.state = DEVICE_STATE_UNBOUND;
    KLOG_ERROR("PCI_BIND", "Failed to find matching driver for device %s.\n",
               dev->device.name);
    return PCI_BIND_FAILED_NO_MATCHING_DRIVER;
}

/* bind the best matching driver for the device based on match score */
bind_device_result_t pci_bind_device_to_best_driver(pci_driver_registry_t *reg,
                                                    pci_device_t *dev) {
    pci_match_score_t best_score = PCI_MATCH_SCORE_NONE;
    const pci_driver_t *best_driver = NULL;

    /* check for validity of the input arguments */
    if (!reg || !dev) {
        KLOG_ERROR("PCI_BIND",
                   "bind device failed. Invalid arguments.\n"
                   "\tdriver registry ref = %p, device ref = %p.\n",
                   reg, dev);
        return PCI_BIND_FAILED;
    }

    /* be can only bind driver to devices which are discovered */
    if (dev->device.state != DEVICE_STATE_DISCOVERED) {
        KLOG_ERROR("PCI_BIND",
                   "bind device failed.\n"
                   "\tdevice %s, state = %s, expected state = discovered.\n",
                   dev->device.name, device_state_name(dev->device.state));

        return PCI_BIND_FAILED_NOT_DISCOVERED;
    }

    /* find the best driver for the device */
    for (uint8_t i = 0; i < reg->count; i++) {
        pci_match_score_t score;
        const pci_driver_t *driver = reg->drivers[i];

        /* check for validity of the current driver instance */
        if (!driver) {
            KLOG_VERBOSE("PCI_BIND",
                         "Skipping invalid driver reference at index %u.\n", i);
            continue;
        }

        /* get the best matching sorce for device amongst all match rules
         * defined by driver */
        score = pci_driver_match_score(driver, dev);

        /* no driver match rule mached with device */
        if (score == PCI_MATCH_SCORE_NONE) {
            KLOG_VERBOSE("PCI_BIND", "driver %s does not match device %s.\n",
                         driver->name, dev->device.name);
            continue;
        }

        KLOG_VERBOSE("PCI_BIND",
                     "driver %s matched with device %s with a score of %u "
                     "(best score = %u).\n",
                     driver->name, dev->device.name, score, best_score);

        /* check if the current drivers best score is the best uptill now */
        if (score > best_score) {
            best_score = score;
            best_driver = driver;
        }
    }

    /* check if any driver matched with the device */
    if (!best_driver) {
        dev->device.state = DEVICE_STATE_UNBOUND;
        dev->device.bound_driver = NULL;
        dev->device.driver_data = NULL;

        KLOG_ERROR("PCI_BIND",
                   "bind result: device = %s, state = %s, reason = no matching "
                   "driver.\n",
                   dev->device.name, device_state_name(dev->device.state));

        return PCI_BIND_FAILED_NO_MATCHING_DRIVER;
    }

    /* device has been matched with a driver but not bound yet */
    dev->device.state = DEVICE_STATE_MATCHED;
    dev->device.bound_driver = NULL;
    dev->device.driver_data = NULL;

    KLOG_INFO("PCI_BIND",
              "bind selected: device = %s, device state = %s, driver = %s, "
              "match score %u.\n",
              dev->device.name, device_state_name(dev->device.state),
              best_driver->name, best_score);

    /* check if the probe routine exists for the driver */
    if (!best_driver->probe) {
        dev->device.state = DEVICE_STATE_PROBE_FAILED;

        KLOG_ERROR("PCI_BIND",
                   "bind failed: device = %s (state = %s), driver %s, reason = "
                   "no probe function.\n",
                   dev->device.name, device_state_name(dev->device.state),
                   best_driver->name);

        return PCI_BIND_FAILED_PROBE_FAILED;
    }

    /* set the device state to probing before starting probe */
    dev->device.state = DEVICE_STATE_PROBING;
    KLOG_INFO("PCI_BIND",
              "probe started: device = %s (state = %s), driver %s.\n",
              dev->device.name, device_state_name(dev->device.state),
              best_driver->name);

    /* invoke the probe from the selected driver */
    uint8_t probe_res = best_driver->probe(dev);

    /* Probing failed */
    if (!probe_res) {
        dev->device.state = DEVICE_STATE_PROBE_FAILED;
        dev->device.bound_driver = NULL;
        /* important to nullify the driver data since it might have been
         * partially initialized by proble routine. */
        dev->device.driver_data = NULL;

        KLOG_ERROR("PCI_BIND",
                   "probe failed: device = %s (state = %s), driver = %s, probe "
                   "result = %d.\n",
                   dev->device.name, device_state_name(dev->device.state),
                   best_driver->name, probe_res);

        return PCI_BIND_FAILED_PROBE_FAILED;
    }

    /* Probing success */
    dev->device.state = DEVICE_STATE_BOUND;
    dev->device.bound_driver = best_driver;

    KLOG_INFO("PCI_BIND",
              "probe success: device = %s (state = %s), driver = %s, probe "
              "result = %d.\n",
              dev->device.name, device_state_name(dev->device.state),
              best_driver->name, probe_res);

    return PCI_BIND_PASSED;
}

uint8_t pci_probe_and_bind_all(pci_device_registry_t *device_reg,
                               pci_driver_registry_t *driver_reg) {
    uint16_t bound_count = 0;

    /* check for validity of int input arguments */
    if (!driver_reg || !device_reg) {
        KLOG_ERROR(
            "PCI_BIND",
            "Failed probing & binding all devices. Invalid input arguments.\n"
            "\t driver registry = %p, device registry = %p.\n",
            driver_reg, device_reg);
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
        /* CASE 1: binary bind device */
        /* res = pci_bind_device(driver_reg, dev); */
        /* CASE 2: bind device based on match score */
        res = pci_bind_device_to_best_driver(driver_reg, dev);

        if (res == PCI_BIND_PASSED) {
            bound_count++;
        }
    }

    KLOG_INFO("PCI_BIND",
              "PCI Probe and Bound complete. Bound %u devices with drivers.\n",
              bound_count);
    return bound_count;
}
