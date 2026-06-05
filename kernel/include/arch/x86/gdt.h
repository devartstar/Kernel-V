#pragma once
#include <stdint.h>

/***********************************************
 * GDT TABLE
           GDTR.base
              │
              ▼
+---------------------------+
| Offset |   Address        | Content
+---------------------------+
|  +0    | GDTR + 0x00      | NULL Descriptor
|        |                  | (mandatory, selector 0)
+---------------------------+
|  +8    | GDTR + 0x08      | Segment 1 Descriptor
|        |                  | (e.g. Kernel Code)
+---------------------------+
| +16    | GDTR + 0x10      | Segment 2 Descriptor
|        |                  | (e.g. Kernel Data)
+---------------------------+
| +24    | GDTR + 0x18      | Segment 3 Descriptor
|        |                  | (optional: User Code)
+---------------------------+
|  ...   | ...              | ...
+---------------------------+


* GDT ENTRY:
   64                    56      52      48           40           32
   ┌─────────────────────┬───────┬───────┬────────────┬────────────┐
   │        [8] Base     │ [4]   │ [4]   │   [8]      │   [8]      │
   │                     │ Flags │ Limit │  Access    │   Base     │
   └─────────────────────┴───────┴───────┴────────────┴────────────┘
   ┌─────────────────────────────────────┬─────────────────────────┐
   │             [16] Base               │       [16] Limit        │
   └─────────────────────────────────────┴─────────────────────────┘
   32                                   16                         0

**************************************************/

#define GDT_ENTRIES 6

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
