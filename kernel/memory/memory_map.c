#include "memory_map.h"
#include "printk.h"

void parse_and_print_e820_map(void)
{
    // stores pointer to the E820 map
    e820_entry_t* map = (e820_entry_t*)E820_MAP_ADDRESS;

    // count of number of entries in the E820 map
    uint16_t count = *(uint16_t*)E820_MAP_COUNT_PTR;

    printk("\n[MEMORY MAP] BIOS provided %u entries:\n", count);

    for (uint16_t i = 0; i < count; i++)
    {
        printk("[%u] Base: 0x%08x%08x, Length: 0x%08x%08x, Type: %u\n",
               i,
               (uint32_t)(map[i].base >> 32),
               (uint32_t)(map[i].base & 0xFFFFFFFF),
               (uint32_t)(map[i].length >> 32),
               (uint32_t)(map[i].length & 0xFFFFFFFF),
               map[i].type
            );
    }
}
