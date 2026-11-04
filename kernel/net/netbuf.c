#include "net/netbuf.h"
#include "arch/x86/interrupt.h"
#include "lib/printk.h"
#include "mm/dma.h"
#include "mm/paging.h"

/* kernel low 4MiB identity map */
#define IDENTITY_LIMIT 0x00400000u

/* memory pool for buffer allocation */
static dma_allocation_t g_dma;
static netbuf_t g_bufs[NETBUF_COUNT];

/* reference to the free network buffer slot */
static netbuf_t *g_free;

/* stores the stats of allocaiton */
static netbuf_stats_t g_stats;

void netbuf_pool_init(void) {
    /* size of the buffer pool to be allocated */
    size_t bytes = NETBUF_COUNT * NETBUF_SLOT_SIZE;

    /* Number of pages to be allocated in the physical memory */
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    /* allocate physical memory */
    if (dma_alloc_pages(pages, &g_dma) != 0) {
        /* failed to allocate physical memory */
        panik("netbuf: DMA pool allocation fialed.\n");
    }

    /* memory is allocated from the lower half of the kernel, where physical to
     * virtual memory map is identiy. validate for the entire range */
    if (g_dma.phys + g_dma.size_bytes > IDENTITY_LIMIT) {
        /* memory allocation exceeded the lower half */
        panik("netbuf: pool crosses identity-map limit. physical addr %u and "
              "size %u exceeds 0x400000\n",
              PRINT_UINT32(g_dma.phys), PRINT_UINT32(g_dma.size_bytes));
    }

    g_free = NULL;

    /* Initialize ref to the @g_dma memory allocated to the network buffer pool
     * @g_buf. Initialize from end to first. */
    /* Use a post-decrement test: i is unsigned, so `i >= 0` would always be
     * true and would wrap to UINT32_MAX after slot zero. */
    for (uint32_t i = NETBUF_COUNT; i-- > 0;) {
        netbuf_t *buf = &g_bufs[i];

        buf->head = (uint8_t *)g_dma.kva + i * NETBUF_SLOT_SIZE;
        buf->end = buf->head + NETBUF_SLOT_SIZE;

        buf->head_phys = g_dma.phys + i * NETBUF_SLOT_SIZE;

        buf->data = buf->tail = buf->head;

        buf->index = (uint16_t)i;
        buf->refcnt = 0;
        buf->next = g_free;
        g_free = buf;
    }

    g_stats.free = g_stats.min_free = NETBUF_COUNT;
}

netbuf_t *netbuf_alloc(uint32_t headroom) {
    /* check: headroom should not exceed the SLOT_SIZE */
    if (headroom > NETBUF_SLOT_SIZE) {
        panik("netbuf_alloc: headroom exceed slot size.\n");
    }

    /* start critical section */
    irq_flags_t flags = irq_save();

    /* get the first free slot.
     * Already allocated, just referencing in the critical section
     */
    netbuf_t *buf = g_free;
    if (!buf) {
        g_stats.alloc_fail++;
        irq_restore(flags);
        return NULL;
    }

    /* update the fee buffer pointer */
    g_free = buf->next;
    g_stats.allocs++;
    if (--g_stats.free < g_stats.min_free) {
        g_stats.min_free = g_stats.free;
    }

    /* end critical section */
    irq_restore(flags);

    /* update the allocated buffer */
    buf->next = NULL;
    buf->refcnt = 1;
    buf->meta = 0;
    buf->data = buf->tail = buf->head + headroom;

    return buf;
}

void netbuf_hold(netbuf_t *buff) {
    /* start critical section */
    irq_flags_t flags = irq_save();

    /* check if the buffer is already freed */
    if (buff->refcnt == 0) {
        panik("netbuf_hold: buffer is free.\n");
    }

    /* hold a reference to the buffer */
    buff->refcnt++;

    /* end critical section */
    irq_restore(flags);
}

void netbuf_release(netbuf_t *buf) {
    /* start critical section */
    irq_flags_t flags = irq_save();

    /* check if the buffer is already freed */
    if (buf->refcnt == 0) {
        panik("netbuf_release: buffer is already released.\n");
    }

    /* release the reference and handle cases */
    if (--buf->refcnt == 0) {
        /* add the buffer back to the pool (tail of the queue)
         * g_free points to first allocatable entry in queue. */
        buf->next = g_free;
        g_free = buf;

        g_stats.free++;
        g_stats.frees++;
    }

    /* end critical section */
    irq_restore(flags);
}

uint8_t *netbuf_push(netbuf_t *buf, uint32_t len) {
    /* check the bytes between slot start and data startis enough */
    if (len > netbuf_headroom(buf)) {
        panik("netbuf_push: headroom underrun.\n");
    }

    /* update the start of the data */
    buf->data -= len;

    return buf->data;
}

uint8_t *netbuf_put(netbuf_t *buf, uint32_t len) {
    /* checks if the bytes between data end and slot end is enough */
    if (len > netbuf_tailroom(buf)) {
        panik("netbuf_put: tailroom overrun.\n");
    }

    /* get the first available bytes */
    uint8_t *p = buf->tail;

    /* update the new first available bytes to write data */
    buf->tail += len;

    return p;
}

uint8_t *netbuf_pull(netbuf_t *buf, uint32_t len) {
    /* length of data to pull is greater than available */
    if (len > netbuf_len(buf)) {
        return NULL;
    }

    /* update the data start after the pull data of length @len */
    uint8_t *hdr = buf->data;
    buf->data += len;

    return hdr;
}

bool netbuf_trim(netbuf_t *buf, uint32_t len) {
    /* length to trim to is  freater than current length */
    if (len > netbuf_len(buf)) {
        return false;
    }

    /* update the first available byte after data to discard rest */
    buf->tail = buf->data + len;
    return true;
}
