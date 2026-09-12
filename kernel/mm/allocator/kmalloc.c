#include "mm/kmalloc.h"
#include "lib/string.h"
#include "mm/pmm.h"

/** Quater spaces sizes
 * classes of memory allocation; every class is a multiple of KMEM_ALIGN(8)
 * so every allocation is 8 bit aligned. Four subdivisions per octave bounds the
 * worst case internal fragmentation to ~25% (inprovement from ~50% when size of
 * classes are power of two)
 */
static const uint16_t g_size_classes[] = {
    8,   16,  24,  32,  48,  64,  80,  96,  112,  128,  160,  192,  224,
    256, 320, 384, 448, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048,
};

const uint32_t g_kmem_num_classes =
    sizeof(g_size_classes) / sizeof(g_size_classes[0]);

/** Size to Class Lookup
 * Worst case lookup time is O(1) for size->class.
 * Largest slab object is of size KMEM_SLAB_MAX, and slabs are 8 byte aligned.
 * So maximum index of the class is KMEM_SLAB_MAX / KMEM_ALIGN = 256
 */
#define KMEM_LOOKUP_ENTRIES (KMEM_SLAB_MAX / KMEM_ALIGN + 1)

#define KMEM_NUM_CLASSES 26
_Static_assert(sizeof(g_size_classes) / sizeof(g_size_classes[0]) ==
                   KMEM_NUM_CLASSES,
               "KMEM_NUM_CLASSES out of sync with g_csize_classes");

/* the header bytes reserved at the page base for the descriptor.
 * bytes aligned to every object slot remains KMEM_ALIGN */
#define KMEM_SLAB_HDR                                                          \
    ((sizeof(kmem_page_desc_t) + KMEM_ALIGN - 1) & ~((size_t)KMEM_ALIGN - 1))

/* Max usable bytes in one large frame (after in-page descriptor) = 4096KB */
#define KMEM_LARGE_MAX ((uint32_t)(PAGE_SIZE - KMEM_SLAB_HDR))

/* Extract the page descriptor from the ref to the base of Page */
#define KMEM_PAGE_OF(ptr)                                                      \
    ((kmem_page_desc_t *)((uintptr_t)(ptr) & ~(uintptr_t)0xFFF))

/** for a particular class index this stores the size of the slab */
static uint8_t g_size8_to_class[KMEM_LOOKUP_ENTRIES];

/** cache list storing free_list of each class of slub */
static kmem_chache_t g_kmem_caches[KMEM_NUM_CLASSES];

/** Build up the slab index to size */
static void kmem_build_lookup(void) {
    uint32_t cls = 0;
    for (uint32_t i = 0; i < KMEM_LOOKUP_ENTRIES; i++) {
        uint32_t bytes = i * KMEM_ALIGN;
        while (cls < g_kmem_num_classes && g_size_classes[cls] < bytes) {
            cls++;
        }

        /* assign first class >= bytes */
        g_size8_to_class[i] = (uint8_t)cls;
    }
}

int kmem_size_to_class(size_t size) {
    if (size == 0) {
        size = 1;
    }

    if (size > KMEM_SLAB_MAX) {
        return -1;
    }

    uint32_t idx = (uint32_t)((size + KMEM_ALIGN - 1) / KMEM_ALIGN);
    return (int)g_size8_to_class[idx];
}

uint32_t kmem_class_size(int class_idx) {
    if (class_idx < 0 || (uint32_t)class_idx >= g_kmem_num_classes) {
        return 0;
    }

    return g_size_classes[class_idx];
}

/**
 * kmem_slots_per_page - returns the number of available slots per page
 * @obj_size - size of each available slot
 */
static inline uint32_t kmem_slots_per_page(uint32_t obj_size) {
    return (PAGE_SIZE - (uint32_t)KMEM_SLAB_HDR) / obj_size;
}

/**
 * kmem_partial_add - adds a page to the start of the cache's partial list
 */
static void kmem_partial_add(kmem_chache_t *cache, kmem_page_desc_t *desc) {
    if (desc == NULL || cache == NULL) {
        return;
    }

    /* add the descriptor to the head of the free list */
    desc->prev = NULL;
    desc->next = cache->partial;

    if (cache->partial) {
        cache->partial->prev = desc;
    }

    cache->partial = desc;
}

/**
 * kmem_partial_remove - removes gicen page ref from the cache partial list
 * donw when the page descriptor in_use count becomes 0
 */
static void kmem_partial_remove(kmem_chache_t *cache, kmem_page_desc_t *desc) {
    /* fix forward reference */
    if (desc->prev) {
        /* page in middle of partial list */
        desc->prev->next = desc->next;
    } else {
        /* page in start of partial list */
        cache->partial = desc->next;
    }

    /* fix backward reference */
    if (desc->next) {
        desc->next->prev = desc->prev;
    }

    /* remove desc from partial list */
    desc->next = desc->prev = NULL;
}

/* kmem_cache_grow - grows the cache for slab referenced by class_idx by one pmm
 * frame */
static int kmem_cache_grow(kmem_chache_t *cache, uint32_t class_idx) {
    void *page = pmm_alloc_frame();
    if (!page)
        return -1;

    /* update the first few bytes of the page with page descriptor */
    kmem_page_desc_t *desc = (kmem_page_desc_t *)page;
    desc->magic = KMEM_SLAB_MAGIC;
    desc->tier = KMEM_TIER_SLAB;
    desc->class_idx = class_idx;
    desc->obj_size = cache->obj_size;
    desc->in_use = 0;
    desc->free_list = NULL;

    /* update base with offset to the start of slots */
    uint8_t *base = (uint8_t *)page + KMEM_SLAB_HDR;
    uint32_t total_slots = kmem_slots_per_page(cache->obj_size);

    /* add all the slots to the free list of page descriptor */
    for (uint32_t i = 0; i < total_slots; i++) {
        void *slot = base + i * cache->obj_size;
        /* add the stop to the head of free list */
        *(void **)slot = desc->free_list;
        desc->free_list = slot;
    }

    /* update the page descriptor to the cache partial list sice it has
     * available slots now */
    kmem_partial_add(cache, desc);
    cache->pages++;
    cache->free_objs += total_slots;
    return 0;
}

/**
 * kmem_slab_alloc - allocate memory from page in the caches partial head
 * if after allocation page has no available stop drop it from partial list
 */
static void *kmem_slab_alloc(int class_idx) {
    /* get the cache based on the class index */
    kmem_chache_t *cache = &g_kmem_caches[class_idx];

    /* verify if the cache has non-null partial list */
    if (!cache->partial) {
        /* then add a page to the cache of this class */
        if (kmem_cache_grow(cache, (uint32_t)class_idx) != 0) {
            /* cannot allocate a page to the cache */
            return NULL;
        }
    }

    /* cache has pages to allocate memory from */

    /* we will allocate from head of partial list */
    kmem_page_desc_t *desc = cache->partial;

    /* list of slots available for allocation in that page
     * allocate from the head of free list */
    void *obj = desc->free_list;
    desc->free_list = *(void **)obj;

    /* update other metadata in descriptor */
    desc->in_use++;
    cache->free_objs--;

    /* if page has no free slots available them remove page from cache partial
     * list */
    if (desc->free_list == NULL) {
        kmem_partial_remove(cache, desc);
    }

    return obj;
}

/**
 * kmem_slab_free - frees previously acquired memory slot back to the page free
 * list.
 */
static void kmem_slab_free(void *ptr, kmem_page_desc_t *desc) {
    /* get the correct cache for the associated class which page descriptor
     * allocated */
    kmem_chache_t *cache = &g_kmem_caches[desc->class_idx];

    /* check if no free slot in the page descriptor */
    int was_full = (desc->free_list == NULL);

    /* reclaim the slot back to the head of the page free_list */
    *(void **)ptr = desc->free_list;
    desc->free_list = ptr;

    /* update the page descriptor metadata */
    desc->in_use--;
    cache->free_objs++;

    /* if the page was full before we would have removed it from the cache
     * partial list, now with page reclaiming back the slot has entry in free
     * list so add page back to cahce partial list */
    if (was_full) {
        kmem_partial_add(cache, desc);
    }

    /* if the page has acquired back all its memory - ie. free_list is full
     * hand the page back. remove page from the cache partial entry.
     * why? page is as good as never touched - if needed some page can be added
     * to the cache partial entry later when we try to allocate memory */
    if (desc->in_use == 0) {
        kmem_partial_remove(cache, desc);
        cache->free_objs -= kmem_slots_per_page(desc->obj_size);
        cache->pages--;
        pmm_free_frame(desc);
    }
}

/**
 * kmem_large_alloc - allocates one whole frame for a > 2048 memory allocation
 * request. The frame carries a KMEM_TIER_LARGE in the page descriptor so
 * kfree/kmalloc can resolve it in O(1) way (KMEM_PAGE_OF). obj_size stores the
 * ACTUAL request so krealloc know how much to copy */
static void *kmem_large_alloc(size_t size) {
    /* memory allocation request cannot be greater than the max allocatable
     * memory per frame */
    if (size > KMEM_LARGE_MAX) {
        return NULL;
    }

    /* allocate a page */
    void *page = pmm_alloc_frame();
    if (!page) {
        return NULL;
    }

    /* update the first fre bytes with the page descriptor object */
    kmem_page_desc_t *desc = (kmem_page_desc_t *)page;
    desc->magic = KMEM_SLAB_MAGIC;
    desc->tier = KMEM_TIER_LARGE;
    /* Note. class_idx is not used for large memory allocation */
    desc->class_idx = 0;
    desc->obj_size = (uint32_t)size;
    desc->in_use = 1;
    desc->free_list = NULL;
    desc->next = desc->prev = NULL;

    return page;
}

/* ======= PUBLIC APIs + INIT ======= */

/**
 * kmalloc - allocate a memoty block of size
 * @return back the starting ref to the memory block
 */
void *kmalloc(size_t size) {
    /* give an memory of required size - get the class from which it should be
     * allocated */
    int class_idx = kmem_size_to_class(size);
    if (class_idx < 0) {
        /* size > 2KB - T3 slab */
        return kmem_large_alloc(size);
    }
    /* T1 slab: allocate a slot using kmem_slab */
    return kmem_slab_alloc(class_idx);
}

/**
 * kcalloc - allocates n number of objects each of size and zero it out.
 * @returns the reference to the zeroed memory block
 */
void *kcalloc(size_t n, size_t size) {
    size_t total = n * size;
    if (n != 0 && total / n != size) {
        /* multiplication overflow */
        return NULL;
    }

    /* allocate memory of size total */
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }

    return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) {
        /* realloc(NULL, size) = kmalloc(size) */
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        /* realloc(ptr, 0) = kfree(ptr) */
        kfree(ptr);
        return NULL;
    }

    /* given a poinyer locate the base of the page */
    kmem_page_desc_t *desc = KMEM_PAGE_OF(ptr);

    /* check if the page is valid */
    if (desc->magic != KMEM_SLAB_MAGIC) {
        return NULL;
    }

    /* usable capacity of the block */
    uint32_t current_cap;
    if (desc->tier == KMEM_TIER_LARGE) {
        current_cap = KMEM_LARGE_MAX;
    } else {
        current_cap = desc->obj_size;
    }

    /* if the usable capacity in the current block can be extended */
    if (new_size <= current_cap) {
        desc->obj_size = new_size;
        return ptr;
    }

    /* need to allocate a new memory block */
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        /* Allocation failed */
        return NULL;
    }

    /* copy contents to the new block */
    uint32_t copy_size =
        (new_size < desc->obj_size) ? (uint32_t)new_size : desc->obj_size;
    memcpy(new_ptr, ptr, copy_size);

    /* free the previously allocated memory */
    kfree(ptr);

    return new_ptr;
}

/**
 * kfree - freems memory back to the slab
 */
void kfree(void *ptr) {
    if (!ptr) {
        return;
    }

    kmem_page_desc_t *desc = KMEM_PAGE_OF(ptr);
    if (desc->magic != KMEM_SLAB_MAGIC) {
        /* todo: corruption or double free as memory ref. is not pointing to
         * correct page descritor reference */
        return;
    }

    if (desc->tier == KMEM_TIER_LARGE) {
        pmm_free_frame(ptr);
        return;
    }

    /* free up the slot */
    kmem_slab_free(ptr, desc);
}

void kmalloc_init(void) {
    /* initialize the size to class mapping */
    kmem_build_lookup();

    /* initialize the slab class caches */
    for (uint32_t i = 0; i < KMEM_NUM_CLASSES; i++) {
        g_kmem_caches[i].partial = NULL;
        g_kmem_caches[i].obj_size = g_size_classes[i];
        g_kmem_caches[i].pages = 0;
        g_kmem_caches[i].free_objs = 0;
    }
}
