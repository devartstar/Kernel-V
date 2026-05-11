#ifndef PCI_CFG_H
#define PCI_CFG_H

/**
 * PCI configuration register:
 * 0xCFC - I/O Port to access the selected data.
 * 0xCF8 - I/O Port to slect the PCI configuration space.
         - It holds a 32 bit selector of the below layout.

31            24 23      16 15    11 10   8 7      2 1 0
+---------------+----------+--------+------+---------+---+
| Enable = 1    | Bus      | Device | Func | Reg[7:2]| 00|
+---------------+----------+--------+------+---------+---+

 * Bit 31 (Enable bit) - should be set to 1 for config space access
 * Bits (16-23)(11-15)(8-10) - 8 bits bus, 5 bits device, 3 bits function
 * Bits (2-7) - register number in dword units
 * Bit 1 - should be 0 for 32 bit alignment
 */

#include <stdint.h>

/*
 * if function 0 is absent then the slot is generally treated as absent
 * if function 0 is present then - check the bit 7 of header type
 *   bit 7 clear - kernel to probe onlu function 0
 *   bit 7 set - kernel to probe function 1..7 individually
 */
#define PCI_CFG_HEADER_TYPE_MASK 0x7f           // 0111 1111
#define PCI_CFG_HEADER_TYPE_MULTI_FUNCTION 0x80 // 1000 0000

#define PCI_CFG_ADDR_PORT 0xCF8u
#define PCI_CFG_DATA_PORT 0xCFCu

#define PCI_CFG_ENABLE_BIT 0x80000000u
#define PCI_CFG_BUS_SHIFT 16u
#define PCI_CFG_DEV_SHIFT 11u
#define PCI_CFG_FUNC_SHIFT 8u
#define PCI_CFG_REG_SHIFT 0u

#define PCI_CFG_BUS_MASK 0xFFu
#define PCI_CFG_DEV_MASK 0x1Fu
#define PCI_CFG_FUNC_MASK 0x07u
#define PCI_CFG_REG_MASK 0xFCu

typedef uint8_t pci_bus_t;
typedef uint8_t pci_device_t;
typedef uint8_t pci_function_t;

typedef struct pci_bdf {
    pci_bus_t bus;
    pci_device_t device;
    pci_function_t function;
} pci_bdf_t;

static inline uint8_t is_device_valid(pci_device_t device) {
    return device < 32;
}

static inline uint8_t is_function_valid(pci_function_t function) {
    return function < 8;
}

static inline uint8_t is_bdf_valid(pci_bdf_t bdf) {
    return is_device_valid(bdf.device) && is_function_valid(bdf.function);
}

static inline uint8_t
pci_cfg_header_type_is_multifunctional(uint8_t header_type) {
    return ((header_type & PCI_CFG_HEADER_TYPE_MULTI_FUNCTION) != 0);
}

static inline uint8_t pci_cfg_header_type_layout(uint8_t header_type) {
    return (uint8_t)(header_type & PCI_CFG_HEADER_TYPE_MASK);
}

static inline uint32_t pci_cfg_addr_make(pci_bdf_t bdf, uint8_t reg_offset) {
    return PCI_CFG_ENABLE_BIT |
           (((uint32_t)bdf.bus & PCI_CFG_BUS_MASK) << PCI_CFG_BUS_SHIFT) |
           (((uint32_t)bdf.device & PCI_CFG_DEV_MASK) << PCI_CFG_DEV_SHIFT) |
           (((uint32_t)bdf.function & PCI_CFG_FUNC_MASK)
            << PCI_CFG_FUNC_SHIFT) |
           ((uint32_t)reg_offset & PCI_CFG_REG_MASK);
}

#endif PCI_CFG_H
