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

void pci_dump_type0_bars(const pci_function_record_t *record) {
    /* check for function recoed validity */
    if (!record || !record->present || !record->bars_valid) {
        KLOG_ERROR(
            "PCI",
            "cannot dump BAR info because of invalid function record.\n");
        return;
    }

    KLOG_INFO("PCI", "=== [%02x:%02x.%u] PCI BAR DUMP START ===\n",
              record->id.bdf.bus, record->id.bdf.device,
              record->id.bdf.function);
    for (uint8_t i = 0; i < PCI_TYPE0_BAR_COUNT; i++) {
        /* dump the BAR record */
        pci_bar_info_t *bar = &record->bars[i];

        KLOG_INFO("PCI",
                  "\tBAR[%u] kind=%s present=%u raw_lo=%08x raw_hi=%08x "
                  "base(low=0x%08x, high=0x%08x) prefetch=%u\n",
                  bar->index, pci_bar_kind_name(bar->kind), bar->present,
                  bar->raw_lo, bar->raw_hi,
                  PRINT_UINT64_LO(bar->base & 0xFFFFFFFF),
                  PRINT_UINT64_HI(bar->base >> 32), bar->prefetchable);
    }
    KLOG_INFO("PCI", "=== [%02x:%02x.%u] PCI BAR DUMP END ===\n",
              record->id.bdf.bus, record->id.bdf.device,
              record->id.bdf.function);

    return;
}

void pci_dump_record_capabilities(const pci_function_record_t *rec) {
    if (!rec || !rec->present || !rec->caps_valid) {
        return;
    }

    KLOG_INFO("PCI", "[%02x:%02x.%u] caps_present=%u cap_count=%u\n",
              rec->id.bdf.bus, rec->id.bdf.device, rec->id.bdf.function,
              rec->caps_present, rec->cap_count);

    for (uint8_t i = 0; i < rec->cap_count; i++) {
        const pci_capability_info_t *cap = &rec->caps[i];

        if (!cap->present) {
            continue;
        }

        KLOG_INFO(
            "PCI", "  CAP[%u] id=0x%02x kind=%s offset=0x%02x next=0x%02x\n", i,
            cap->id, pci_capability_kind_name(cap->id), cap->offset, cap->next);
    }
}

void pci_dump_record_resources(const pci_function_record_t *rec) {
    if (!rec || !rec->present) {
        return;
    }

    KLOG_INFO("PCI",
              "[%02x:%02x.%u] vendor=%04x device=%04x class=%02x subclass=%02x "
              "progif=%02x rev=%02x hdr=%02x\n",
              rec->id.bdf.bus, rec->id.bdf.device, rec->id.bdf.function,
              rec->id.vendor_id, rec->id.device_id, rec->id.class_code,
              rec->id.subclass, rec->id.prog_if, rec->id.revision_id,
              rec->id.header_type);

    if (rec->cmd_status_valid) {
        KLOG_INFO("PCI",
                  "\tcmd=%04x status=%04x io_en=%u mem_en=%u busm_en=%u\n",
                  rec->cmd_status.command, rec->cmd_status.status,
                  (rec->cmd_status.command & PCI_CMD_IO_SPACE) != 0,
                  (rec->cmd_status.command & PCI_CMD_MEM_SPACE) != 0,
                  (rec->cmd_status.command & PCI_CMD_BUS_MASTER) != 0);
    }

    if (rec->bars_valid) {
        for (uint8_t i = 0; i < PCI_TYPE0_BAR_COUNT; i++) {
            const pci_bar_info_t *bar = &rec->bars[i];

            KLOG_INFO("PCI",
                      "\tBAR[%u] kind=%s present=%u raw_lo=%08x raw_hi=%08x "
                      "base(low=0x%08x, high=0x%08x) prefetch=%u\n",
                      bar->index, pci_bar_kind_name(bar->kind), bar->present,
                      bar->raw_lo, bar->raw_hi, PRINT_UINT64_LO(bar->base),
                      PRINT_UINT64_HI(bar->base), bar->prefetchable);
        }
    }

    if (rec->caps_valid) {
        KLOG_INFO("PCI", "  caps_present=%u cap_count=%u\n", rec->caps_present,
                  rec->cap_count);

        for (uint8_t i = 0; i < rec->cap_count; i++) {
            const pci_capability_info_t *cap = &rec->caps[i];

            if (!cap->present) {
                continue;
            }

            KLOG_INFO("PCI",
                      "  CAP[%u] id=0x%02x kind=%s offset=0x%02x next=0x%02x\n",
                      i, cap->id, pci_capability_kind_name(cap->kind),
                      cap->offset, cap->next);
        }
    }
}

void pci_dump_registry_resources(const pci_registry_t *reg) {
    if (!reg) {
        return;
    }

    KLOG_INFO("PCI", "=== PCI RESOURCE DUMP START (count=%u) ===\n",
              reg->count);

    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->entries[i].present) {
            pci_dump_record_resources(&reg->entries[i]);
        }
    }

    KLOG_INFO("PCI", "=== PCI RESOURCE DUMP END ===\n", reg->count);
}
