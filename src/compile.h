#ifndef _compile_h_
#define _compile_h_

#include "parser.h"
#include "vm.h"

struct compile_error
{
	const char *msg;
	int line;
};

void compile(struct ast_node *ast,
		struct vm *vm,
		struct compile_error *err,
		size_t errsz);

#endif

