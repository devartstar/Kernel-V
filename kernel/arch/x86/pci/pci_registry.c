#include "arch/x86/pci/pci_registry.h"
#include "lib/printk.h"

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

void pci_registry_foreach(const pci_registry_t *reg,
                          pci_registry_visitor_fn visitor, void *ctx) {
    /* check if the input pointers are valid */
    if (!reg || !visitor) {
        KLOG_ERROR(
            "PCI",
            "invalid pointer to register structure or callback routine.\n");
        return;
    }

    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->entries[i].present) {
            visitor(&reg->entries[i], ctx);
        }
    }
}

const pci_function_record_t *pci_registry_find_bdf(const pci_registry_t *reg,
                                                   pci_bdf_t bdf) {
    /* check if the pointer to the registry is valid */
    if (!reg) {
        KLOG_ERROR("PCI", "invalid registry pointer to find function.\n");
    }

    /* loop through all the entries of the registry to find function */
    for (uint32_t i = 0; i < reg->count; i++) {
        const pci_function_record_t *rec = &reg->entries[i];

        if (!rec->present) {
            continue;
        }

        if (pci_bdf_is_equal(rec->id.bdf, bdf)) {
            KLOG_VERBOSE("PCI", "maching record found for bdf %02x:%02x.%u.\n",
                         bdf.bus, bdf.device, bdf.function);
            return rec;
        }
    }

    KLOG_ERROR("PCI", "No matching record found for bdf %02x:%02x.%u.\n",
               bdf.bus, bdf.device, bdf.function);
    return NULL;
}
