#pragma once
#include <stdint.h>

/***************************************
 * GDT TABLE
    ┌─────────────────────┬───────────┐
    │       Address       │  Content  │
    ├─────────────────────┼───────────┤
    │ GDTR Offset + 0     │   NULL    │
    ├─────────────────────┼───────────┤
    │ GDTR Offset + 8     │ Segment 1 │
    ├─────────────────────┼───────────┤
    │ GDTR Offset + 16    │ Segment 2 │
    └─────────────────────┴───────────┘

* GDT ENTRY
    64                    56      52      48           40           32
    ┌─────────────────────┬───────┬───────┬────────────┬────────────┐
    │        [8] Base     │ [4]   │ [4]   │   [8]      │   [8]      │
    │                     │ Flags │ Limit │  Access    │   Base     │
    └─────────────────────┴───────┴───────┴────────────┴────────────┘
    ┌─────────────────────────────────────┬─────────────────────────┐
    │             [16] Base               │       [16] Limit        │
    └─────────────────────────────────────┴─────────────────────────┘
    32                                   16                         0

********************************************/

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void gdt_init(void);
