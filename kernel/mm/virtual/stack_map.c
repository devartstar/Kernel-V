#include "mm/stack_map.h"
#include "core/debug.h"
#include "core/panik.h"
#include "lib/printk.h"
#include "mm/paging.h"
#include "mm/pmm.h"

void map_high_stack(uint32_t stack_bottom, uint32_t stack_top, uint32_t flags) {
    uint32_t stack_size = stack_top - stack_bottom;
    debug_module(MEMORY, "Mapping stack pages...\n");

    for (uint32_t off = PAGE_SIZE; off < stack_size; off += PAGE_SIZE) {
        uint32_t virt = stack_bottom + off;
        void *phys_frame = pmm_alloc_frame();
        if (!phys_frame) {
            debug_module(MEMORY,
                         "Failed to allocate stack frame for virt=0x%08x\n",
                         PRINT_UINT32(virt));
            panik("Stack frame allocation failed");
        }
        debug_module(MEMORY, "Mapping stack page: virt=0x%08x phys=0x%08x\n",
                     PRINT_UINT32(virt), PRINT_UINT32(phys_frame));
        paging_map_page(virt, (uint32_t)phys_frame, PAGE_PRESENT | PAGE_WRITE);
        pmm_set_frame_bitmap((uint32_t)phys_frame,
                             (uint32_t)phys_frame + PAGE_SIZE);
    }

    pr_info("Paging initialized successfully!\n");
}
