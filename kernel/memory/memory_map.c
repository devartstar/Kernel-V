#include "memory_map.h"
#include "printk.h"

uint16_t usable_memory_region_count = 0;
memory_region_t usable_memory_region[MAX_MEMORY_REGIONS];

static const char* region_type_to_string (uint32_t type)
{
    switch (type)
    {
        case E820_TYPE_AVAILABLE: return "Available";
        case E820_TYPE_RESERVED: return "Reserved";
        case E820_TYPE_ACPI_RECLAIMABLE: return "ACPI Reclaimable";
        case E820_TYPE_ACPI_NVS: return "ACPI NVS";
        case E820_TYPE_UNUSABLE: return "Unusable";
        default: return "Unknown";
    }
}

void parse_and_print_e820_map(void)
{
    // stores pointer to the E820 map
    e820_entry_t* map = (e820_entry_t*)E820_MAP_ADDRESS;

    // count of number of entries in the E820 map
    uint16_t count = *(uint16_t*)E820_MAP_COUNT_PTR;

    printk("\n[MEMORY MAP] BIOS provided %u entries:\n", count);

    for (uint16_t i = 0; i < count; i++)
    {
        const char* current_region_type = region_type_to_string(map[i].type);
        printk("[%u] Base: 0x%08x%08x, Length: 0x%08x%08x, Type: %s\n",
               i,
               (uint32_t)(map[i].base >> 32),
               (uint32_t)(map[i].base & 0xFFFFFFFF),
               (uint32_t)(map[i].length >> 32),
               (uint32_t)(map[i].length & 0xFFFFFFFF),
               current_region_type
            );

        if (map[i].type == E820_TYPE_AVAILABLE && usable_memory_region_count < MAX_MEMORY_REGIONS)
        {
            usable_memory_region[usable_memory_region_count] = (memory_region_t){
                .base = map[i].base,
                .length = map[i].length,
                .type = map[i].type
            };
            usable_memory_region_count++;
        }
    }

    printk("\n[MEMORY MAP] Usable memory regions count: %u\n", usable_memory_region_count);

}
