/*
 * Internal cscript state type used by files in this directory
 */
#ifndef _cstate_struct_h_
#define _cstate_struct_h_

#include "common.h"

typedef struct cstate_pool_s cstate_pool_t;
typedef struct cstate_error_s cstate_error_t;

/* All the pools for types in cstate */
typedef enum
{
	CSTATEPOOL_TYPE,
	CSTATEPOOL_AST,
	CSTATEPOOL_ERROR,
	CSTATEPOOL_MAX
} cstate_pool_types_t;

/*
 * State data used by most functions in c script
 */
typedef struct cstate_s
{
	/* For allocating chunks of data */
	cs_alloc_t				alloc;
	void				   *allocuser;

	/* Pools of data for different types */
	cstate_pool_t		   *pools[CSTATEPOOL_MAX];
	struct
	{
		cstate_error_t	   *head;
		cstate_error_t	   *tail;
	}						errors;
} cstate_t;

#endif

