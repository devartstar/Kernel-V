#include "arch/x86/pci/pci.h"
#include "lib/printk.h"

pci_probe_result_t pci_probe_function(pci_bdf_t bdf,
                                      pci_function_identity_t *out) {
    uint16_t vendor_id;

    if (!out) {
        KLOG_ERROR("PCI", "Invalid pointer to pci function identifier.\n");
        return PCI_PROBE_ERROR;
    }

    /* check if the endpoint to search is valid */
    if (!is_bdf_valid(bdf)) {
        KLOG_ERROR("PCI", "Invalid bdf (%02x:%02x.%u) endpoint.\n", bdf.bus,
                   bdf.device, bdf.function);
        return PCI_PROBE_ERROR;
    }

    /* Set default values to the function identifier */
    out->bdf = bdf;
    out->vendor_id = PCI_INVALID_VENDOR_ID;
    out->device_id = 0;
    out->class_code = 0;
    out->subclass = 0;
    out->prog_if = 0;
    out->revision_id = 0;
    out->header_type = 0;

    /* read the vendor id from the config space */
    vendor_id = pci_cfg_read16(bdf, PCI_CFG_VENDOR_ID);
    if (vendor_id == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI", "Invalid vendor id 0x%08x.\n", vendor_id);
        return PCI_PROBE_ABSENT;
    }

    /* Populate the fields of the function identifier reading from config space
     */
    out->vendor_id = vendor_id;
    out->device_id = pci_cfg_read16(bdf, PCI_CFG_DEVICE_ID);
    out->class_code = pci_cfg_read8(bdf, PCI_CFG_CLASS_CODE);
    out->subclass = pci_cfg_read8(bdf, PCI_CFG_SUBCLASS);
    out->prog_if = pci_cfg_read8(bdf, PCI_CFG_PROG_IF);
    out->revision_id = pci_cfg_read8(bdf, PCI_CFG_REVISION_ID);
    out->header_type = pci_cfg_read8(bdf, PCI_CFG_HEADER_TYPE);

    return PCI_PROBE_SUCCESS;
}

pci_probe_result_t pci_probe_slot(pci_bus_t bus, pci_device_t device,
                                  pci_probe_visitor_fn visitor, void *ctx,
                                  uint32_t *fn_found) {
    pci_bdf_t bdf0;
    pci_function_identity_t id0;
    pci_probe_result_t res;

    /* Check if input device and callback routine is valid */
    if (!is_device_valid(device) || !visitor || !fn_found) {
        KLOG_ERROR("PCI", "Input device or callback routine is not valid.\n");
        return PCI_PROBE_ERROR;
    }

    /* Initialize the caller owner function found count */
    *fn_found = 0;

    /* Probe function 0 in the slot bus:device */
    bdf0.bus = bus;
    bdf0.device = device;
    bdf0.function = 0;
    res = pci_probe_function(bdf0, &id0);
    if (res == PCI_PROBE_ABSENT) {
        KLOG_WARN("PCI", "Function 0 absent at slot bus:device %02x:%02x\n",
                  bdf0.bus, bdf0.device);
        return PCI_PROBE_ABSENT;
    }

    if (res == PCI_PROBE_ERROR) {
        KLOG_ERROR("PCI",
                   "Failed to probe function 0 at slot bus:device %02x:%02x\n",
                   bdf0.bus, bdf0.device);
        return PCI_PROBE_ERROR;
    }

    /* Successfully probed function 0 - trigger callback routine */
    visitor(&id0, ctx);
    (*fn_found)++;

    /* Read the bit 7 of Header type of the function 0 */
    if (!pci_cfg_header_type_is_multifunctional(id0.header_type)) {
        /* If slot is not multifunctional */
        KLOG_VERBOSE("PCI", "Completed probing slot at bus:device %02x:%02x\n",
                     bdf0.bus, bdf0.device);
        return PCI_PROBE_SUCCESS;
    }

    /* If bit 7 is set scan all functions 1-7 */
    for (pci_function_t fn = 1; fn < PCI_FUNCTION_PER_SLOT; fn++) {
        pci_bdf_t bdf;
        pci_function_identity_t id;

        bdf.bus = bus;
        bdf.device = device;
        bdf.function = fn;
        res = pci_probe_function(bdf, &id);

        if (res == PCI_PROBE_ERROR) {
            KLOG_ERROR(
                "PCI",
                "Failed to probe function %u at slot bus:device %02x:%02x\n",
                bdf.function, bdf.bus, bdf.device);
            return PCI_PROBE_ERROR;
        }

        /* Some functions in the slot might be absent, keep scanning. */
        if (res == PCI_PROBE_ABSENT) {
            KLOG_VERBOSE(
                "PCI",
                "function at bus:device %02x:%02x.%u absent. Keep scanning.\n",
                bdf.bus, bdf.device, fn);
        }

        if (res == PCI_PROBE_SUCCESS) {
            KLOG_VERBOSE("PCI",
                         "Suvvessfully probed Function %u at slot bus:device "
                         "%02x:%02x\n",
                         bdf.function, bdf.bus, bdf.device);
            visitor(&id, ctx);
            (*fn_found)++;
        }
    }

    return PCI_PROBE_SUCCESS;
}

pci_probe_result_t pci_scan_bus0(pci_scan_visitor_fn visitor, void *ctx,
                                 uint32_t *fn_found) {
    if (!visitor || !fn_found) {
        KLOG_ERROR("PCI", "Invalid callback or pointer to function counter.\n");
        return PCI_PROBE_ERROR;
    }

    (*fn_found) = 0;

    for (pci_device_t dev = 0; dev < PCI_DEVICES_PER_BUS; dev++) {
        uint32_t fn_count_in_slot = 0;
        pci_probe_result_t res;

        res = pci_probe_slot(0x00, dev, visitor, ctx, &fn_count_in_slot);

        if (res == PCI_PROBE_ERROR) {
            KLOG_ERROR("PCI", "probing slot for bus:device 00:%02x failed.\n",
                       dev);
            return PCI_PROBE_ERROR;
        }

        /* Some device slot in the bus might be absent, keep scanning. */
        if (res == PCI_PROBE_ABSENT) {
            KLOG_VERBOSE("PCI",
                         "slot at bus:device 00:%02x absent. Keep scanning.\n",
                         dev);
        }

        if (res == PCI_PROBE_SUCCESS) {
            (*fn_found) += fn_count_in_slot;
            KLOG_VERBOSE(
                "PCI",
                "probing slot for bus:device 00:%02x success. slot count %u.\n",
                dev, fn_count_in_slot);
        }
    }

    return PCI_PROBE_SUCCESS;
}

void pci_registry_init(pci_registry_t *reg) {
    /* check if pointer to registery structure is valid */
    if (!reg) {
        KLOG_ERROR("PCI", "invalid pointer to registry structure.\n");
        return;
    }

    reg->count = 0;

    /* intialize the function record entries with default */
    for (uint32_t i = 0; i < PCI_MAX_FUNCTIONS_BUS0; i++) {
        reg->entries[i].present = 0;
    }
}

uint8_t pci_registry_add(pci_registry_t *reg,
                         const pci_function_identity_t *id) {
    /* check if pointer to registry and function id to add is valid*/
    if (!reg || !id) {
        KLOG_ERROR(
            "PCI",
            "invalid pointer to registry struc or function id to add.\n");
        return 0;
    }

    /* check if already max/all functions are registered */
    if (reg->count >= PCI_MAX_FUNCTIONS_BUS0) {
        KLOG_ERROR(
            "PCI",
            "failed registring function id. max function registry achieved.\n");
        return 0;
    }

    /* check if function id was already registered */
    for (uint32_t i = 0; i < PCI_MAX_FUNCTIONS_BUS0; i++) {
        if (reg->entries[i].present &&
            pci_bdf_is_equal(reg->entries[i].id.bdf, id->bdf)) {
            KLOG_ERROR("PCI",
                       "failed registering function at %0x2:%0x2.%u. Already "
                       "registered!\n",
                       id->bdf.bus, id->bdf.device, id->bdf.function);
            return 0;
        }
    }

    /* register the function id */
    reg->entries[reg->count].id = *id;
    reg->entries[reg->count].present = 1;
    reg->count++;
    return 1;
}

uint8_t pci_enumerate_bus0_into_registry(pci_registry_t *reg) {
    pci_registry_fill_ctx_t reg_ctx;
    pci_probe_result_t result;
    uint32_t discovered = 0;

    /* check if the pointer to registry structure is valid */
    if (!reg) {
        KLOG_ERROR("PCI", "invalid pointer to the registry structure.\n");
        return 0;
    }

    /* initialize the registry context for callback */
    pci_registry_init(reg);
    reg_ctx.registry = reg;
    reg_ctx.inserted = 0;
    reg_ctx.errors = 0;

    /* scan bus0 for identifying all functions */
    result = pci_scan_bus0(pci_registry_fill_visitor, &reg_ctx, &discovered);
    if (result == PCI_PROBE_ERROR) {
        KLOG_ERROR("PCI", "scanning bus 0 failed.\n");
        return 0;
    }

    /* check errors if we missed registring some function identity */
    if (reg_ctx.errors != 0) {
        KLOG_ERROR("PCI",
                   "failed capturing function identity to registry. failed "
                   "count = %u.\n",
                   reg_ctx.errors);
        return 0;
    }

    /* sanity check from callback and probing routine. */
    if (reg_ctx.inserted != discovered || reg->count != discovered) {
        KLOG_ERROR("PCI",
                   "count mismatch discovered: discovered=%u inserted=%u "
                   "registry=%u.\n",
                   discovered, reg_ctx.inserted, reg->count);
        return 0;
    }

    KLOG_INFO(
        "PCI",
        "Enumerated bus0 successfully. functions discoverd=%u stored=%u.\n",
        discovered, reg->count);
    return 1;
}
