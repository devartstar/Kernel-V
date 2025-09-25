#pragma once

#include <stdint.h>
#include "memory_map.h"

#define PAGE_SIZE           4096

// Heap size = 16MB
#define KERNEL_HEAP_START   0xC1000000
#define KERNEL_HEAP_END     0xC2000000

// Stack size = 64KB
// stack grows downwards
// ESP starts at KERNEL_STACK_TOP and goes down to KERNEL_STACK_BOTTOM
#define KERNEL_STACK_TOP_VIRT   0xC3000000
#define KERNEL_STACK_BOTTOM_VIRT (KERNEL_STACK_TOP_VIRT - 0x10000)

// pmm - process memory management utilities
void pmm_init(void);
void pmm_reserve_memory_region(reserved_memory_type_t reserved_type);
void pmm_set_frame_bitmap(uint32_t start_address, uint32_t end_address);
void* pmm_alloc_frame(void);
void pmm_free_frame(void* addr);