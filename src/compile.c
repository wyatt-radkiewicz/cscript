#include <stdlib.h>
#include <string.h>

#include "compile.h"

#define MAX_USERTYPES 64
#define MAX_SCOPES 12

struct state
{
	struct ast_node *funcs[MAX_USERTYPES];
	struct ast_node *structs[MAX_USERTYPES];
	struct ast_node *typedefs[MAX_USERTYPES];
	struct ast_node *scopebuf[512];
	int scopes[MAX_SCOPES];
	int scope;
	struct ast_node *ast;
	struct vm *vm;
	struct compile_error *errs;
	size_t nerrs, errsz;

	int globalhead;
};

static int alloc_global(struct state *state, int nelems) {
	const int old = state->globalhead;
	state->globalhead += nelems;
	return old;
}

static int get_size(struct state *state, struct ast_node *type) {
	return 1;
}

static void add_variable(struct state *state, struct ast_node *vardef) {
	state->scopebuf[state->scopes[state->scope+1]++] = vardef;
}

static int get_podtype(struct state *state, struct ast_node *type) {
	if (type->type == AST_IDENT) return -1;
	switch (type->type) {
	case AST_REF_OF: return VAR_REF;
	case AST_CONST_OF: return get_podtype(state, type->inner);
	case AST_PTR_OF: return VAR_PTR;
	case AST_CPTR_OF: return VAR_CPTR;
	case AST_BASE_TYPE:
		switch (type->token.type) {
		case TOK_TYPE_INT: return VAR_INT;
		case TOK_TYPE_UINT: return VAR_UINT;
		case TOK_TYPE_FLOAT: return VAR_FLOAT;
		case TOK_TYPE_CHAR: return VAR_CHAR;
		default: return -1;
		}
	default: return -1;
	}
}

static struct vm_typed_var eval_constexpr(struct state *state,
					struct ast_node *expr,
					struct ast_node *finaltype) {
	struct vm_typed_var eval = (struct vm_typed_var){ .type = get_podtype(state, finaltype) };
	
	if (expr->type == AST_BINARY) {
		struct vm_typed_var left = eval_constexpr(state, expr->alt1, finaltype);
		struct vm_typed_var right = eval_constexpr(state, expr->alt2, finaltype);
		switch (eval.type) {
		case VAR_INT:
			switch (expr->token.type) {
			case TOK_PLUS: eval.data.i = left.data.i + right.data.i; break;
			case TOK_DASH: eval.data.i = left.data.i - right.data.i; break;
			case TOK_STAR: eval.data.i = left.data.i * right.data.i; break;
			case TOK_SLASH: eval.data.i = left.data.i / right.data.i; break;
			default: break;
			}
			break;
		case VAR_CHAR:
			switch (expr->token.type) {
			case TOK_PLUS: eval.data.c = left.data.c + right.data.c; break;
			case TOK_DASH: eval.data.c = left.data.c - right.data.c; break;
			case TOK_STAR: eval.data.c = left.data.c * right.data.c; break;
			case TOK_SLASH: eval.data.c = left.data.c / right.data.c; break;
			default: break;
			}
			break;
		case VAR_UINT:
			switch (expr->token.type) {
			case TOK_PLUS: eval.data.u = left.data.u + right.data.u; break;
			case TOK_DASH: eval.data.u = left.data.u - right.data.u; break;
			case TOK_STAR: eval.data.u = left.data.u * right.data.u; break;
			case TOK_SLASH: eval.data.u = left.data.u / right.data.u; break;
			default: break;
			}
			break;
		case VAR_FLOAT:
			switch (expr->token.type) {
			case TOK_PLUS: eval.data.f = left.data.f + right.data.f; break;
			case TOK_DASH: eval.data.f = left.data.f - right.data.f; break;
			case TOK_STAR: eval.data.f = left.data.f * right.data.f; break;
			case TOK_SLASH: eval.data.f = left.data.f / right.data.f; break;
			default: break;
			}
			break;
		default: break;
		}
	}
	if (expr->type == AST_LITERAL) {
		char tmp[32];
		strncpy(tmp, expr->token.str, 32);
		tmp[31] = '\0';
		eval.data.i = atoi(tmp);
	}

	return eval;
}

static void compile_let(struct state *state, struct ast_node *let) {
	if (state->scope == 0) {
		add_variable(state, let);
		// Allocate space in the globals buffer and set the variable
		int storage = alloc_global(state, get_size(state, let->inner));
		state->vm->data[storage] = eval_constexpr(state, let->alt1, let->inner).data;
		return;
	}
}

void compile(struct ast_node *ast,
		struct vm *vm,
		struct compile_error *err,
		size_t errsz)
{
	struct state state = {
		.ast = ast,
		.vm = vm,
		.errs = err,
		.nerrs = 0,
		.errsz = errsz,
	};

	struct ast_node *curr = state.ast;
	while (curr) {
		if (curr->type == AST_LET) compile_let(&state, curr);

		curr = curr->next;
	}
}

