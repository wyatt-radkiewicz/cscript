#ifndef _parser_h_
#define _parser_h_

#include <stddef.h>

#include "lexer.h"

enum ast_type
{
	AST_STRUCT_DEF,
	AST_FUNC_DEF,
	AST_TYPE_DEF,

	AST_STRUCT_MEMBER,
	AST_LET,
	AST_IDENT,
	AST_TYPE,
	AST_CONST_OF,
	AST_REF_OF,
	AST_ARRAY_OF,
	AST_BASE_TYPE,
	AST_FUNC_OF,

	AST_TYPEOF,
	AST_SIZEOF,
	AST_LENOF,
	AST_AS,

	AST_LIT_INT,
	AST_LIT_FLOAT,
	AST_LIT_STR,

	AST_BINARY,
	AST_UNARY,
	
	AST_IF,
	AST_WHILE,
	AST_EXPR,
	AST_COMPOUND,
	AST_BREAK,
	AST_CONTINUE,
	AST_RETURN,
};

struct ast_node
{
	enum ast_type type;
	unsigned int flags;
	struct token token;
	struct ast_node *inner, *alt1, *alt2, *next;
};

struct ast_error
{
	const char *msg;
	int line;
};

// Returns the root node
struct ast_node *ast_construct(const char *src,
				struct ast_node *buffer,
				size_t bufsz,
				struct ast_error *err_buffer,
				size_t err_bufsz);

#endif

