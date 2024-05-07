#include "cstate_alloc.h"

/*
 * Maintains free list when free, and pointer to parent chunk when allocated
 */
typedef struct cstate_chunk_s cstate_chunk_t;
typedef union cstate_block_u
{
	cstate_chunk_t		   *chunk;
	union cstate_block_u   *nextfree;
} cstate_block_t;

/*
 * Intentionally aligned to max align type (not including SIMD types of course)
 */
struct cstate_chunk_s
{
	/* Next allocated chunk */
	cstate_chunk_t		   *next;

	/* Next free chunk */
	cstate_chunk_t		   *nextfree;

	/* Number of blocks */
	int						len;

	/* Where to allocate the next block. */
	/* If this equals len, then use the free list */
	int						allocptr;

	/* Linked list of all free blocks */
	cstate_block_t		   *freelist;
};

/*
 * Maintains the allocation state for objects of a certain size
 */
struct cstate_pool_s
{
	/* All of the chunks full or not */
	cstate_chunk_t		   *list;

	/* Chunks that can be allocated from */
	cstate_chunk_t		   *useable;

	/* Size of each element */
	int						elemsize;

	/* Current chunk length being used */
	int						chunklen;
};

void *
cstate_alloc(
	cstate_t	   *state,
	size_t			size)
{
	void *ptr;

	/* Emit error if can't allocate anymore room */
	if (!(ptr = state->alloc(state->allocuser, size)))
	{
		cstate_emit_error(state, false, ERRMSG_00);
		return NULL;
	}

	return ptr;
}

/*
 * Creates a new chunk and reflects that change in the pool object passed in
 */
static int
cstate_chunk_new(
	cstate_pool_t  *pool,
	cstate_t	   *state)
{
	cstate_chunk_t *chunk;

	/* Initialize the chunk */
	if (!(chunk = cstate_alloc(
		state,
		sizeof(*chunk) + pool->elemsize * pool->chunklen)))
	{
		return CS_COMPILER_CRITICAL;
	}
	chunk->len = pool->chunklen;
	chunk->next = pool->list;
	chunk->nextfree = pool->useable;
	chunk->freelist = NULL;
	chunk->allocptr = 0;

	/* Add the chunk to the pool's chunk list */
	pool->list = chunk;
	pool->useable = chunk;
	pool->chunklen *= 2;

	return CS_OKAY;
}

cstate_pool_t *
cstate_pool_new(
	cstate_t	   *state,
	int				elemsize,
	int				initcapacity)
{
	cstate_pool_t *pool;

	/* Allocate the pool */
	if (!(pool = cstate_alloc(state, sizeof(*pool)))) return NULL;
	pool->elemsize = elemsize;
	pool->chunklen = initcapacity;
	
	/* Create an initial chunk */
	if (cstate_chunk_new(pool, state) != CS_OKAY) return NULL;
	return pool;
}

void
cstate_pool_delete(
	cstate_t	   *state,
	cs_free_t		freepfn,
	cstate_pool_t **pool)
{
	cstate_chunk_t *iter;

	/* Free every chunk */
	for (iter = (*pool)->list; iter;) {
		cstate_chunk_t  *next;

		next = iter->next;
		freepfn(state->allocuser, iter);
		iter = next;
	}

	/* Free the pool */
	freepfn(state->allocuser, *pool);
	*pool = NULL;
}

void *
cstate_pool_alloc(
	cstate_pool_t  *pool,
	cstate_t	   *state)
{
	cstate_chunk_t *chunk;
	void		   *ret;

	/* Create a new chunk if there are none left */
	if (!pool->useable)
	{
		if (cstate_chunk_new(pool, state) != CS_OKAY) return NULL;
	}

	chunk = pool->useable;
	if (chunk->allocptr != chunk->len)
	{
		/* Use allocptr method */
		ret = (void *)((uintptr_t)(chunk + 1) + pool->elemsize * chunk->allocptr++);
	}
	else if (chunk->freelist)
	{
		/* Take from freelist */
		ret = chunk->freelist;
		chunk->freelist = chunk->freelist->nextfree;
	}
	else
	{
		DBG_ASSERT2("Malformed allocation chunk!");
	}
	
	/* Remove from free chunks list */
	if (chunk->allocptr == chunk->len
		&& !chunk->freelist)
	{
		pool->useable = chunk->nextfree;
		chunk->nextfree = NULL;
	}

	/* Set the 'next' pointer to a pointer to the owning chunk of this block */
	((cstate_block_t *)ret)->chunk = chunk;
	return ret;
}

void 
cstate_pool_free(
	cstate_pool_t  *pool,
	void		   *blockparam)
{
	cstate_block_t *block;
	cstate_chunk_t *chunk;

	block = blockparam;
	chunk = block->chunk;

	/* Make sure that the chunk pointer is valid */
#ifndef NDEBUG
	{
		cstate_chunk_t *iter;
		for (iter = pool->list; iter; iter = iter->next)
		{
			if (iter == chunk) goto found;
		}
		DBG_ASSERT2("Heap corruption detected");
	}
	found:
#endif

	/* Add it back to the free list of chunks */
	if (!chunk->freelist && chunk->allocptr == chunk->len)
	{
		chunk->nextfree = pool->useable;
		pool->useable = chunk;
	}

	/* Add the block to the freelist */
	block->nextfree = chunk->freelist;
	chunk->freelist = block;
}

