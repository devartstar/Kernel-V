#include "arch/x86/pci/pci_cmd.h"
#include "arch/x86/pci/pci_registry.h"

uint8_t pci_enrich_reocrd_cmd_status(struct pci_function_record *record) {
    /* verify the validity of the function record to decode */
    if (!record || !record->present) {
        KLOG_ERROR("PCI", "PCI Command and Status bytes decode failed. Invalid "
                          "record reference.\n");
        return 0;
    }

    record->cmd_status.command = pci_read_command(record->id.bdf);
    record->cmd_status.status = pci_read_status(record->id.bdf);
    record->cmd_status_valid = 1;

    return 1;
}
