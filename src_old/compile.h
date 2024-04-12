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
	struct ast_node *type;
	struct token name;
	int dataloc, absloc;
	int is_externfn;
};
struct scope {
	int stackbase, scopebase, nparams;
};
struct vartype {
	enum vm_varty ty;
	int size;
	int static_arrlen;
};

struct scoperef {
	int absloc;
	enum vm_varty podtype;
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

#include <stdio.h>
static void vm_print_opcode(struct vm_code code) {
	switch (code.op) {
	case OP_PUSH0: printf("OP_PUSH0"); break;
	case OP_PUSHNI: printf("OP_PUSHNI"); break;
	case OP_POPN: printf("OP_POPN"); break;
	case OP_REPUSH: printf("OP_REPUSH"); break;
	case OP_TRANSFER: printf("OP_TRANSFER"); break;
	case OP_NEWRC: printf("OP_NEWRC"); break;
	case OP_ADDRC: printf("OP_ADDRC"); break;
	case OP_DECRC: printf("OP_DECRC"); break;
	case OP_ADD: printf("OP_ADD"); break;
	case OP_SUB: printf("OP_SUB"); break;
	case OP_MUL: printf("OP_MUL"); break;
	case OP_DIV: printf("OP_DIV"); break;
	case OP_STORE: printf("OP_STORE"); break;
	case OP_LOAD: printf("OP_LOAD"); break;
	case OP_STOREPTR: printf("OP_STOREPTR"); break;
	case OP_LOADPTR: printf("OP_LOADPTR"); break;
	case OP_STORECPTR: printf("OP_STORECPTR"); break;
	case OP_LOADCPTR: printf("OP_LOADCPTR"); break;
	case OP_CALL: printf("OP_CALL"); break;
	case OP_EXTERN_CALL: printf("OP_EXTERN_CALL"); break;
	case OP_RET: printf("OP_RET"); break;
	case OP_JMP: printf("OP_JMP"); break;
	case OP_BEQ: printf("OP_BEQ"); break;
	case OP_BNE: printf("OP_BNE"); break;
	case OP_TEQ: printf("OP_TEQ"); break;
	case OP_TNE: printf("OP_TNE"); break;
	case OP_TGT: printf("OP_TGT"); break;
	case OP_TLT: printf("OP_TLT"); break;
	case OP_TGE: printf("OP_TGE"); break;
	case OP_TLE: printf("OP_TLE"); break;
	case OP_CAST: printf("OP_CAST"); break;
	case OP_AND: printf("OP_AND"); break;
	case OP_OR: printf("OP_OR"); break;
	case OP_EOR: printf("OP_EOR"); break;
	case OP_SHL: printf("OP_SHL"); break;
	case OP_SHR: printf("OP_SHR"); break;
	}
	printf(": %d\n", code.arg);
}

#endif

