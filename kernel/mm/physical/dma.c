#include "mm/dma.h"
#include "lib/string.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include <string.h>

int dma_alloc_pages(size_t page_count, dma_allocation_t *out) {
    if (!out || page_count == 0) {
        return -1;
    }

    /* Allocate physical memory for DMA entry */
    phys_addr_t phys = pmm_alloc_frames_v1(page_count);
    if (!phys) {
        out->kva = NULL;
        out->phys = 0;
        out->page_count = 0;
        out->size_bytes = 0;
        return -1;
    }

    /* Map the physical address to the virtual */
    void *kva = phys_to_virt_identity(phys);
    out->kva = kva;
    out->phys = phys;
    out->page_count = page_count;
    out->size_bytes = page_count * PAGE_SIZE;

    /* Assign 0 to the the allocated range */
    memset(out->kva, 0, out->size_bytes);
    return 0;
}
