#ifndef PCI_BAR_H
#define PCI_BAR_H

#include <stdint.h>

#define PCI_TYPE0_BAR_COUNT 6

/* List of PCI BAR register offsets */
#define PCI_CFG_BAR0 0x10u
#define PCI_CFG_BAR1 0x14u
#define PCI_CFG_BAR2 0x18u
#define PCI_CFG_BAR3 0x1Cu
#define PCI_CFG_BAR4 0x20u
#define PCI_CFG_BAR5 0x24u

#define PCI_BAR_IO_SPACE 0x1u
#define PCI_BAR_MEM_TYPE_MASK 0x6u
#define PCI_BAR_MEM_TYPE_32 0x0u
#define PCI_BAR_MEM_TYPE_64 0x4u
#define PCI_BAR_MEM_PREFETCHABLE 0x8u

#define PCI_BAR_IO_BASE_MASK 0xFFFFFFFCu
#define PCI_BAR_MEM_BASE_MASK 0xFFFFFFF0u

/**
 * pci_bar_type_t : Enum to define the type of BAR
 */
typedef enum pci_bar_kind {
    PCI_BAR_KIND_UNUSED = 0,
    PCI_BAR_KIND_IO,
    PCI_BAR_KIND_MEM32,
    PCI_BAR_KIND_MEM64
} pci_bar_kind_t;

/**
 * pci_bar_into stores all information of a BAR in a given endpoint.
 * an endpoint can have upto 6 BAR slots.
 * index: BAR Number
 * present: if BAR decodes a resource - 1 else 0
 * kind: type of the BAR - I/O, MEM32, MEM64, UNUSED
 * raw_lo: raw dword read from BARn
 * raw_hi: raw dword read from BARn+1 for MEM64, else 0
 * base: resource bytes after masing type bits
 * prefetchable: only for memoty BAR
 */
typedef struct pci_bar_info {
    uint8_t index;
    uint8_t present;
    pci_bar_kind_t kind;

    uint32_t raw_lo;
    uint32_t raw_hi;

    uint64_t base;
    uint8_t prefetchable;
} pci_bar_info_t;

/**
 * pci_bar_info_init intializes PCI BAR record in an endpoint with initial
 * values
 * bar - pointer to the BAR information record
 * index - index entry of the current BAR in the list of BARs present in
 * function config space
 */
static inline void pci_bar_info_init(pci_bar_info_t *bar, uint8_t index) {
    /* check if pointer to the BAR info structure is valid */
    if (!bar) {
        return;
    }

    /* initialize default values to the BAR */
    bar->index = index;
    bar->present = 0;
    bar->kind = PCI_BAR_KIND_UNUSED;
    bar->raw_lo = 0;
    bar->raw_hi = 0;
    bar->base = 0;
    bar->prefetchable = 0;
}

#endif PCI_BAR_H
