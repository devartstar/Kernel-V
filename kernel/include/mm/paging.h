#pragma once

#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024

#define PAGE_DIR_START_ADDR 0x80000
#define PAGE_TABLE_START_ADDR 0x81000

#define PAGE_PRESENT 0x1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4

#define PD_INDEX(x) (((x) >> 22) & 0x3FF)
#define PT_INDEX(x) (((x) >> 12) & 0x3FF)
#define PAGE_ALIGN(x) ((x)&0xFFFFFF000)
#define PAGE_ALIGN_DOWN(x) ((x)&0xFFFFF000)
#define PAGE_ALIGN_UP(x) (((x) + 0xFFF) & 0xFFFFF000)

void paging_init();

void paging_map_page(uint32_t virtual_addr, uint32_t physical_addr,
                     uint32_t flags);

void debug_dump_pte(uint32_t virtual_addr);

uint32_t paging_get_physical_address(uint32_t virt);

void paging_unmap_page(uint32_t virt);

void paging_free_region(uint32_t start, uint32_t size);
