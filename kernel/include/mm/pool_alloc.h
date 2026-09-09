#ifndef POOL_ALLOC_H
#define POOL_ALLOC_H

/**
 * Kernel allocates 4KB Pages using pmm_alloc_frame()
 * Each 4KB page is broken down in to N fix-size objects.
 * A simple singly-linked free list manages the available objects.
 * Pool empty - allocate a new 4KB frame and add entries to free list.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * NOTE: updating pool_allocator free list:
 *
 * free──►0x1000 0x1100 0x1200 0x1300
 * list      │      │      │      │
 *			 ▼      ▼      ▼      ▼
 *           ┌──────┬──────┬──────┬──────┐
 *           │0x1100│0x1200│0x1300│0x1400│
 *           └──────┴──────┴──────┴──────┘
 *             obj1   obj2   obj3   obj4
 *
 *
 * Allocation: get first entry from free list
 * 1. obj=free_list
 * 2. free_list = *((void*)obj) = 0x1100
 *
 * DeAllocation: add freed object to first entry
 * 1. *((void **)obj) = free_list
 *    update the value stored in obj
 *    to the address free_list was pointing
 * 2. free_list = obj (points to addr of obj)
 */

typedef struct pool_allocator {
    //  pointer to the first free object
    void *free_list;

    //  size of each object
    size_t object_size;

} pool_allocator_t;

/**
 * pool_init - Create a pool for allocating memory
 * @pool: structure maintaining the pool
 * @obj_size: size of each element in pool, set it to the size of the object for
 * which we are creating pool
 *
 * Return: 0 on success
 */
int pool_init(pool_allocator_t *pool, size_t obj_size);

/** pool_alloc - If there is no object in the free list of the pool.
 * Allocate a new frame to the pool. Each Frame = 4KB, create multiple
 * pool objects from a frame and add all in the free_list.
 *
 * @pool - pointer to the pool struct
 *
 * @void* - pointer to the first entry in the pool freelist.
 */
void *pool_alloc(pool_allocator_t *pool);

/**
 * pool_free - Free object back to the pool
 * @pool: structure to maintain the pool
 * @obj: pointer to the object to free
 */
void pool_free(pool_allocator_t *pool, void *obj);

#endif
