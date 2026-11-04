#ifndef NETBUF_H
#define NETBUF_H

#include "types/address.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 1514 frame + virtio hdr + kernelv header */
#define NETBUF_SLOT_SIZE 2048u

/* 128 KiB pool; must stay in lower half of kernel < 4MiB */
#define NETBUF_COUNT 64u

/* ======= Data Structure ======= */

/* @netbf_t holds the network buffer layout
 * @head - ref to the start of the slot
 * @data - ref to the first valid byte
 * @tail - ref to the first invalid byte
 * @end - ref to the end ofthe slot
 * @head_phys - ref to the device visible address of the head
 * @next - ref to the next entry in the network buffer list
 * @refcnt - count of the number of references of buffer in slot
 * @index - slot number
 * @meta - owner defined flags
 * */
typedef struct netbuf {
    uint8_t *head;
    uint8_t *data;
    uint8_t *tail;
    uint8_t *end;
    phys_addr_t head_phys;
    struct netbuf *next;
    uint16_t refcnt;
    uint16_t index;
    uint32_t meta;
} netbuf_t;

/**
 * @netbuf_stats_t stores the statistics of buffer availability
 */
typedef struct netbuf_stats {
    uint32_t free, min_free;
    uint32_t allocs, frees, alloc_fail;
} netbuf_stats_t;

/**
 * @netbuf_queue FIFO list of network buffer slots
 */
typedef struct netbuf_queue {
    netbuf_t *head, *tail;
    uint32_t len;
} netbuf_queue_t;

/** ===== Allocation Utilities ======= */

/**
 * @netbuf_pool_init initializes the memory pool for network buffer and assign
 * ref to the memory for each slot
 */
void netbuf_pool_init(void);

/**
 * @netbuf_alloc to allocate a network buffer from the pool (queue)
 * @headroom provides space before for the data starts in buffer.
 *
 * @returns the reference to the buffer allocated
 */
netbuf_t *netbuf_alloc(uint32_t headroom);

/**
 * @netbuf_hold takes reference to a network buffer
 */
void netbuf_hold(netbuf_t *buf);

/**
 * @netbuf_release releases the reference count on the buffer and if no more
 * references then add the buffer back to the queue
 */
void netbuf_release(netbuf_t *buf);

void netbuf_get_stats(netbuf_stats_t *stats);

/* ======= Buffer Geometry Utilities ======== */

/**
 * @netbuf_len return the buffer valid data size
 */
static inline uint32_t netbuf_len(const netbuf_t *buf) {
    return buf->tail - buf->data;
}

/**
 * @netbuf_headroom returns the buffer headroom before first valid data
 */
static inline uint32_t netbuf_headroom(const netbuf_t *buf) {
    return buf->data - buf->head;
}

/**
 * @netbuf_tailroom returns the buffer headroom after last valid data
 */
static inline uint32_t netbuf_tailroom(const netbuf_t *buf) {
    return buf->end - buf->tail;
}

static inline phys_addr_t netbuf_phys(const netbuf_t *buf, const uint8_t *p) {
    return buf->head_phys + (phys_addr_t)(p - buf->head);
}

/* ======== Buffer Builders ========= */

/**
 * @netbuf_push check if enough headroom to push @len length of data
 * headroom is the available bytes between start of slot and data.
 * @returns the ref to start writing the data of @len - prepend
 */
uint8_t *netbuf_push(netbuf_t *buf, uint32_t len);

/**
 * @netbuf_put checks if enough tailroom to write @len of data at end
 * tailroom is the avaialble bytes between the end of data and slot
 * @returns the ref to start writing the data of @len - append
 */
uint8_t *netbuf_put(netbuf_t *buf, uint32_t len);

/* ======== Buffer Parsers ========= */

/**
 * @netbuf_pull validates gets the ref from where to start pulling the data of
 * @len from buffer @buf
 */
uint8_t *netbuf_pull(netbuf_t *buf, uint32_t len);

/**
 * @netbuf_trim keeps only the first @len bits of data
 * @returns true when trimming was done else return false.
 */
bool netbuf_trim(netbuf_t *buf, uint32_t len);

/* ========= Buffer List ============ */

static inline void netbuf_queue_init(netbuf_queue_t *q) {
    q->head = q->tail = NULL;
    q->len = 0;
}

/**
 * @netbuf_enqueue enqueues the network buffer to the end of the list, the last
 * entry of the list references itself
 */
static inline void netbuf_enqueue(netbuf_queue_t *q, netbuf_t *buf) {
    buf->next = NULL;
    if (q->tail) {
        q->tail->next = buf;
    } else {
        q->head = buf;
    }

    q->tail = buf;
    q->len++;
}

/**
 * @netbuf_dequeue returns the first entry in the queue and updates the queue
 * accordingly
 */
static inline netbuf_t *netbuf_dequeue(netbuf_queue_t *q) {
    /* returns the first entry in the queue */
    netbuf_t *buf = q->head;
    if (!buf) {
        return NULL;
    }

    /* head of the queue to point to the next entry */
    q->head = buf->next;
    if (!q->head) {
        /* we removed the last entry from the queue */
        q->tail = NULL;
    }

    /* detach the entry from the queue */
    buf->next = NULL;

    q->len--;

    return buf;
}

#endif /* NETBUF_H */
