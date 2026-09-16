#pragma once

#include "types/address.h"
#include <stddef.h>

typedef struct dma_allocation {
    void *kva;         /* CPU - usable kernel virtual alias */
    phys_addr_t phys;  /* hardware usable physical address */
    size_t page_count; /* number of contigious pages */
    size_t size_bytes; /* page_count * PAGE_SIZE */
} dma_allocation_t;

/**
 * dma_alloc_pages - Allocate memory for an entry in DMA table
 * @page_count - Number of pages to allocate
 * @out - pointer to the structure holding the entry of DMA table
 *
 * @return - 0 for success and -1 for error
 */
int dma_alloc_pages(size_t page_count, dma_allocation_t *out);

void dma_free_pages(dma_allocation_t *alloc);
