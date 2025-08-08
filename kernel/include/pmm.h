#pragma once

#include <stdint.h>
#include "memory_map.h"

#define PAGE_SIZE 4096

// pmm - process memory management utilities
void pmm_init(void);
void pmm_reserve_memory_region(reserved_memory_type_t reserved_type);
void* pmm_alloc_frame(void);
void pmm_free_frame(void* addr);