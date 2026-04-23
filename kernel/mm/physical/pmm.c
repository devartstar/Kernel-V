#include "mm/pmm.h"
#include "core/debug.h"
#include "lib/printk.h"
#include "mm/memory_map.h"
#include "mm/paging.h"

static uint8_t *frame_bitmap = NULL;
static uint32_t max_frame_idx = 0;

/* Total usable frames */
static uint32_t total_frames = 0;
/* Number of used frames */
static uint32_t used_frames = 0;

/**
 * 1. memory_address -> frame_index = memory_address / PAGE_SIZE
 * 2. max_frame_index for the array = max (frame_index)
 * 3. frame_bitmap[max_frame_index] => each entry = 8 bits => each bit = 1 PAGE
 */
#define FRAME_INDEX(addr) ((addr) / PAGE_SIZE)
#define FRAME_INDEX_TO_PHYS_ADDR(idx) (idx * PAGE_SIZE)
#define BITMAP_SET(idx) (frame_bitmap[(idx) / 8] |= (1 << ((idx) % 8)))
#define BITMAP_CLEAR(idx) (frame_bitmap[(idx) / 8] &= ~(1 << ((idx) % 8)))
#define BITMAP_GET(idx) (frame_bitmap[(idx) / 8] & (1 << ((idx) % 8)))

//
//  Initialize the entire bitmap to 1 (used)
//  Iterate through all the usable memory regions
//  and mark the frames as free in the bitmap
//
void pmm_init(void) {
    total_frames = 0;
    used_frames = 0;

    /* usage_memory_region count is initialized as a part of parse e820 map
     * Calculate the number of allocatable frames from the usable memory */
    for (uint32_t usable_memory_region_idx = 0;
         usable_memory_region_idx < usable_memory_region_count;
         usable_memory_region_idx++) {
        /* get the end address of the usable region */
        phys_addr_t region_base =
            usable_memory_region[usable_memory_region_idx].base;
        uint32_t region_length =
            usable_memory_region[usable_memory_region_idx].length;
        phys_addr_t region_end = region_base + region_length;

        /* at what index of frame_bitmap can the address of region_end be
         * represented */
        uint32_t current_frame_idx = FRAME_INDEX(region_end - 1);

        /* max of all frame index to calculate the size of the frame bitmap
         * array */
        if (current_frame_idx > max_frame_idx) {
            max_frame_idx = current_frame_idx;
        }
    }

    /* Calculate the max index of the frame bitmap to cover all the usable
     * regions */
    frame_bitmap = (uint8_t *)0x90000;
    uint32_t max_frame_bitmap_idx = (max_frame_idx / 8) + 1;

    /* Set all bits in the frame bitmap as 1 (occupied) */
    for (uint32_t frame = 0; frame < max_frame_bitmap_idx; frame++) {
        frame_bitmap[frame] = 0xFF;
    }

    /* Clear bits for all pages in the usable memory region
     * each page = 1 bit of a 8 bit entry of frame bitmap */
    for (uint32_t usable_memory_region_idx = 0;
         usable_memory_region_idx < usable_memory_region_count;
         usable_memory_region_idx++) {
        phys_addr_t region_base =
            usable_memory_region[usable_memory_region_idx].base;
        uint32_t region_length =
            usable_memory_region[usable_memory_region_idx].length;
        phys_addr_t region_end = region_base + region_length;

        for (uint32_t addr = region_base; addr < region_end;
             addr += PAGE_SIZE) {
            /* bitmap index = (address / PAGE_SIZE) / 8,
             * bit position in array index = (address / PAGE_SIZE) % 8
             */
            uint32_t frame_index = FRAME_INDEX(addr);
            if (frame_index <= max_frame_idx) {
                BITMAP_CLEAR(frame_index);
                total_frames++;
            }
        }
    }

    /* initially none of the usable frames are used */
    used_frames = 0;
    debug_module(MEMORY, "Total Usable Frames: %u\n",
                 PRINT_UINT32(total_frames));
    pr_info("[PROCESS_MGMT] Frame Bitmap initialized at address: %p\n",
            frame_bitmap);
}

//
//  @param reserved_type: Type of memory region to reserve
//
void pmm_reserve_memory_region(reserved_memory_type_t reserved_type) {
    //  addr < 1Mib Reserve all memory below 1Mb for BIOS, IVT, VGA
    if ((reserved_type & RESERVED_TYPE_INIT) |
        (reserved_type & RESERVED_TYPE_BIOS) |
        (reserved_type & RESERVED_TYPE_IVT) |
        (reserved_type & RESERVED_TYPE_VGA)) {
        pmm_set_frame_bitmap(0x0, 0x100000);

        debug_module(MEMORY, "[PMM] Reserved kernel range: 0x%u - 0x%u\n", 0,
                     100000);
    }

    //  reserve the kernel memory region
    if (reserved_type & RESERVED_TYPE_KERNEL) {
        //  We load the kernel at 0x100000, get it from the linker script
        //  reserve the memory used to load the kernel
        extern char kernel_start;
        extern char kernel_end;

        uint32_t kernel_memory_start = (uint32_t)&kernel_start;
        uint32_t kernel_memory_end = (uint32_t)&kernel_end;
        pmm_set_frame_bitmap(kernel_memory_start, kernel_memory_end);

        debug_module(MEMORY, "[PMM] Reserved kernel range: 0x%u - 0x%u\n",
                     PRINT_UINT32(kernel_memory_start),
                     PRINT_UINT32(kernel_memory_end));
    }

    //  reserve memory used by memory bitmap
    if (reserved_type & RESERVED_TYPE_BITMAP) {
        //  each frame - 1 bit -> max_frame_idx / 8 + 1 gives total bytes of
        //  bitmap
        uint32_t bitmap_bytes = (max_frame_idx / 8) + 1;
        uint32_t bitmap_start = (uint32_t)frame_bitmap;
        uint32_t bitmap_end = bitmap_start + bitmap_bytes;
        pmm_set_frame_bitmap(bitmap_start, bitmap_end);

        debug_module(MEMORY, "[PMM] Reserved bitmap: 0x%u - 0x%u (%u bytes)\n",
                     PRINT_UINT32(bitmap_start), PRINT_UINT32(bitmap_end),
                     PRINT_UINT32(bitmap_bytes));
    }

    //  reserve memory used by page tables
    if (reserved_type & RESERVED_TYPE_PAGE_TABLE) {
        //  PAGE_ENTRIES *
        //  Reserve page directory (4K at 0x80000)
        uint32_t page_dir_start = PAGE_DIR_START_ADDR;
        uint32_t page_dir_end =
            page_dir_start + PAGE_ENTRIES * sizeof(uint32_t);
        pmm_set_frame_bitmap(page_dir_start, page_dir_end);
        debug_module(MEMORY, "[PMM] Page Directory: 0x%u - 0x%u (%u bytes)\n",
                     PRINT_UINT32(page_dir_start), PRINT_UINT32(page_dir_end),
                     PRINT_UINT32(page_dir_end - page_dir_start));

        //  Reserve page table (4K at 0x81000)
        uint32_t page_table_start = PAGE_TABLE_START_ADDR;
        uint32_t page_table_end =
            page_table_start + PAGE_ENTRIES * sizeof(uint32_t);
        pmm_set_frame_bitmap(page_table_start, page_table_end);
        debug_module(MEMORY, "[PMM] Page Table: 0x%u - 0x%u (%u bytes)\n",
                     PRINT_UINT32(page_table_start),
                     PRINT_UINT32(page_table_end),
                     PRINT_UINT32(page_table_end - page_table_start));
    }

    debug_module(MEMORY, "Total usable frames: %u\n",
                 PRINT_UINT32(total_frames));
    debug_module(MEMORY, "Total reserved frames: %u\n",
                 PRINT_UINT32(used_frames));
    debug_module(MEMORY, "Free frames: %u\n",
                 PRINT_UINT32(total_frames - used_frames));
    debug_module(MEMORY, "Reserved memory regions: %u\n",
                 PRINT_UINT32(reserved_type));
}

//
//  Given a start and end address, set the corresponding frames in the bitmap as
//  used
//
void pmm_set_frame_bitmap(phys_addr_t start_address, phys_addr_t end_address) {
    //  round down the start address to k*PAGE_SIZE
    //  bit manipulation to unset all bits below PAGE_SIZE
    phys_addr_t start = start_address & ~(PAGE_SIZE - 1);

    //  round up the end address to k*PAGE_SIZE
    phys_addr_t end = end_address + PAGE_SIZE - 1;
    end &= ~(PAGE_SIZE - 1);

    for (phys_addr_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t frame_index = FRAME_INDEX(addr);
        if (!BITMAP_GET(frame_index)) {
            BITMAP_SET(frame_index);
            used_frames++;
        }
    }
}

phys_addr_t pmm_alloc_frames_v1(uint32_t count) {
    if (count == 0) {
        return 0;
    }

    //  Start from frame 1 to avoid allocating frame 0 (address 0x0)
    //  often reserved by BIOS.
    for (uint32_t frame_idx = 1; frame_idx <= max_frame_idx; frame_idx++) {
        uint32_t start_idx = frame_idx;
        uint32_t end_idx = frame_idx + count - 1;
        if (end_idx > max_frame_idx) {
            break;
        }

        int all_free = 1;
        for (uint32_t idx = start_idx; idx <= end_idx; idx++) {
            if (BITMAP_GET(idx)) {
                all_free = 0;
                break;
            }
        }

        if (!all_free) {
            continue;
        }

        for (uint32_t idx = start_idx; idx <= end_idx; idx++) {
            BITMAP_SET(idx);
        }

        return FRAME_INDEX_TO_PHYS_ADDR(start_idx);
    }

    debug_module(MEMORY, "[PMM] No free frames available!\n");
    return 0;
}

phys_addr_t pmm_alloc_frame(void) { return pmm_alloc_frames_v1(1); }

void pmm_free_frames_v1(phys_addr_t base, uint32_t count) {
    if (count == 0) {
        return;
    }

    /* Check if frame is aligned */
    if (!(base & (PAGE_SIZE - 1))) {
        panik("pmm_free_frames_v1: unaligned base address");
    }

    uint32_t start_idx = FRAME_INDEX(base);
    uint32_t end_idx = start_idx + count - 1;

    if (start_idx < 1 || end_idx > max_frame_idx) {
        panik("pmm_free_frames_v1: frame range is out of bounds");
    }

    for (uint32_t idx = start_idx; idx <= end_idx; idx++) {
        if (!BITMAP_GET(idx)) {
            panik("pmm_free_frames_v1: double free or freeing unallocated "
                  "frames");
        }
    }

    for (uint32_t idx = start_idx; idx <= end_idx; idx++) {
        BITMAP_CLEAR(idx);
    }
}

void pmm_free_frame(phys_addr_t base) { pmm_free_frames_v1(base, 1); }
