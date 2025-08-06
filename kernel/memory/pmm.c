#include "pmm.h"
#include "memory_map.h"
#include "printk.h"

static uint8_t* frame_bitmap = NULL;;
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;
static uint32_t max_frame_idx = 0;

// frame_bitmap[x] -> byte entry (8 bits) => array_index = bitmap_index / 8 
// now within that 8 bits -> need to update the correct bit at (bitmap_index % 8)
#define FRAME_INDEX(addr)   ((addr) / PAGE_SIZE)
#define BITMAP_SET(idx)     (frame_bitmap[(idx) / 8] |=  (1 << ((idx) % 8)))
#define BITMAP_CLEAR(idx)   (frame_bitmap[(idx) / 8] &= ~(1 << ((idx) % 8)))
#define BITMAP_GET(idx)     (frame_bitmap[(idx) / 8] &   (1 << ((idx) % 8)))

//
// Initialize the entire bitmap to 1 (used)
// Iterate through all the usable memory regions
// and mark the frames as free in the bitmap
//
void pmm_init(void)
{
    total_frames = 0;
    used_frames = 0;

    // Step1: Calculate the maximum frame index based on the usable memory regions
    for (uint32_t usable_memory_region_idx = 0; usable_memory_region_idx < usable_memory_region_count; usable_memory_region_idx++)
    {
        // base and length of the usable memory region
        uint32_t region_base = usable_memory_region[usable_memory_region_idx].base;
        uint32_t region_length = usable_memory_region[usable_memory_region_idx].length;
        uint32_t region_end = region_base + region_length;
        uint32_t current_frame_idx = FRAME_INDEX(region_end);

        if (current_frame_idx > max_frame_idx)
        {
            max_frame_idx = current_frame_idx;
        }
    }

    // Step2: Allocate address for bitmap array in a safe memory region
    frame_bitmap = (uint8_t*)0x90000;
    uint32_t max_frame_bitmap_idx = (max_frame_idx / 8) + 1; 

    // Step3: Initialize the bitmap to 1 (all frames are used)
    for (uint32_t frame = 0; frame < max_frame_bitmap_idx; frame++)
    {
        frame_bitmap[frame] = 0xFF;
    }     

    // Step4: Iterate through the usable memory regions and mark frames as free
    for (uint32_t usable_memory_region_idx = 0; usable_memory_region_idx < usable_memory_region_count; usable_memory_region_idx++)
    {
        uint32_t region_base = usable_memory_region[usable_memory_region_idx].base;
        uint32_t region_length = usable_memory_region[usable_memory_region_idx].length;
        
        for (uint32_t addr = region_base; addr < region_base + region_length; addr += PAGE_SIZE)
        {
            uint32_t frame_index = FRAME_INDEX(addr);
            if (frame_index < max_frame_idx)
            {
                BITMAP_CLEAR(frame_index);
                total_frames++;
            }
        }
    }

    // initially none of the usable frames are used
    used_frames = 0;
    printk("[PMM] Total Usable Frames: %u\n", total_frames);

    pmm_reserve_memory_region();

}

void pmm_reserve_memory_region(void)
{
    // < 1Mib Reserve all memory below 1Mb for BIOS, IVT, VGA
    for (uint32_t addr = 0; addr < 0x100000; addr += PAGE_SIZE)
    {
        uint32_t frame_index = FRAME_INDEX(addr);
        if (!BITMAP_GET(frame_index))
        {
            BITMAP_SET(frame_index);
            used_frames++;
        }
    }

    // We load the kernel at 0x100000, get it from the linker script
    // reserve the memory used to load the kernel
    extern char kernel_start;
    extern char kernel_end;

    uint32_t kernel_memory_start = (uint32_t)&kernel_start;
    uint32_t kernel_memory_end   = (uint32_t)&kernel_end;

    for (uint32_t addr = kernel_memory_start; addr < kernel_memory_end; addr += PAGE_SIZE)
    {
        uint32_t frame_index = FRAME_INDEX(addr);
        if (!BITMAP_GET(frame_index))
        {
            BITMAP_SET(frame_index);
            used_frames++;
        }
    }

    // each frame - 1 bit -> max_frame_idx / 8 + 1 gives total bytes of bitmap
    uint32_t bitmap_bytes = (max_frame_idx / 8) + 1;
    // Reserve memory used by memory bitmap
    uint32_t bitmap_start = (uint32_t)frame_bitmap;
    uint32_t bitmap_end = bitmap_start + bitmap_bytes;
    for (uint32_t addr = bitmap_start; addr < bitmap_end; addr += PAGE_SIZE)
    {
        uint32_t frame_index = FRAME_INDEX(addr);
        if (!BITMAP_GET(frame_index))
        {
            BITMAP_SET(frame_index);
            used_frames++;
        }
    }

    printk("[PMM] Reserved kernel range: 0x%u - 0x%u\n", kernel_memory_start, kernel_memory_end);
    printk("[PMM] Reserved bitmap: 0x%u - 0x%u (%u bytes)\n", bitmap_start, bitmap_end, bitmap_bytes);
    printk("[PMM] Total usable frames: %u\n", total_frames);
    printk("[PMM] Total reserved frames: %u\n", used_frames);
}

void* pmm_alloc_frame (void)
{
    // Start from frame 1 to avoid allocating frame 0 (address 0x0)
    // often reserved by BIOS.
    for (uint32_t frame_idx = 1; frame_idx < total_frames; frame_idx++)
    {
        if (!BITMAP_GET(frame_idx))
        {
            BITMAP_SET(frame_idx);
            used_frames++;
            return (void*)(frame_idx * PAGE_SIZE);
        }
    }
    printk("[PMM] No free frames available!\n");
    return 0;
}

void pmm_free_frame (void* addr)
{
    uint32_t frame_idx = FRAME_INDEX((uint32_t)addr);
    if (frame_idx < total_frames)
    {
        BITMAP_CLEAR(frame_idx);
        used_frames--;
    }
    else
    {
        printk("[PMM] Attempted to free an invalid frame at address: %p\n", addr);
    }
}