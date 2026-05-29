#ifndef PCI_CAPABILITY_H
#define PCI_CAPABILITY_H

/*
 * Conventional PCI capabilities are linked-list entries that live
 * within the first 256 bytes of PCI config space. bytes 0x40..0xFF = capability
 * Status register at offset 0x06 - bit 4 says if capability list exists.
 * Header at address 0x34 contains the capability pointer ---> First Capability
 * Each Capability:
 *		byte 0 => Capability ID
 *		byte 1 => Next Capability Pointer
 */

#include "arch/x86/pci/pci_cfg.h"
#include "lib/printk.h"
#include "stdint.h"

#define PCI_CAP_MAX_PER_FUNCTION 16

/* standard location in config space where the pointer to first capability */
#define PCI_CFG_CAP_PTR 0x34

/* bit mask for checking the status register bits for capacility list present
 * 4th bit in the status bits tell wether the capacity is present
 */
#define PCI_STATUS_CAP_LIST 0x10

/* Common PCI capability IDs */
#define PCI_CAP_ID_PM 0x01u
#define PCI_CAP_ID_MSI 0x05u
#define PCI_CAP_ID_PCIX 0x07u
#define PCI_CAP_ID_VENDOR 0x09u
#define PCI_CAP_ID_PCIEXP 0x10u
#define PCI_CAP_ID_MSIX 0x11u

typedef enum pci_cap_kind {
    PCI_CAP_KIND_UNKNOWN = 0,
    PCI_CAP_KIND_PM,
    PCI_CAP_KIND_MSI,
    PCI_CAP_KIND_MSIX,
    PCI_CAP_KIND_PCIEXP,
    PCI_CAP_KIND_VENDOR,
} pci_cap_kind_t;

/**
 * pci_cap_info - struct to store specific capability information of function
 * present - 1 if this slot contains a discovered capability
 * id - capability identifier
 * offset - config space pointer of the capability header
 * next - pointer to the offset to next capability header
 * kind - capability kind defined from the header
 */
typedef struct pci_capability_info {
    uint8_t present;
    uint8_t id;
    uint8_t offset;
    uint8_t next;
    pci_cap_kind_t kind;
} pci_capability_info_t;

static inline void pci_capability_info_init(pci_capability_info_t *cap) {
    /* check if pointer to capability info is valid */
    if (!cap) {
        KLOG_ERROR("PCI", "invalid reference to the capability.\n");
        return;
    }

    cap->present = 0;
    cap->id = 0;
    cap->offset = 0;
    cap->next = 0;
    cap->kind = PCI_CAP_KIND_UNKNOWN;
}

/**
 * pci_capability_list_present_in_status - given the status bits utility to
 * check if capability list is present for this config space.
 * @status - 16 bit status bits
 * @return - 1 if status has capability present bit set else 0
 */
static inline uint8_t pci_capability_list_present_in_status(uint16_t status) {
    return (status & PCI_STATUS_CAP_LIST) != 0;
}

/**
 * pci_capability_list_present_in_fuction - given a function endpoint utility to
 * check if capability list is present
 * @bdf - function endpoint to check for
 */
static inline uint8_t pci_capability_list_prsent_in_fuction(pci_bdf_t bdf) {
    uint16_t status_bits = pci_cfg_read16(bdf, PCI_CFG_STATUS);
    return pci_capability_list_present_in_status(status_bits);
}

/**
 * pci_capability_ptr_valid - checks the validity of capability pointer
 * Conditions for the capability pointer to be valid:
 * non zero
 * 4-byte aligned
 * first pointer starts after 0x40
 * last pointer starts at max 0xFC leaving 2 byte capability header
 */
static inline uint8_t pci_capability_ptr_valid(uint8_t ptr) {
    if (ptr == 0) {
        return 0;
    }

    if ((ptr & 0x03) != 0) {
        return 0;
    }

    if (ptr < 0x40 || ptr > 0xFC) {
        return 0;
    }

    return 1;
}

static inline pci_cap_kind_t pci_capability_kind_from_id(uint8_t id) {
    switch (id) {
    case PCI_CAP_ID_PM:
        return PCI_CAP_ID_PM;

    case PCI_CAP_ID_MSI:
        return PCI_CAP_KIND_MSI;

    case PCI_CAP_ID_MSIX:
        return PCI_CAP_KIND_MSIX;

    case PCI_CAP_ID_PCIEXP:
        return PCI_CAP_KIND_PCIEXP;

    case PCI_CAP_ID_VENDOR:
        return PCI_CAP_KIND_VENDOR;

    default:
        return PCI_CAP_KIND_UNKNOWN;
    }
}

struct pci_function_record;

/**
 * pci_capability_state_init - for a function record update the capability
 * information with default vaules
 * @rec - pointer to the function record to initialize.
 */
void pci_capability_state_init(struct pci_function_record *rec);

/**
 * pci_capability_info_enrich - for a function record update the capability
 * information with correct vaules post reading from config space.
 * @rec - pointer to the function record to enrich.
 */
void pci_capability_info_enrich(struct pci_function_record *rec);

/**
 * pci_capability_offset_seen - to protect from being stuck in loop
 * @rec - function record whose capability we are checking.
 * @offset - capability offset in the linked list of capacilites
 */
uint8_t pci_capability_offset_seen(const struct pci_function_record *rec,
                                   uint8_t offset);

/**
 * pci_capability_enrich_records - iterate thru the linked list of cpability
 * pointes starting from PCI_CFG_CAP_PTR and update function record
 * @rec - pointer to the function record to update cpability info into
 * @return 1 if capabilities discovered successfully else 0
 */
uint8_t pci_capability_enrich_records(struct pci_function_record *rec);

/**
 * pci_capability_find_kind - from a fucntion record find the capability of a
 * particular kind
 * @rec - function record to find the capability in
 * @kind - kind of capability of find
 *
 * @return - pointer to the capability info
 */
const pci_capability_info_t *
pci_capability_find_kind(const struct pci_function_record *rec,
                         pci_cap_kind_t kind);

/**
 * pci_capability_of_kind_present - check if capacity of a kind is present in
 * record.
 * @rec - function record to search in.
 * @kind - capability kind to search for
 *
 * @return 1 if capacity matching kind found else 0
 */
static uint8_t
pci_capability_of_kind_present(const struct pci_function_record *rec,
                               pci_cap_kind_t kind) {
    return pci_capability_find_kind(rec, kind) != NULL;
}

#endif /* PCI_CAPABILITY_H */
