#ifndef PCI_H
#define PCI_H

#include "arch/x86/pci/pci_cfg.h"

#define PCI_FUNCTION_PER_SLOT 8
#define PCI_DEVICES_PER_BUS 32

/**
 * pci_function_identity - a minimal register record of what the config space
 * tells the pci subsystem for discovery.
 */
typedef struct pci_function_identity {
    pci_bdf_t bdf;

    uint8_t vendor_id;
    uint8_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;

    uint8_t header_type;
} pci_function_identity_t;

static inline uint8_t pci_func_is_zero(pci_function_t func) {
    return func == 0;
}

static inline uint8_t
pci_identity_is_present(const pci_function_identity_t *id) {
    return (id && id->vendor_id != 0xffff);
}

#endif PCI_H
