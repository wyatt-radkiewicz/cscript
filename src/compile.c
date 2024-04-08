#include "compile.h"

struct state
{
	struct ast_node *ast;
	struct vm *vm;
	struct compile_error *errs;
	size_t nerrs, errsz;
};

void compile(struct ast_node *ast,
		struct vm *vm,
		struct compile_error *err,
		size_t errsz)
{
	struct state state = {
		ast,
		vm,
		err,
		0,
		errsz
	};

	
}

