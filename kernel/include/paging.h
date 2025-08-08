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