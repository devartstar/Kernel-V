#ifndef POOL_ALLOC_H
#define POOL_ALLOC_H

/**
* Kernel allocates 4KB Pages using pmm_alloc_frame()
* Each 4KB page is broken down in to N fix-size objects.
* A simple singly-linked free list manages the available objects.
* Pool empty - allocate a new 4KB frame and add entries to free list.
*/

#include <stdint.h>
#include <stddef.h>

typedef struct pool_allocator
{
	// pointer to the first free object
	void*	free_list;

	// size of each object
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
int pool_init (pool_allocator_t *pool, size_t obj_size);

/**
* pool_aloc - Allocates an object from the pool also refil if needed
* @pool: structure maintaining the pool
*
* Return: pointer to the allocated memory obj
*/
void *pool_alloc (pool_allocator_t *pool);

/**
* pool_free - Free object back to the pool
* @pool: structure to maintain the pool
* @obj: pointer to the object to free 
*/
void pool_free (pool_allocator_t *pool, void* obj);

#endif
