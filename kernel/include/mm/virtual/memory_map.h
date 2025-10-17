#pragma once

#include <stdint.h>

#define E820_MAX_ENTRIES 128

// Memory Regions Types
#define E820_TYPE_AVAILABLE 1
#define E820_TYPE_RESERVED  2
#define E820_TYPE_ACPI_RECLAIMABLE 3
#define E820_TYPE_ACPI_NVS 4
#define E820_TYPE_UNUSABLE 5

// Reserved Memory Regions
typedef enum {
    RESERVED_TYPE_INIT          = (1 << 0) | (1 << 1) | (1 << 2),
    RESERVED_TYPE_BIOS          = 1 << 0,
    RESERVED_TYPE_IVT           = 1 << 1,
    RESERVED_TYPE_VGA           = 1 << 2,
    RESERVED_TYPE_KERNEL        = 1 << 3,
    RESERVED_TYPE_BITMAP        = 1 << 4,
    RESERVED_TYPE_PAGE_TABLE    = 1 << 5,
} reserved_memory_type_t;

// BIOS provided memory map entry structure
typedef struct {
    uint64_t base;      // Start address of the memory region
    uint64_t length;    // Length of the memory region
    uint32_t type;       // Type of the memory region (E820_TYPE_*)
    uint32_t reserved;  // Reserved for future use
} __attribute__((packed)) e820_entry_t;

// Exposed for parsing
#define E820_MAP_ADDRESS    0x5000
#define E820_MAP_COUNT_PTR  0x2004

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
