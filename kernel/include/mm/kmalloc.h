#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

/* --- TIERS (Only SLAB + LARGE implemented for now) --- */
typedef enum {
    KMEM_TIER_SLAB = 1,
    KMEM_TIER_LARGE = 2,
} kmem_tier_t;

/* Largest object served by slab tier. Requests above this goes to T3 */
#define KMEM_SLAB_MAX 2048u

/* Minimum alignment gaurantee to every allocation (fundamental align = 32b) */
#define KMEM_ALIGN 8u

/* Magic stamped into every slabs' page descriptor */
#define KMEM_SLAB_MAGIC 0x4B534C42 /* 'K', 'S', 'L', 'B' */

/* the header bytes reserved at the page base for the descriptor.
 * bytes aligned to every object slot remains KMEM_ALIGN */
#define KMEM_SLAB_HDR                                                          \
    ((sizeof(kmem_page_desc_t) + KMEM_ALIGN - 1) & ~((size_t)KMEM_ALIGN - 1))

/* Extract the page descriptor from the ref to the base of Page */
#define KMEM_PAGE_OF(ptr)                                                      \
    ((kmem_page_desc_t *)((uintptr_t)(ptr) & ~(uniqueptr *)0xFFF))

/* ===== PAGE DESCRIPTOR ====== */
/** NOTES:
 * 1. Each Page descriptor will contain a free_list along with in_use count.
 * 2. free_list = NULL when the page is full
 * 3. page in partial_list when it has available slot & in_use > 0
 */

/**
 * Per Page Descriptor, stored at base of every 4KB slab page.
 * used while kfree(ptr): desc = (kmem_page_descriptor *)((uintptr_t)ptr &
 * ~0xFFFu) The low 12 bits of the page is the page descriptor. This is the
 * unified O(1) pointer->metadata resolution: no per-object header, no free-list
 * search on free
 *
 * @magic - KMEM_MAGIC_SLAB - validates is the page is a live slab
 * @tier - kmem_tier_t
 * @class_idx - index into the size-class table
 * @obj_size - bytes per slot on this page (=class_size)
 * @in_use - live objects on this page
 * @free_list - lists free slots per page
 * @next/@prev - links in cache partial list
 */
typedef struct kmem_page_desc {
    uint32_t magic;
    uint16_t tier;
    uint16_t class_idx;
    uint32_t obj_size;
    uint32_t in_use;
    void *free_list;
    struct kmem_page_desc *next;
    struct kmem_page_desc *prev;
} kmem_page_desc_t;

/* Size class introspection */
extern const uint32_t g_kmem_num_classes;

/**
 * @kmem_size_to_class - retuens the class index for a given size of allocation,
 * or -1 -> KMEM_SLAB_MAX
 */
int kmem_size_to_class(size_t size);

/**
 * @kmem_class_size - returns the class size for a given class index
 */
uint32_t kmem_class_size(int class_idx);

/* ===== PAGE CACHE ====== */
/** NOTES:
 * 1. Cache tracks a partial list (ie. pages >= 1 free slot)
 * 2. Page transitions between 3 states all in O(1)
 *
 *               list = cache list
 *
 *           ┌─────────┐alloc ┌──────────┐
 *   grow    │PARTIAL  ├─────►│ FULL     │
 * ─────────►│(on list)│      │(off list)│
 *           └┬────────┘◄─────┴──────────┘
 *            │           free
 *            │in_use=0
 *            │free
 *         ┌──▼───┐
 *         │EMPTY ├─────────► free frame
 *         └──────┘
 */

/**
 * kmem_cache - cache of the per class state
 *
 * @partial - list of pages which are partially available to allocation
 * @obj_size - cache for the slab size - by g_size_classes[idx]
 * @pages - count of pages in current slab
 * count of free slots across partial pages
 */
typedef struct kmem_cache {
    kmem_page_desc_t *partial;
    uint32_t obj_size;
    uint32_t pages;
    uint32_t free_objs;
} kmem_chache_t;

/* ===== PUBLIC APIS ====== */
void kmalloc_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t new_size);

#endif /* KMALLOC_H */
