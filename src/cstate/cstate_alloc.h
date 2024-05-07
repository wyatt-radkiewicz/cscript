/*
 * Allocation primitives used by the cscript state
 */
#ifndef _cstate_alloc_h_
#define _cstate_alloc_h_

#include <stddef.h>

#include "cstate_struct.h"
#include "cstate.h"

/*
 * Convient wrapper around allocation
 * If it returns NULL, a compiler error was already emitted
 */
void *
cstate_alloc(
	cstate_t	   *state,
	size_t			size);

/*
 * Initializes a new pool from a cstate_t
 * If it returns NULL, an error was already emitted
 */
cstate_pool_t *
cstate_pool_new(
	cstate_t	   *state,
	int				elemsize,
	int				initcapacity);

/*
 * Releases the resources used by this pool object
 */
void
cstate_pool_delete(
	cstate_t	   *state,
	cs_free_t		freepfn,
	cstate_pool_t **pool);

/*
 * Allocates a new entry in the pool
 * If it returns NULL, and error was already emitted
 */
void *
cstate_pool_alloc(
	cstate_pool_t  *pool,
	cstate_t	   *state);

/*
 * Frees a entry in the pool
 */
void 
cstate_pool_free(
	cstate_pool_t  *pool,
	void		   *blockparam);

#endif

