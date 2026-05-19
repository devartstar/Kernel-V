#include "arch/x86/pci/pci_dump.h"
#include "lib/printk.h"

void pci_dump_record_visitor(const pci_function_record_t *record, void *ctx) {
    /* context unused */
    (void)ctx;

    /* check validity of pointer to record and its presence */
    if (!record || !record->present) {
        KLOG_ERROR(
            "PCI",
            "invalid entries for function record or record not present.\n");
        return;
    }

    const pci_function_identity_t *id = &(record->id);
    KLOG_INFO("PCI",
              "%02x:%02x.%u: vendor=%04x device=%04x"
              "\n\tclass=%02x subclass=%02x (%s)"
              "\n\tprogif=%02x rev=%02x hdr=%02x multi=%u layout=%02x\n",
              id->bdf.bus, id->bdf.device, id->bdf.function, id->vendor_id,
              id->device_id, id->class_code, id->subclass,
              pci_class_name(id->class_code, id->subclass), id->prog_if,
              id->revision_id, id->header_type,
              pci_cfg_header_type_is_multifunctional(id->header_type),
              pci_cfg_header_type_layout(id->header_type));
}

void pci_dump_registry(const pci_registry_t *reg) {
    /* check if pointer to the registry is valid */
    if (!reg) {
        KLOG_ERROR("PCI", "invalide pointer to the registry structure.\n");
        return;
    }

    KLOG_INFO("PCI", "===== PCI REGISTRY DUMP (%u entries) =====\n",
              reg->count);
    pci_registry_foreach(reg, pci_dump_record_visitor, NULL);
    KLOG_INFO("PCI", "===== PCI REGISTRY DUMP END =====\n", reg->count);
}
