#include "pool_alloc.h"
#include "pmm.h"

#include <stdint.h>
#include <stddef.h>

#define POOL_PAGE_SIZE 4096


/**
 * round_up - Helper to round up to multiple
 * @size: current value to be rounded up
 * @align: alignment, rounded up to multiple of align
 */
static size_t round_up (size_t size, size_t align) 
{
	return (size + align - 1) & ~(align - 1);
}

int pool_init (pool_allocator_t *pool, size_t obj_size)
{
	/* size of each object in the pool should be least pointer size to store
	* next pointer */
	if (!pool || obj_size < sizeof(void*))
	{
		return -1;
	}

	pool->free_list = NULL;
	pool->object_size = round_up (obj_size, sizeof(void*));

	return 0;
}

void *pool_alloc (pool_allocator_t* pool)
{
	if (!pool) 
	{
		return;
	}

	/* If no free object, allocate a new page and split into object */
	if (!pool->free_list)
	{
		void *page = pmm_alloc_frame ();
		if (!page)
		{
			return NULL;
		}

		uint8_t *p = (uint8_t*) page;

		size_t n_objs = POOL_PAGE_SIZE / pool->object_size;

		for (size_t obj_idx = 0; obj_idx < n_objs; obj_idx++)
		{
			void *obj = (void*)(p + obj_idx * pool->object_size);
			*((void**)obj) = pool->free_list;
			pool->free_list = obj;
		}
	}

	/* Pop the first free object */
	void *obj = pool->free_list;
	pool->free_list = *((void**)obj);

	return obj;
}

void pool_free (pool_allocator_t *pool, void *obj)
{
	if (!pool || !obj)
	{
		return;
	}

	*((void**)obj) = pool->free_list;
	pool->free_list = obj;
}

