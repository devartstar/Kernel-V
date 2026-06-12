#include "arch/x86/pci/pci_bind.h"
#include "arch/x86/pci/pci_dump.h"
#include "lib/string.h"

/* bind the first matching driver for the device */
bind_device_result_t pci_bind_device(pci_driver_registry_t *reg,
                                     pci_device_t *dev) {

    if (!reg || !dev || !dev->record) {
        KLOG_ERROR(
            "PCI_BIND",
            "bind failed: reason=invalid_args driver_registry=%p device=%p\n",
            reg, dev);
        return PCI_BIND_FAILED;
    }

    /* be can only bind driver to devices which are discovered */
    if (dev->device.state != DEVICE_STATE_DISCOVERED) {
        KLOG_ERROR("PCI_BIND",
                   "bind failed: device=%s state=%s reason=not_discovered\n",
                   dev->device.name, device_state_name(dev->device.state));

        return PCI_BIND_FAILED_NOT_DISCOVERED;
    }

    /* iterate thru the registry to assign driver to the device */
    for (uint32_t i = 0; i < reg->count; i++) {
        const pci_driver_t *driver = reg->drivers[i];
        uint8_t probe_res;

        if (!driver) {
            KLOG_VERBOSE("PCI_BIND", "bind skip: reason=invalid_driver_ref\n");
            continue;
        }

        /* check if driver matches for the device */
        if (!pci_driver_matches(driver, dev)) {
            KLOG_VERBOSE("PCI_BIND",
                         "bind result: device=%s state=%s "
                         "reason=no_matching_driver_for_candidate driver=%s\n",
                         dev->device.name, device_state_name(dev->device.state),
                         driver->name);
            continue;
        }

        dev->device.state = DEVICE_STATE_MATCHED;

        if (!driver->probe) {
            dev->device.state = DEVICE_STATE_PROBE_FAILED;
            KLOG_VERBOSE("PCI_BIND",
                         "probe failed: device=%s driver=%s state=%s "
                         "reason=no_probe_function result=0\n",
                         dev->device.name, driver->name,
                         device_state_name(dev->device.state));
            return PCI_BIND_FAILED_PROBE_FAILED;
        }

        dev->device.state = DEVICE_STATE_PROBING;

        probe_res = driver->probe(dev);

        /* Probing failed */
        if (!probe_res) {
            dev->device.state = DEVICE_STATE_PROBE_FAILED;
            dev->device.bound_driver = NULL;
            dev->device.driver_data = NULL;

            KLOG_ERROR("PCI_BIND",
                       "probe failed: device=%s driver=%s state=%s result=%d\n",
                       dev->device.name, driver->name,
                       device_state_name(dev->device.state), probe_res);
            return PCI_BIND_FAILED_PROBE_FAILED;
        }

        /* Probing success */
        dev->device.state = DEVICE_STATE_BOUND;
        dev->device.bound_driver = driver;

        KLOG_INFO("PCI_BIND",
                  "probe success: device=%s driver=%s state=%s result=%d\n",
                  dev->device.name, driver->name,
                  device_state_name(dev->device.state), probe_res);

        return PCI_BIND_PASSED;
    }

    dev->device.state = DEVICE_STATE_UNBOUND;
    KLOG_ERROR("PCI_BIND",
               "bind result: device=%s state=%s reason=no_matching_driver\n",
               dev->device.name, device_state_name(dev->device.state));
    return PCI_BIND_FAILED_NO_MATCHING_DRIVER;
}

/* bind the best matching driver for the device based on match score */
bind_device_result_t
pci_bind_device_to_best_driver(pci_driver_registry_t *reg, pci_device_t *dev,
                               pci_bind_summary_t *out_summary) {
    pci_match_score_t best_score = PCI_MATCH_SCORE_NONE;
    const pci_driver_t *best_driver = NULL;

    /* check for validity of the input arguments */
    if (!reg || !dev || !dev->record || !dev->record->present) {
        out_summary->failed_invalid++;

        KLOG_ERROR(
            "PCI_BIND",
            "bind failed: reason=invalid_args driver_registry=%p device=%p\n",
            reg, dev);
        return PCI_BIND_FAILED;
    }

    /* be can only bind driver to devices which are discovered */
    if (dev->device.state != DEVICE_STATE_DISCOVERED) {
        out_summary->skipped++;

        KLOG_ERROR("PCI_BIND",
                   "bind skipped: device=%s state=%s reason=not_discovered\n",
                   dev->device.name, device_state_name(dev->device.state));

        return PCI_BIND_FAILED_NOT_DISCOVERED;
    }

    out_summary->bind_attempted++;

    /* find the best driver for the device */
    for (uint32_t i = 0; i < reg->count; i++) {
        pci_match_score_t score;
        const pci_driver_t *driver = reg->drivers[i];

        /* check for validity of the current driver instance */
        if (!driver) {
            KLOG_VERBOSE("PCI_BIND",
                         "bind skip: reason=invalid_driver_ref index=%u\n", i);
            continue;
        }

        /* get the best matching sorce for device amongst all match rules
         * defined by driver */
        score = pci_driver_match_score(driver, dev);

        /* no driver match rule mached with device */
        if (score == PCI_MATCH_SCORE_NONE) {
            KLOG_VERBOSE("PCI_BIND",
                         "bind skip: device=%s driver=%s reason=score_none\n",
                         dev->device.name, driver->name);
            continue;
        }

        KLOG_VERBOSE("PCI_BIND",
                     "candidate match: device=%s driver=%s score=%u best=%u\n",
                     dev->device.name, driver->name, score, best_score);

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

        out_summary->unbound++;

        KLOG_INFO("PCI_BIND",
                  "bind result: device=%s state=%s reason=no_matching_driver\n",
                  dev->device.name, device_state_name(dev->device.state));

        return PCI_BIND_FAILED_NO_MATCHING_DRIVER;
    }

    /* device has been matched with a driver but not bound yet */
    dev->device.state = DEVICE_STATE_MATCHED;
    dev->device.bound_driver = NULL;
    dev->device.driver_data = NULL;

    KLOG_INFO("PCI_BIND", "selected driver %s for device %s score=%u\n",
              best_driver->name, dev->device.name, best_score);

    /* check if the probe routine exists for the driver */
    if (!best_driver->probe) {
        dev->device.state = DEVICE_STATE_PROBE_FAILED;

        out_summary->probe_failed++;

        KLOG_ERROR("PCI_BIND",
                   "probe failed: device=%s driver=%s state=%s "
                   "reason=no_probe_function result=0\n",
                   dev->device.name, best_driver->name,
                   device_state_name(dev->device.state));

        return PCI_BIND_FAILED_PROBE_FAILED;
    }

    /* set the device state to probing before starting probe */
    dev->device.state = DEVICE_STATE_PROBING;
    KLOG_INFO("PCI_BIND", "probe started: device=%s driver=%s state=%s\n",
              dev->device.name, best_driver->name,
              device_state_name(dev->device.state));
    /* invoke the probe from the selected driver */
    uint8_t probe_res = best_driver->probe(dev);

    /* Probing failed */
    if (!probe_res) {
        dev->device.state = DEVICE_STATE_PROBE_FAILED;
        dev->device.bound_driver = NULL;
        /* important to nullify the driver data since it might have been
         * partially initialized by proble routine. */
        dev->device.driver_data = NULL;

        out_summary->probe_failed++;

        KLOG_ERROR("PCI_BIND",
                   "probe failed: device=%s driver=%s state=%s result=%d\n",
                   dev->device.name, best_driver->name,
                   device_state_name(dev->device.state), probe_res);

        return PCI_BIND_FAILED_PROBE_FAILED;
    }

    /* Probing success */
    dev->device.state = DEVICE_STATE_BOUND;
    dev->device.bound_driver = best_driver;

    out_summary->bound++;

    KLOG_INFO("PCI_BIND",
              "probe success: device=%s driver=%s state=%s result=%d\n",
              dev->device.name, best_driver->name,
              device_state_name(dev->device.state), probe_res);

    return PCI_BIND_PASSED;
}

uint8_t pci_probe_and_bind_all(pci_device_registry_t *device_reg,
                               pci_driver_registry_t *driver_reg,
                               pci_bind_summary_t *out_summary) {
    uint16_t bound_count = 0;
    pci_bind_summary_t local_summary;

    /* initialize the bind summary */
    pci_bind_summary_init(&local_summary);

    /* check for validity of int input arguments */
    if (!driver_reg || !device_reg) {
        KLOG_ERROR(
            "PCI_BIND",
            "Failed probing & binding all devices. Invalid input arguments.\n"
            "\t driver registry = %p, device registry = %p.\n",
            driver_reg, device_reg);

        if (out_summary) {
            *out_summary = local_summary;
        }

        return 0;
    }

    KLOG_INFO("PCI_BIND", "bind-all start: devices=%u drivers=%u\n",
              device_reg->count, driver_reg->count);

    /* Iterate thru all the function records in the registry and invoke
     * pci_bind_device - takes input as pci_device_t and pci_driver_t */
    for (uint32_t i = 0; i < device_reg->count; i++) {
        pci_device_t *dev = &device_reg->devices[i];
        bind_device_result_t res;

        local_summary.device_seen++;

        /* bind the device to the matching driver from registry */
        /* CASE 1: binary bind device */
        /* res = pci_bind_device(driver_reg, dev); */
        /* CASE 2: bind device based on match score */
        res = pci_bind_device_to_best_driver(driver_reg, dev, &local_summary);

        if (dev->device.state != DEVICE_STATE_BOUND) {
        }

        if (res == PCI_BIND_PASSED) {
            bound_count++;
        }
    }

    pci_dump_bind_summary(&local_summary);

    *out_summary = local_summary;

    KLOG_INFO("PCI_BIND", "bind-all complete: devices=%u drivers=%u bound=%u\n",
              device_reg->count, driver_reg->count, bound_count);
    return bound_count;
}

void pci_bind_summary_init(pci_bind_summary_t *summary) {
    if (!summary) {
        KLOG_ERROR("BIND_SUMMARY",
                   "failed bind_summary init: invalid ref. to summary %p.\n",
                   summary);
        return;
    }

    summary->device_seen = 0;
    summary->bind_attempted = 0;
    summary->bound = 0;
    summary->unbound = 0;
    summary->probe_failed = 0;
    summary->skipped = 0;
    summary->failed_invalid = 0;
}
