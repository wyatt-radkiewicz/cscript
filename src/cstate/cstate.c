#include <string.h>

#include "cstate.h"
#include "cstate_alloc.h"
#include "parser/ast.h"

/* The size of every type used in the pools */
static const int _type_sizes[CSTATEPOOL_MAX] = {
	sizeof(type_t),
	sizeof(ast_t),
	sizeof(cstate_error_t),
};

cstate_t *
cstate_init( 
	cs_alloc_t		allocator,
	void		   *allocuser)
{
	cstate_t   *state;
	int			i;

	DBG_ASSERT(allocator);

	/* Allocate the state object itself and initialize it */
	if (!(state = allocator(allocuser, sizeof(cstate_t)))) return NULL;
	memset(state, 0, sizeof(*state));
	state->alloc = allocator;
	state->allocuser = allocuser;
	
	/* Create every pool in the pools array */
	for (i = 0; i < CSTATEPOOL_MAX; i++)
	{
		if (!(state->pools[i] = cstate_pool_new(state, _type_sizes[i], 32))) return NULL;
	}
	return state;
}

void
cstate_cleanup(
	cstate_t	  **state,
	cs_free_t		freepfn)
{
	int			i;

	if (!freepfn) return;

	/* Free every pool */
	for (i = 0; i < CSTATEPOOL_MAX; i++)
	{
		cstate_pool_delete(*state, freepfn, (*state)->pools + i);
	}

	/* Free the state object itself */
	freepfn((*state)->allocuser, *state);
	*state = NULL;
}

ast_t *
cstate_alloc_ast(
	cstate_t	   *state)
{
	return cstate_pool_alloc(state->pools[CSTATEPOOL_AST], state);
}

/*
 * Throws error if it can not allocate
 */
static void *
cstate_alloc_error(
	cstate_t	   *state)
{
	cstate_error_t *error;

	if (!(error = cstate_pool_alloc(state->pools[CSTATEPOOL_ERROR], state)))
	{
		if (!state->errors.tail) assert(0 && "Can't allocate errors!");
		state->errors.tail->lvl = CS_COMPILER_CRITICAL;
		strcpy(state->errors.tail->msg, ERRMSG_01);
		return NULL;
	}

	/* Set the error's next pointer */
	error->next = state->errors.head;
	state->errors.head = error;
	if (!state->errors.tail) state->errors.tail = error;
	return error;
}

int
cstate_emit_error(
	cstate_t	   *state,
	bool			isinfo,
	const char	   *msg)
{
	cstate_error_t *error;
	int				ret;

	if (!(error = cstate_alloc_error(state)))
	{
		ret = CS_COMPILER_CRITICAL;
		goto end;
	}

	/* Initialize error */
	error->lvl = isinfo ? CSTATE_ERROR_INFO : CSTATE_ERROR_WARNING;
	strcpy(error->msg, msg);
	ret = isinfo ? CS_OKAY : CS_ERROR;
end:
	return ret;
}

int
cstate_emit_srcerror(
	cstate_t	   *state,
	bool			iserror,
	int				srcline,
	int				srccol,
	int				srclen,
	const char	   *msg)
{
	cstate_error_t *error;
	int				ret;

	if (!(error = cstate_alloc_error(state)))
	{
		ret = CS_COMPILER_CRITICAL;
		goto end;
	}

	/* Initialize the error */
	error->lvl = iserror ? CSTATE_ERROR_CRITICAL : CSTATE_ERROR_COMPILER;
	strcpy(error->msg, msg);
	error->src.line = srcline;
	error->src.col = srccol;
	error->src.len = srclen;
	ret = iserror ? CS_ERROR : CS_OKAY;
end:
	return ret;
}

cstate_error_t *
cstate_get_errors(
	cstate_t	   *state)
{
	return state->errors.head;
}

