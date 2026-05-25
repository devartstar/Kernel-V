#ifndef PCI_DUMP_H
#define PCI_DUMP_H

#include "arch/x86/pci/pci_registry.h"

/**
 * pci_dump_record_visitor - callback for dumping all pci records.
 * @rec - registry record to dump.
 * @ctx - constext passed to the callback.
 */
void pci_dump_record_visitor(const pci_function_record_t *record, void *ctx);

/**
 * pci_dump_registry - helper routine to dump all the function registered in the
 * registry.
 * @reg - pointer to the registry structure.
 */
void pci_dump_registry(const pci_registry_t *reg);

/**
 * pci_class_name - decode the class name based on class and subclass bits
 */
static const char *pci_class_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
    case 0x01:
        return "Mass storage";
    case 0x02:
        return "Network controller";
    case 0x03:
        return "Display controller";
    case 0x06:
        switch (subclass) {
        case 0x00:
            return "Host bridge";
        case 0x01:
            return "ISA bridge";
        case 0x04:
            return "PCI-to-PCI bridge";
        default:
            return "Bridge device";
        }
    default:
        return "Unknown";
    }
}

/**
 * pci_bar_kind_name - convert enum to string for bar type.
 */
static const char *pci_bar_kind_name(pci_bar_kind_t kind) {
    switch (kind) {
    case PCI_BAR_KIND_IO:
        return "io";
    case PCI_BAR_KIND_MEM32:
        return "mem32";
    case PCI_BAR_KIND_MEM64:
        return "mem64";
    case PCI_BAR_KIND_UNUSED:
        return "unused";
    default:
        return "unknown";
    }
}

/**
 * pci_dump_type0_bars - helper routine to dump all the bars for the type0
 * endpoint
 *
 * @record - pointer to the type0 endpoint identifier.
 */
void pci_dump_type0_bars(const pci_function_record_t *record);

/**
 * pci_dump_record_resource - dump entries of a function record
 * @rec - reference to the function record to dump.
 */
void pci_dump_record_resources(const pci_function_record_t *rec);

/**
 * pci_dump_registry_resources - dump all entries of the pci registry structure.
 * @reg - reference to the pci registry structure.
 */
void pci_dump_registry_resources(const pci_registry_t *reg);
#endif
