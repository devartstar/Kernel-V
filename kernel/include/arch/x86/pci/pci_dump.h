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
#endif
