#ifndef _compile_h_
#define _compile_h_

#include <stdbool.h>

#include "parser.h"
#include "vm.h"

struct compile_error
{
	const char *msg;
	int line;
};

#define MAX_USERTYPES 64
#define MAX_SCOPES 12

struct scopevar {
	const struct ast_node *type;
	struct token name;
	int dataloc, absloc;
	int is_externfn;
};
struct scope {
	int stackbase, scopebase, nparams;
};

struct scoperef {
	int absloc;
	bool isglobal;
	bool lvalue;
	bool isvoid;
};

struct state {
	struct ast_node *structs[MAX_USERTYPES];
	struct ast_node *typedefs[MAX_USERTYPES];
	struct scopevar scopebuf[512];
	struct scope scopes[MAX_SCOPES];
	struct ast_node *ast;
	struct vm *vm;
	struct compile_error *errs;
	size_t nerrs, errsz;

	int currscope, stacktop, scopetop;
	int globalhead, nstructs, ntypedefs, codehead;
};

void compile_init(struct state *state,
		struct ast_node *ast,
		struct vm *vm,
		struct compile_error *err,
		size_t errsz);

void compile_extern_fn(struct state *state,
		const char *name,
		vm_extern_pfn pfn);

void compile(struct state *state);

#endif

