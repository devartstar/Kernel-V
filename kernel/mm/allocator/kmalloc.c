#include "mm/kmalloc.h"
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
        return NULL;
    }

    /* todo: for very large memory fallback to other tier */

    /* allocate a slot using kmem_slab */
    return kmem_slab_alloc(class_idx);
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
        /* todo: large memory block freed */
        /* todo: corruption or double free as memory ref. is not pointing to
         * correct page descritor reference */
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
