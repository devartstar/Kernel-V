#pragma once

#include <stdint.h>

#define PAGE_SIZE       4096
#define PAGE_ENTRIES    1024

#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4

void paging_init();