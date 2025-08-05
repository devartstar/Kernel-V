#include "pmm.h"
#include "memory_map.h"
#include "printk.h"

static uint8_t frame_bitmap[FRAME_BITMAP_SIZE];
static uint32_t total_frames = 0;
static uint32_t used_frames = 0;

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

    for (int frame = 0; frame < FRAME_BITMAP_SIZE; frame++)
    {
        frame_bitmap[frame] = 0xFF;
    }

    total_frames = 0;
    used_frames = 0;

    for (uint32_t usable_memory_region_idx = 0; usable_memory_region_idx < usable_memory_region_count; usable_memory_region_idx++)
    {
        // base and length of the usable memory region
        uint32_t base = usable_memory_region[usable_memory_region_idx].base;
        uint32_t length = usable_memory_region[usable_memory_region_idx].length;

        // iterate through the usable memory region
        for(uint32_t memory_addr = base; memory_addr < base + length; memory_addr += PAGE_SIZE)
        {
            uint32_t frame_index = FRAME_INDEX(memory_addr);
            if (frame_index < MAX_FRAMES)
            {
                BITMAP_CLEAR(frame_index);
                total_frames++;
            }
        }
    }

    // initially none of the usable frames are used
    used_frames = 0;
    printk("[PMM] Total Usable Frames: %u\n", total_frames);

}

void* pmm_alloc_frame (void)
{
    for (uint32_t frame_idx = 0; frame_idx < total_frames; frame_idx++)
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