/*
 * Ast utilities and definitions
 */
#ifndef _ast_h_
#define _ast_h_

#include "cstate/cstate.h"
#include "common.h"

/*
 * Types of ast nodes
 */
typedef enum
{
	/* Type nodes */
	AST_TYPE_CONST,
	AST_TYPE_MUT,
	AST_TYPE_PTR,
	AST_TYPE_REF,
	AST_TYPE_ARR,
	AST_TYPE_PTRSLICE,
	AST_TYPE_REFSLICE,
	AST_TYPE,				/* POD types, enums, structs, and typedefs */

	/* Expression nodes */
	AST_EXPR_COND,
	AST_EXPR_BINARYOP,
	AST_EXPR_UNARYOP,
	AST_EXPR_BUILTIN,
	AST_EXPR_ACCESSOR,
	AST_EXPR_CALL,
	AST_EXPR_INIT,
	AST_EXPR_STMT,
	AST_EXPR_LITERAL,

	/* Variable nodes */
	AST_NAMED_TYPE,
	AST_TEMPLATE_PARAM,
	
	/* Statement nodes */
	AST_STMT_LET,
	AST_STMT_WHILE,
	AST_STMT_FOR,
	AST_STMT_IF,
	AST_STMT_MATCH,
	AST_STMT_EXPR,
	AST_STMT_RETURN,
	AST_STMT_BREAK,
	AST_STMT_CONTINUE,
	
	/* Top-level nodes */
	AST_DEF_STRUCT,
	AST_DEF_ENUM,
	AST_DEF_TYPE,
	AST_DEF_FUNC
} ast_type_t;

/*
 * Generic AST Node
 */
struct ast_s
{
	int				type;
	strview_t		str;
	struct ast_s   *next;

	/* Used for primary node child */
	struct ast_s   *child;

	/* Used when a node has 'counterpart' children. */
	struct ast_s   *a, *b;
};

#endif

