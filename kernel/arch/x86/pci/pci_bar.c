#include "arch/x86/pci/pci_bar.h"
#include "arch/x86/pci/pci_cfg.h"
#include "arch/x86/pci/pci_registry.h"

pci_bar_raw_read_result_t
pci_read_type0_bars_raw(pci_function_record_t *record) {
    /* check pointer to the function record is valid and record is present */
    if (!record || !record->present) {
        KLOG_ERROR(
            "PCI",
            "invalid pointer to function record or record not present.\n");
        return PCI_BAR_RAW_READ_ERROR;
    }

    /* skip reading raw bars if header type is 1 as its a bridge device and 0
     * for endpoint device */
    if (!pci_header_layout_is_type0(record->id.header_type)) {
        KLOG_ERROR("PCI",
                   "header type of function record at %0x2:%02x.%u is %u.\n",
                   record->id.bdf.bus, record->id.bdf.device,
                   record->id.bdf.function, record->id.header_type);
        return PCI_BAR_RAW_READ_SKIPPED;
    }

    /* initialize the bars structure */
    for (uint8_t i = 0; i < PCI_TYPE0_BAR_COUNT; i++) {
        pci_bar_info_init(&record->bars[i], i);
    }

    /* read the raw low and high bits and update the bar record */
    for (uint8_t i = 0; i < PCI_TYPE0_BAR_COUNT; i++) {
        uint32_t raw_low;
        uint8_t offset;

        /* we need to get the offset of BARi from config space start */
        offset = pci_cfg_bar_offset(i);

        /* read 32 bits of BARi starting from the offset of config space. */
        raw_low = pci_cfg_read32(record->id.bdf, offset);
        record->bars[i].raw_lo = raw_low;

        /* if BARi is of type Memory 64 then also read BARi+1 */
        if ((raw_low & PCI_BAR_IO_SPACE) == 0 && /* Its is a memory BAR */
            (raw_low & PCI_BAR_MEM_TYPE_MASK) ==
                PCI_BAR_MEM_TYPE_64 && /* Memory type 64 */
            ((i + 1u) <
             PCI_TYPE0_BAR_COUNT)) { /* BAR i+1 also present to read */
            uint32_t raw_high;
            offset = pci_cfg_bar_offset(i + 1u);
            raw_high = pci_cfg_read32(record->id.bdf, offset);
            record->bars[i].raw_hi = raw_high;

            KLOG_VERBOSE("PCI",
                         "[%02x:%02x.%u] BAR[%u] is a memory 64bit BAR. [low "
                         "bits:%08x, high bits:%08x]\n",
                         record->id.bdf.bus, record->id.bdf.device,
                         record->id.bdf.function, i, raw_low, raw_high);

            /* BARi+1 is consumes as upper half of 64 bit BARi */
            i++;
        } else {
            KLOG_VERBOSE("PCI",
                         "[%02x:%02x.%u] BAR[%u] is a memory 32bit BAR. [low "
                         "bits:%08x]\n",
                         record->id.bdf.bus, record->id.bdf.device,
                         record->id.bdf.function, i, raw_low);
        }
    }

    record->bars_valid = 1;
    KLOG_VERBOSE("PCI", "[%02x:%02x.%u] BARs read successfully.\n",
                 record->id.bdf.bus, record->id.bdf.device,
                 record->id.bdf.function);

    return PCI_BAR_RAW_READ_OK;
}

void pci_decode_type0_bars(struct pci_function_record *record) {
    /* check for validity of the record */
    if (!record || !record->present || !record->bars_valid) {
        KLOG_ERROR("PCI", "Invalid function record to decode.\n");
        return;
    }

    /* assign values to all bar info */
    for (uint8_t i = 0; i < PCI_TYPE0_BAR_COUNT; i++) {
        pci_bar_info_t *bar_info = &record->bars[i];

        bar_info->present = 0;
        bar_info->kind = PCI_BAR_KIND_UNUSED;
        bar_info->prefetchable = 0;
        bar_info->base = 0;

        /* BAR register shouldbe non zero to decode */
        if (bar_info->raw_lo == 0) {
            continue;
        }

        /* check and assign proper valie if IO bar */
        if (pci_bar_is_io(bar_info->raw_lo)) {
            bar_info->present = 1;
            bar_info->kind = PCI_BAR_KIND_IO;
            bar_info->base =
                (uint64_t)(bar_info->raw_lo & PCI_BAR_IO_BASE_MASK);
            continue;
        }

        /* check and assign proper value if MEM32 bar */
        if (pci_bar_is_mem32(bar_info->raw_lo)) {
            bar_info->present = 1;
            bar_info->kind = PCI_BAR_KIND_MEM32;
            bar_info->base =
                (uint64_t)(bar_info->raw_lo & PCI_BAR_MEM_BASE_MASK);
            bar_info->prefetchable =
                pci_bar_mem_is_prefetchable(bar_info->raw_lo);
            continue;
        }

        /* check and assign proper value if MEM4 bar */
        if (pci_bar_is_mem64(bar_info->raw_lo)) {
            uint32_t lo = (bar_info->raw_lo & PCI_BAR_MEM_BASE_MASK);
            uint32_t hi = bar_info->raw_hi;

            bar_info->present = 1;
            bar_info->kind = PCI_BAR_KIND_MEM64;
            bar_info->base = (uint64_t)lo | ((uint64_t)hi << 32);
            bar_info->prefetchable =
                pci_bar_mem_is_prefetchable(bar_info->raw_lo);

            if (i + 1 < PCI_TYPE0_BAR_COUNT) {
                /* this bas has no independent significance */
                pci_bar_info_t *partner_bar = &record->bars[i + 1];
                partner_bar->present = 0;
                partner_bar->kind = PCI_BAR_KIND_UNUSED;
                partner_bar->prefetchable = 0;
                partner_bar->base = 0;
            }

            i++;
            continue;
        }

        /* Other BAR encoding is marked as unsed */
    }
}

uint8_t pci_enrich_record_bars(struct pci_function_record *record) {
    pci_bar_raw_read_result_t result;

    /* check validity of the reference to the record */
    if (!record || !record->present) {
        KLOG_ERROR(
            "PCI",
            "BAR decoding failed. Invalid reference to function record.\n");
        return 0;
    }

    /* read the raw bytes of the BAR */
    result = pci_read_type0_bars_raw(record);
    if (result == PCI_BAR_RAW_READ_ERROR) {
        KLOG_ERROR("PCI", "BAR decoding failed. Failed reading BAR bytes.\n");
        return 0;
    }

    if (result == PCI_BAR_RAW_READ_SKIPPED) {
        KLOG_VERBOSE("PCI", "BAR decoding skipped. Skipped BAR reading.\n");
        record->bars_valid = 0;
        return 0;
    }

    /* successfully read the BAR bytes. Next decode them. */
    pci_decode_type0_bars(record);
    record->bars_valid = 1;
    return 1;
}
