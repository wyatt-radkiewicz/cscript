/*
 * This contains all encompasing c script state.
 * Things within cstate_t can be access from the lexer stage, to the parser,
 * and all the way to the compiler, and some can be accessed from the vm.
 *
 * The VM is somewhat seperated here since not all allocations have to be
 * around for the VM to run.
 */
#ifndef _cstate_h_
#define _cstate_h_

#include "cstate_struct.h"
#include "cstate_error.h"
#include "comp/type.h"

typedef struct ast_s ast_t;

/*
 * Initialize the cstate
 * Returns NULL on error
 */
cstate_t *
cstate_init( 
	cs_alloc_t		allocator,
	void		   *allocuser);

/*
 * Cleans up a valid cstate struct.
 * May accept NULL as free pointer, although function will do nothing
 */
void
cstate_cleanup(
	cstate_t	  **state,
	cs_free_t		freepfn);

/*
 * Allocates a AST node
 * If it returns NULL, a allocation error is already emitted
 */
ast_t *
cstate_alloc_ast(
	cstate_t	   *state);

/*
 * Emits a cstate info/compiler error
 * Returns CS_OKAY if info, and CS_COMPILER_CRITICAL if not
 */
int
cstate_emit_error(
	cstate_t	   *state,
	bool			isinfo,
	const char	   *msg);

/*
 * Emits a cstate warning/error error
 * Returns CS_OKAY if warning, and CS_ERROR if not
 */
int
cstate_emit_srcerror(
	cstate_t	   *state,
	bool			iserror,
	int				srcline,
	int				srccol,
	int				srclen,
	const char	   *msg);

/*
 * Returns the start of the cstate error list
 * Will return NULL if there are no errors.
 * NOTE: Remember not all errors make the code non-runable
 */
cstate_error_t *
cstate_get_errors(
	cstate_t	   *state);


#endif

