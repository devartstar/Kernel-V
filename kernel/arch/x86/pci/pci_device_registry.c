#include "arch/x86/pci/pci_device_registry.h"
#include "arch/x86/pci/pci.h"
#include "arch/x86/pci/pci_devices.h"
#include "lib/printk.h"
#include "lib/string.h"

void pci_device_registry_init(pci_device_registry_t *reg) {
    if (!reg) {
        KLOG_ERROR("PCI", "pci device registry init failed. invalid reference "
                          "to device registry.\n");
        return;
    }

    /* assign memory to the device registry block and zero it */
    memset(reg, 0, sizeof(*reg));
}

uint8_t pci_device_registry_add(pci_device_registry_t *reg,
                                pci_function_record_t *rec, device_t *parent) {
    pci_device_t *dev;
    uint8_t res;

    /* check for input args validity */
    if (!reg || !rec || !rec->present) {
        KLOG_ERROR("PCI",
                   "failed add device to registry. invalid input args.\n");
        return 0;
    }

    /* check if we exceeded the max registry entries */
    if (reg->count >= PCI_MAX_DEVICE_OBJECTS) {
        KLOG_ERROR(
            "PCI",
            "failed add device to registry. already max entries registered.\n");
        return 0;
    }

    /* get the reference of the registry entry */
    dev = &reg->devices[reg->count];

    /* create a device object from record */
    if (!pci_device_init(dev, rec, parent)) {
        KLOG_ERROR("PCI", "failed add device to registry. Failed to intialize "
                          "a new device object.\n");
        return 0;
    }

    KLOG_VERBOSE("PCI", "Successfully intialized device %s in the registry.\n",
                 dev->device.name);
    reg->count++;
    return 1;
}

pci_device_t *pci_device_registry_find_bdf(pci_device_registry_t *reg,
                                           pci_bdf_t bdf) {
    /* check for valid ref. to input args */
    if (!reg) {
        KLOG_ERROR(
            "PCI",
            "failed to find device object for [%02x:%02x.%u]. Invalid ref "
            "to the registry.\n",
            bdf.bus, bdf.device, bdf.function);
        return NULL;
    }

    /* find the device for bfd endpoint in the registry */
    for (uint32_t i = 0; i < reg->count; i++) {
        pci_device_t *dev = &reg->devices[i];

        if (!dev->record) {
            continue;
        }

        if (pci_bdf_is_equal(dev->record->id.bdf, bdf)) {
            KLOG_VERBOSE(
                "PCI",
                "found matching device for the endpoint [%02x:%02x.%u].\n",
                bdf.bus, bdf.device, bdf.function);
            return dev;
        }
    }

    KLOG_ERROR("PCI", "no matching device found for [%02x:%02x.%u].\n", bdf.bus,
               bdf.device, bdf.function);

    return NULL;
}

uint8_t pci_device_registry_materialize_from_record_registry(
    pci_device_registry_t *dev_reg, pci_record_registry_t *rec_reg,
    device_t *parent) {

    /* check for input arg validity */
    if (!dev_reg || !rec_reg) {
        KLOG_ERROR("PCI", "failed materialize device reg. from record reg. bcs "
                          "of invalide input args.\n");
        return 0;
    }

    /* clear the device registry per recrf runtime device */
    pci_device_registry_clear_record_runtime_links(rec_reg);

    /* initalize the device registry before materializing */
    pci_device_registry_init(dev_reg);

    /* iterate thru the record reg and materialize device reg entry for each */
    for (uint32_t i = 0; i < rec_reg->count; i++) {
        pci_function_record_t *rec = &rec_reg->entries[i];

        if (!rec->present) {
            continue;
        }

        if (!pci_device_registry_add(dev_reg, rec, parent)) {
            KLOG_ERROR("PCI",
                       "failed to materialize device reg. from record reg. bcs "
                       "failed to initialize/add device object to reg. for "
                       "[%02x:%02x.%u]\n",
                       rec->id.bdf.bus, rec->id.bdf.device,
                       rec->id.bdf.function);
            return 0;
        }
    }

    KLOG_VERBOSE("PCI",
                 "successfully materialized device reg. from record. reg.\n");
    return 1;
}

void pci_device_registry_foreach(const pci_device_registry_t *reg,
                                 pci_device_registry_visitor_fn visitor,
                                 void *ctx) {
    if (!reg || !visitor) {
        KLOG_ERROR("DEVICE", "Failed iterating the device registry. Invalid "
                             "ref to input args.\n");
        return;
    }

    for (uint32_t i = 0; i < reg->count; i++) {
        const pci_device_t *dev = &reg->devices[i];
        if (!dev->record || !dev->record->present) {
            continue;
        }
        visitor(dev, ctx);
    }
}

pci_device_t *pci_device_registry_find_vendor_device(pci_device_registry_t *reg,
                                                     uint16_t vendor_id,
                                                     uint16_t device_id) {
    if (!reg) {
        return NULL;
    }

    for (uint32_t i = 0; i < reg->count; i++) {
        pci_device_t *pdev = &reg->devices[i];

        if (!pdev->record) {
            continue;
        }

        if (pdev->record->id.vendor_id == vendor_id &&
            pdev->record->id.device_id == device_id) {
            return pdev;
        }
    }

    return NULL;
}

pci_device_t *pci_device_registry_find_bound_vendor_device(
    pci_device_registry_t *reg, uint16_t vendor_id, uint16_t device_id) {
    if (!reg) {
        return NULL;
    }

    for (uint32_t i = 0; i < reg->count; i++) {
        pci_device_t *pdev = &reg->devices[i];

        if (!pdev->record) {
            continue;
        }

        if (pdev->record->id.vendor_id == vendor_id &&
            pdev->record->id.device_id == device_id &&
            pdev->device.state == DEVICE_STATE_BOUND) {
            return pdev;
        }
    }

    return NULL;
}

uint32_t pci_device_registry_state_count(const pci_device_registry_t *reg,
                                         device_state_t state) {
    if (!reg) {
        KLOG_ERROR("PCI", "Failed to cound devices in state %s.\n",
                   device_state_name(state));
        return 0;
    }

    uint32_t state_count[DEVICE_STATE_COUNT] = {0};

    pci_device_registry_foreach(reg, pci_device_state_match, state_count);

    KLOG_INFO("PCI", "Count of devices in the state %s is %u.\n",
              device_state_name(state), state_count[state]);

    return state_count[state];
}

void pci_device_registry_clear_record_runtime_links(
    pci_record_registry_t *reg) {
    if (!reg) {
        KLOG_ERROR(
            "PCI",
            "clear runtime device failed. invalid ref to record registry %p.\n",
            reg);
        return;
    }

    for (uint32_t i = 0; i < reg->count; i++) {
        reg->entries[i].runtime_device = NULL;
    }
}

uint8_t
pci_device_registry_validate_links(const pci_device_registry_t *dev_reg,
                                   const pci_record_registry_t *rec_reg) {
    uint32_t linked_records = 0;
    if (!dev_reg || !rec_reg) {
        KLOG_ERROR("PCI",
                   "device registry link validation failed: invalid args.\n");
        return 0;
    }

    /* Validate device registry entries for correct link */
    for (uint32_t i = 0; i < dev_reg->count; i++) {
        const pci_device_t *dev = &dev_reg->devices[i];
        const pci_function_record_t *rec;

        if (!dev->record) {
            KLOG_ERROR("PCI",
                       "device registry link validation failed: "
                       "device index=%u has NULL record.\n",
                       i);
            return 0;
        }

        rec = dev->record;

        if (!rec->present) {
            KLOG_ERROR("PCI",
                       "device registry link validation failed: "
                       "device=%s points to non-present record.\n",
                       dev->device.name);
            return 0;
        }

        if (dev->device.bus_type != DEVICE_BUS_PCI) {
            KLOG_ERROR("PCI",
                       "device registry link validation failed: "
                       "device=%s has wrong bus type=%u.\n",
                       dev->device.name, dev->device.bus_type);
            return 0;
        }

        if (dev->device.bus_data != rec) {
            KLOG_ERROR("PCI",
                       "device registry link validation failed: "
                       "device=%s bus_data does not point to record.\n",
                       dev->device.name);
            return 0;
        }

        if (rec->runtime_device != dev) {
            KLOG_ERROR("PCI",
                       "device registry link validation failed: "
                       "record [%02x:%02x.%u] runtime_device mismatch.\n",
                       rec->id.bdf.bus, rec->id.bdf.device,
                       rec->id.bdf.function);
            return 0;
        }
    }

    /* Validate record registry entries for correct link */
    for (uint32_t i = 0; i < rec_reg->count; i++) {
        const pci_function_record_t *rec = &rec_reg->entries[i];

        if (!rec->present) {
            continue;
        }

        if (!rec->runtime_device) {
            KLOG_ERROR("PCI",
                       "device registry link validation failed: "
                       "record [%02x:%02x.%u] has NULL runtime_device.\n",
                       rec->id.bdf.bus, rec->id.bdf.device,
                       rec->id.bdf.function);
            return 0;
        }

        linked_records++;
    }

    if (linked_records != dev_reg->count) {
        KLOG_ERROR("PCI",
                   "device registry link validation failed: "
                   "linked_records=%u device_count=%u.\n",
                   linked_records, dev_reg->count);
        return 0;
    }

    KLOG_INFO("PCI", "device registry links valid: devices=%u records=%u.\n",
              dev_reg->count, rec_reg->count);

    return 1;
}

pci_device_t *pci_record_get_runtime_device(pci_function_record_t *rec) {
    if (!rec || !rec->present) {
        KLOG_ERROR("PCI", "failed to get runtime device: invalid record.\n");
        return NULL;
    }

    if (!rec->runtime_device) {
        KLOG_ERROR("PCI",
                   "failed to get runtime device for [%02x:%02x.%u]: NULL.\n",
                   rec->id.bdf.bus, rec->id.bdf.device, rec->id.bdf.function);
        return NULL;
    }

    return rec->runtime_device;
}
