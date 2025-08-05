#pragma once

#include <stdint.h>

#define PAGE_SIZE 4096

// At Max assume 128 MiB of RAM. Each Frame is 4KiB.
#define MAX_FRAMES 32768

// Bitmap to represent allocated frames. 1 bit per frame.
#define FRAME_BITMAP_SIZE (MAX_FRAMES / 8)

// pmm - process memory management utilities
void pmm_init(void);
void* pmm_alloc_frame(void);
void pmm_free_frame(void* addr);