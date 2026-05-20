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

#include "core/io.h"
#include "lib/printk.h"
#include <stdint.h>

/*
 * Standard PCI configuration header offsets.
 * These are byte offsets within one function's config space.
 */
#define PCI_CFG_VENDOR_ID 0x00u
#define PCI_CFG_DEVICE_ID 0x02u
#define PCI_CFG_COMMAND 0x04u
#define PCI_CFG_STATUS 0x06u
#define PCI_CFG_REVISION_ID 0x08u
#define PCI_CFG_PROG_IF 0x09u
#define PCI_CFG_SUBCLASS 0x0Au
#define PCI_CFG_CLASS_CODE 0x0Bu
#define PCI_CFG_CACHELINE_SIZE 0x0Cu
#define PCI_CFG_LATENCY_TIMER 0x0Du
#define PCI_CFG_HEADER_TYPE 0x0Eu
#define PCI_CFG_BIST 0x0Fu

/*
 * Header type interpretation
 * if function 0 is absent then the slot is generally treated as absent
 * if function 0 is present then - check the bit 7 of header type
 *   bit 7 clear - kernel to probe onlu function 0
 *   bit 7 set - kernel to probe function 1..7 individually
 */
#define PCI_CFG_HEADER_TYPE_MASK 0x7f           // 0111 1111
#define PCI_CFG_HEADER_TYPE_MULTI_FUNCTION 0x80 // 1000 0000

/*
 * Standard sentinel for "no function present".
 */
#define PCI_INVALID_VENDOR_ID 0xFFFFu

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

/* PCI Config Space Read Utilities */

/**
 * pci_cfg_read32 - 32 bit read of the pci config space
 * @bdf - to specify the endpoint whose config space to read
 * @reg_off - offset(bytes) in config space at  which to read
 *
 * @return value read from config space.
 */
static inline uint32_t pci_cfg_read32(pci_bdf_t bdf, uint8_t reg_off) {
    uint32_t addr;

    if (!is_bdf_valid(bdf)) {
        KLOG_ERROR("PCI", "Querying for invalid bfd %02x:%02.%02f\n", bdf.bus,
                   bdf.device, bdf.function);
        return 0xFFFFFFFFu;
    }

    addr = pci_cfg_addr_make(bdf, reg_off);
    outl(PCI_CFG_ADDR_PORT, addr);
    return inl(PCI_CFG_DATA_PORT);
}

/**
 * pci_cfg_read16 - reads 16 bits of config space from a given offset
 *
 * @bdf - to specify the endpoint whose config space to read
 * @reg_ofset - offset(bytes) of the config space to read from. It should be
 * either:
 *              - low halfword aligned (0x...0) read bits 0-15
 *              - high halfword aligned (0x...2) read bits 16-31
 */
static inline uint16_t pci_cfg_read16(pci_bdf_t bdf, uint8_t reg_off) {
    uint32_t value;
    uint32_t shift;

    /* check register offset alignment */
    if (reg_off & 0x1) {
        KLOG_ERROR(
            "PCI",
            "Incorrect alignment of config space offset (%0x8) to read16.\n",
            reg_off);
        return 0xFFFF;
    }

    value = pci_cfg_read32(bdf, reg_off);

    /* offset bytes to bits conversion */
    shift = (uint8_t)((reg_off & 0x2) << 3);

    /* shift = 0 for low halfword and = 16 for high halfword */
    return (uint16_t)((value >> shift) & 0xFFFF);
}

/**
 * pci_cfg_read8 - reads 8 bits of config space from a given offset
 *
 * @bdf - to specify the endpoint whose config space to read
 * @reg_ofset - offset(bytes) of the config space to read from.
 */
static inline uint8_t pci_cfg_read8(pci_bdf_t bdf, uint8_t reg_off) {
    uint32_t value;
    uint32_t shift;

    value = pci_cfg_read32(bdf, reg_off);

    /* offset bytes to bits conversion to shift */
    shift = (uint8_t)((reg_off & 0x3) << 3);

    /* shift can be 0(0-7), 8(8-15), 16(16-23), 24(24,31) */
    return (uint16_t)((value >> shift) & 0xFF);
}

/* PCI Config Space Field Extractor */

static inline uint16_t pci_dword_lo16(uint32_t value) {
    return (uint16_t)(value & 0xFFFFu);
}

static inline uint16_t pci_dword_hi16(uint32_t value) {
    return (uint16_t)((value >> 16) & 0xFFFFu);
}

static inline uint8_t pci_dword_byte0(uint32_t value) {
    return (uint8_t)(value & 0xFFu);
}

static inline uint8_t pci_dword_byte1(uint32_t value) {
    return (uint8_t)((value >> 8) & 0xFFu);
}

static inline uint8_t pci_dword_byte2(uint32_t value) {
    return (uint8_t)((value >> 16) & 0xFFu);
}

static inline uint8_t pci_dword_byte3(uint32_t value) {
    return (uint8_t)((value >> 24) & 0xFFu);
}
#endif /* PCI_CFG_H */
