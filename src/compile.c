#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "compile.h"

static void makeop(struct state *state, int op, int arg) {
	state->vm->code[state->codehead++] = (struct vm_code){ .op = op, .arg = arg };
}

static int alloc_global(struct state *state, int nelems) {
	const int old = state->globalhead;
	state->globalhead += nelems;
	return old;
}

static int get_size(struct state *state, struct ast_node *type) {
	return 1;
}

static void push_scope(struct state *state) {
	struct scope *scope = state->scopes + ++state->currscope;
	scope->scopebase = state->scopetop;
	scope->stackbase = state->stacktop;
	scope->nparams = 0;
}
static void pop_scope(struct state *state) {
	assert(state->currscope);
	state->stacktop = state->scopes[state->currscope].stackbase - 1;
	state->scopetop = state->scopes[state->currscope].scopebase - 1;
	state->currscope--;
}

static int add_variable(struct state *state, struct ast_node *vardef, int global) {
	//struct scope *scope = state->scopes + state->currscope;
	state->scopebuf[state->scopetop] = (struct scopevar){
		.name = vardef->token,
		.type = vardef->inner,
		.absloc = global ? -1 : state->stacktop,
		.dataloc = global ? alloc_global(state, get_size(state, vardef->inner)) : -1,
	};
	int loc = global ? state->scopebuf[state->scopetop].dataloc : state->scopebuf[state->scopetop].absloc;
	if (!global) state->stacktop += get_size(state, vardef->inner);
	state->scopetop++;
	return loc;
}

static void add_argument(struct state *state, struct ast_node *vardef, int param_idx) {
	struct scope *scope = state->scopes + state->currscope;
	state->scopebuf[state->scopetop] = (struct scopevar){
		.name = vardef->token,
		.type = vardef->inner,
		.absloc = scope->stackbase - param_idx,
		.dataloc = -1,
	};
	state->scopetop++;
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
	if (state->currscope == 0) {
		int storage = add_variable(state, let, 1);
		// Allocate space in the globals buffer and set the variable
		state->vm->data[storage] = eval_constexpr(state, let->alt1, let->inner).data;
		return;
	}
}

static int compile_expr(struct state *state, struct ast_node *expr) {
	int eval = 0;

	if (expr->type == AST_BINARY) {
		compile_expr(state, expr->alt1);
		compile_expr(state, expr->alt2);
		switch (expr->token.type) {
		case TOK_PLUS: makeop(state, OP_ADD, 0); break;
		case TOK_DASH: makeop(state, OP_SUB, 0); break;
		case TOK_STAR: makeop(state, OP_MUL, 0); break;
		case TOK_SLASH: makeop(state, OP_DIV, 0); break;
		default: break;
		}
		eval = --state->stacktop;
	}
	if (expr->type == AST_LITERAL) {
		char tmp[32];
		strncpy(tmp, expr->token.str, 32);
		tmp[31] = '\0';
		state->vm->data[state->globalhead].i = atoi(tmp);
		makeop(state, OP_PUSH0, VAR_INT);
		makeop(state, OP_LOAD, state->globalhead++);
		eval = ++state->stacktop;
	}

	return eval;
}

static void compile_stmt_list(struct state *state,
				struct ast_node *stmt,
				struct ast_node *fn) {
	struct scope *scope = state->scopes + state->currscope;

	switch (stmt->type) {
	case AST_LET:
		add_variable(state, stmt, 0);
		compile_expr(state, stmt->inner);
		break;
	case AST_EXPR:
		{
			int stacktop = state->stacktop;
			compile_expr(state, stmt->inner);
			makeop(state, OP_POPN, state->stacktop - stacktop);
			state->stacktop = stacktop;
		}
		break;
	case AST_RETURN:
		{
			if (stmt->inner && stmt->inner->token.type != TOK_TYPE_VOID) {
				compile_expr(state, stmt->inner);
				makeop(state, OP_TRANSFER, state->stacktop - scope->stackbase - 1);
				state->stacktop--;
			}
			makeop(state, OP_POPN, state->stacktop - scope->stackbase);
			makeop(state, OP_RET, 1);
		}
		break;
	default: break;
	}
}

static void compile_fn(struct state *state, struct ast_node *fn) {
	int fnloc = state->codehead;

	push_scope(state);
	// Add parameters to the scope
	{
		int *const nparams = &state->scopes[state->currscope].nparams;
		struct ast_node *param = fn->alt1;
		while (param) {
			add_argument(state, param, *nparams);
			param = param->next;
			(*nparams)++;
		}
	}

	// Compile the function
	compile_stmt_list(state, fn->inner->inner, fn);

	vm_addfn(state->vm, fn->token.str, fn->token.strlen, fnloc);
	pop_scope(state);
}

void compile_init(struct state *state,
		struct ast_node *ast,
		struct vm *vm,
		struct compile_error *err,
		size_t errsz) {
	*state = (struct state){
		.ast = ast,
		.vm = vm,
		.errs = err,
		.nerrs = 0,
		.errsz = errsz,
		.globalhead = 0,
	};
}

void compile_extern_fn(struct state *state,
		const char *name,
		vm_extern_pfn pfn) {
	//struct scope *scope = state->scopes + state->currscope;
	state->scopebuf[state->scopetop] = (struct scopevar){
		.name = (struct token){
			.str = name,
			.strlen = strlen(name),
		},
		.type = NULL,
		.is_externfn = 1,
		.absloc = -1,
		.dataloc = alloc_global(state, 1),
	};
	state->vm->data[state->scopebuf[state->scopetop].dataloc].p = pfn;
	state->scopetop++;
}

void compile(struct state *state) {
	struct ast_node *curr = state->ast;
	while (curr) {
		if (curr->type == AST_LET) compile_let(state, curr);
		if (curr->type == AST_FUNC_DEF) compile_fn(state, curr);

		curr = curr->next;
	}
}

