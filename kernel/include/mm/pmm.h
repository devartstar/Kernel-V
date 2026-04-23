#pragma once

#include "mm/memory_map.h"
#include "types/address.h"
#include <stdint.h>

#define PAGE_SIZE 4096

//  Heap size = 16MB
#define KERNEL_HEAP_START 0xC1000000
#define KERNEL_HEAP_END 0xC2000000

//  Stack size = 64KB
//  stack grows downwards
//  ESP starts at KERNEL_STACK_TOP and goes down to KERNEL_STACK_BOTTOM
#define KERNEL_STACK_TOP_VIRT 0xC3000000
#define KERNEL_STACK_BOTTOM_VIRT                                               \
    (KERNEL_STACK_TOP_VIRT - 0x10000) //  0xc2FF0000

//  pmm - process memory management utilities
void pmm_init(void);
void pmm_reserve_memory_region(reserved_memory_type_t reserved_type);
void pmm_set_frame_bitmap(phys_addr_t start_address, phys_addr_t end_address);

/*
 * pmm_alloc_frame - allocates a free memory block of 4KB from the available
 * memory regions
 *
 * @return - base physical address of the frame returned
 */
phys_addr_t pmm_alloc_frame(void);

/**
 * pmm_free_frame - free the 4KB memory block
 * @frame - base physical address of the frame to free
 *
 * @return void
 */
void pmm_free_frame(phys_addr_t frame);
