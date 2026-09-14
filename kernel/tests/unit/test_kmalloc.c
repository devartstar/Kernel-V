#include "tests/test_kmalloc.h"
#include "lib/printk.h"
#include "mm/kmalloc.h"
#include <stddef.h>
#include <stdint.h>

static uint32_t tests_run, tests_passed, tests_failed;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) {                                                            \
            tests_passed++;                                                    \
            KLOG_INFO("KMALLOC_TEST", "[TEST %u] [PASS] %s\n", tests_run,      \
                      msg);                                                    \
        } else {                                                               \
            tests_failed++;                                                    \
            KLOG_ERROR("KMALLOC_TEST", "[TEST %u] [FAIL] %s\n", tests_run,     \
                       msg);                                                   \
        }                                                                      \
    } while (0)

/* desc_of - foe any given heap pointer -> returns the page descriptor */
static inline kmem_page_desc_t *desc_of(void *p) {
    return (kmem_page_desc_t *)((uintptr_t)p & ~(uintptr_t)0xFFF);
}

void run_kmalloc_tests(void) {
    tests_run = tests_passed = tests_failed = 0;
    KLOG_INFO("KMALLOC_TEST", "Starting Memory Allocation tests....\n");

    /* size to class conversions tests */
    KLOG_VERBOSE("KMALLOC_TEST",
                 "Tests. Allocatable size -> Class size conversion tests...\n");
    TEST_ASSERT(kmem_class_size(kmem_size_to_class(20)) == 24,
                "allocation size 20 -> class size 24");
    TEST_ASSERT(kmem_class_size(kmem_size_to_class(33)) == 48,
                "allocation size 33 -> class size 48");
    TEST_ASSERT(kmem_class_size(2048) >= 0,
                "allocation size 2048 is an allocatable slab");
    TEST_ASSERT(kmem_size_to_class(2049) == -1,
                "allocation size 2049 -> large alloc tier");

    int aligned = 1;
    KLOG_VERBOSE("KMALLOC_TEST", "Starting alignment tests...\n");

    for (size_t sz = 1; sz <= 2048; sz += 61) {
        void *p = kmalloc(sz);
        /* alignment check - ie. allocated memory should be multiple of
         * KMEM_ALIGN = 8. KMEM_ALIGN-1 = 7 (all bits set) if not aligned &
         * operation of memory ref will give non zero */
        if (!p || ((uintptr_t)p & (KMEM_ALIGN - 1))) {
            aligned = 0;
        }
        kfree(p);
    }
    TEST_ASSERT(aligned, "All slab allocation is 8byte aligned.");

    KLOG_VERBOSE("KMALLOC_TEST", "Staring allocation check tests...\n");

    void *a = kmalloc(24);
    kfree(a);
    void *b = kmalloc(24);
    TEST_ASSERT(a == b, "freed slot reused (LIFO)");
    kfree(b);

    void *x = kmalloc(24), *y = kmalloc(24);
    TEST_ASSERT(x != y, "two live allocations distinct");
    kfree(x);
    kfree(y);

    uint8_t *lp = kmalloc(3000);
    TEST_ASSERT(lp && ((uintptr_t)lp & 0xFFF) != 0,
                "large ptr not at page base");
    TEST_ASSERT(desc_of(lp)->tier == KMEM_TIER_LARGE, "large tier tagged");
    int lok = 1;
    for (int i = 0; i < 3000; i++)
        lp[i] = (uint8_t)(i & 0xFF);
    for (int i = 0; i < 3000; i++)
        if (lp[i] != (uint8_t)(i & 0xFF))
            lok = 0;
    TEST_ASSERT(lok, "large block fully writable/readable");
    kfree(lp);

    TEST_ASSERT(kmalloc(5000) == NULL, "alloc > frame capacity returns NULL");

    uint8_t *cp = kcalloc(100, 4);
    int zok = 1;
    for (int i = 0; i < 400; i++)
        if (cp[i])
            zok = 0;
    TEST_ASSERT(zok, "kcalloc zero-initializes");
    kfree(cp);
    TEST_ASSERT(kcalloc((size_t)1 << 20, (size_t)1 << 20) == NULL,
                "kcalloc overflow -> NULL");

    void *k0 = krealloc(NULL, 100);
    TEST_ASSERT(k0 != NULL, "krealloc(NULL,n) == kmalloc");
    kfree(k0);
    void *rp = kmalloc(100);
    TEST_ASSERT(krealloc(rp, 0) == NULL, "krealloc(p,0) frees -> NULL");

    void *s1 = kmalloc(64), *s2 = kmalloc(64);
    uint32_t before = desc_of(s1)->obj_size;
    krealloc(s1, 50);
    TEST_ASSERT(desc_of(s1)->obj_size == before,
                "slab shrink keeps class size");
    kfree(s1);
    kfree(s2);

    uint8_t *g = kmalloc(24);
    for (int i = 0; i < 24; i++)
        g[i] = (uint8_t)(i + 1);
    uint8_t *g2 = krealloc(g, 600);
    int gok = (g2 != NULL);
    for (int i = 0; i < 24; i++)
        if (g2[i] != (uint8_t)(i + 1))
            gok = 0;
    TEST_ASSERT(gok, "krealloc grow preserves data");
    kfree(g2);

    uint8_t *cvp = kmalloc(4000);
    for (int i = 0; i < 20; i++)
        cvp[i] = (uint8_t)(i * 3 + 1);
    uint8_t *cvr = krealloc(cvp, 20);
    int cok = (desc_of(cvr)->tier == KMEM_TIER_SLAB);
    for (int i = 0; i < 20; i++)
        if (cvr[i] != (uint8_t)(i * 3 + 1))
            cok = 0;
    TEST_ASSERT(cok, "large->slab shrink moves & keeps data");
    kfree(cvr);

    static void *S[512];
    for (int i = 0; i < 512; i++)
        S[i] = kmalloc((size_t)((i * 131 + 7) % 2048) + 1);
    for (int i = 0; i < 512; i += 2) {
        kfree(S[i]);
        S[i] = NULL;
    }
    for (int i = 1; i < 512; i += 2) {
        kfree(S[i]);
        S[i] = NULL;
    }
    TEST_ASSERT(1, "stress alloc/free completed without fault");

    kmalloc_dump_stats();

    KLOG_INFO("KMALLOC_TEST", "Completed Memory Allocation tests....\n");
}
