#pragma once

#include <stdint.h>

#define PAGE_SIZE       4096
#define PAGE_ENTRIES    1024

#define PAGE_DIR_START_ADDR     0x80000
#define PAGE_TABLE_START_ADDR   0x81000

#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4

void paging_init();
// void page_fault_handler(); // do we need this ? dupplicate of page_fault.h
void paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void debug_page_tables();
