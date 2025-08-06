#pragma once

#include <stdint.h>

#define PAGE_SIZE 4096

// pmm - process memory management utilities
void pmm_init(void);
void pmm_reserve_memory_region(void);
void* pmm_alloc_frame(void);
void pmm_free_frame(void* addr);