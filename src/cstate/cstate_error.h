/*
 * Header used to unify cstate errors
 */
#ifndef _cstate_error_h_
#define _cstate_error_h_

#define ERRMSG_00 "errcode 00: Ran out of memory!"
#define ERRMSG_01 "errcode 01: Ran out of memory for errors! Aborting now."

/*
 * Different error levels
 */
typedef enum
{
	CSTATE_ERROR_NONE,		/* Code still compiles */
	CSTATE_ERROR_INFO,		/* Code still compiles */
	CSTATE_ERROR_WARNING,	/* Code still compiles, but something is wrong */
	CSTATE_ERROR_CRITICAL,	/* Code will not compile, something is wrong */
	CSTATE_ERROR_COMPILER	/* Not a fault with the code, but the compiler */
							/* Code will not compile, something is wrong */
} cstate_error_level_t;

/*
 * Represents all errors that can occur in a cstate program
 */
struct cstate_error_s
{
	/* If lvl is CSTATE_ERROR_NONE, no other fields matter */
	int			lvl;

	/* Null-terminated message buffer */
	char		msg[1024];

	/* Where the error occurs in the source code */
	/* If error level is COMPILER, or INFO, then these fields don't matter */
	struct
	{
		int		line;
		int		col;
		int		len;
	}			src;

	/* The next error in the list */
	struct cstate_error_s  *next;
};

#endif

