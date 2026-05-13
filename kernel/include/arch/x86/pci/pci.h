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

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;

    uint8_t header_type;
} pci_function_identity_t;

/**
 * Defining an enum for the differnet typed of outcome on probing:
 * If function is absent, should not be classified with transfort failure
 * For transport failure, result is 0
 * For success result is 1
 */
typedef enum pci_probe_result {
    PCI_PROBE_ABSENT = -1,
    PCI_PROBE_ERROR = 0,
    PCI_PROBE_PRESENT = 1,
} pci_probe_result_t;

static inline uint8_t pci_func_is_zero(pci_function_t func) {
    return func == 0;
}

static inline uint8_t
pci_identity_is_present(const pci_function_identity_t *id) {
    return (id && id->vendor_id != 0xffff);
}

/**
 * pci_probe_function - routine to check given a bdf, function is present and
 * valid
 *
 * @bdf - enpoint for which we need to check the function
 * @out - populate the info about function from config space
 *
 * @pci_probe_result_t - result of the probing
 */
pci_probe_result_t pci_probe_function(pci_bdf_t bdf,
                                      pci_function_identity_t *out);

#endif PCI_H
