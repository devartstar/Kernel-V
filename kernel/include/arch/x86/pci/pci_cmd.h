#ifndef PCI_CMD_H
#define PCI_CMD_H

#include "arch/x86/pci/pci_cfg.h"
#include "stdint.h"

#define PCI_CMD_IO_SPACE 0x0001u
#define PCI_CMD_MEM_SPACE 0x0002u
#define PCI_CMD_BUS_MASTER 0x0004u

typedef struct pci_command_staus_info {
    uint16_t command;
    uint16_t status;
} pci_command_status_info_t;

static inline uint16_t pci_read_command(pci_bdf_t bdf) {
    return pci_cfg_read16(bdf, PCI_CFG_COMMAND);
}

static inline uint16_t pci_read_status(pci_bdf_t bdf) {
    return pci_cfg_read16(bdf, PCI_CFG_STATUS);
}

static inline void pci_write_command(pci_bdf_t bdf, uint16_t value) {
    pci_cfg_write16(bdf, PCI_CFG_COMMAND, value);
}

/**
 * pci_read_cmd_status - read the command and status bits of the bdf endpoint
 *
 * @bdf - endpoint to read
 * @out - update the structure to store command and status bits
 */
static inline void pci_read_cmd_status(pci_bdf_t bdf,
                                       pci_command_status_info_t *out) {
    /* check validity of pointer to the output status */
    if (!out) {
        KLOG_ERROR("PCI",
                   "invalide pointer passed for upating command status.\n");
        return;
    }

    out->command = pci_read_command(bdf);
    out->status = pci_read_status(bdf);
}

/**
 * pci_update_cmd_bits - update the pci command for a given bdfm endpoint
 *
 * @bdf - endpoint whose command bits to update
 * @set_mask - bitmask of the bits to set in the command bits
 * @clear_mask - bitmask of the bits to clear in the command bits
 */
static inline void pci_update_cmd_bits(pci_bdf_t bdf, uint16_t set_mask,
                                       uint16_t clear_mask) {
    uint16_t command = pci_read_command(bdf);

    command = (uint16_t)(command | set_mask);
    command = (uint16_t)(command & (uint16_t)(~clear_mask));

    pci_write_command(bdf, command);
}

#endif /* PCI_CMD_H */
