#pragma once

#include <stdint.h>

#define E820_MAX_ENTRIES 128

// Memory Regions Types
#define E820_TYPE_AVAILABLE 1
#define E820_TYPE_RESERVED  2
#define E820_TYPE_ACPI_RECLAIMABLE 3
#define E820_TYPE_ACPI_NVS 4
#define E820_TYPE_UNUSABLE 5

// BIOS provided memory map entry structure
typedef struct {
    uint64_t base;      // Start address of the memory region
    uint64_t length;    // Length of the memory region
    uint32_t type;       // Type of the memory region (E820_TYPE_*)
    uint32_t reserved;  // Reserved for future use
} __attribute__((packed)) e820_entry_t;

// Exposed for parsing
#define E820_MAP_ADDRESS 0x5000
#define E820_MAP_COUNT_PTR 0x2004

// Memory map structure for storing usable memory regions
typedef struct {
    uint64_t base;          // Start address of the memory region
    uint64_t length;        // Length of the memory region
    uint32_t type;          // Type of the memory region (E820_TYPE_*)
} memory_region_t;

#define MAX_MEMORY_REGIONS 32

extern void parse_and_print_e820_map(void);
extern memory_region_t usable_memory_region[MAX_MEMORY_REGIONS];
extern uint16_t usable_memory_region_count;
