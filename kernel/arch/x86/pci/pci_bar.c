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
