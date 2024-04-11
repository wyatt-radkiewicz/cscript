#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "compile.h"

static struct vm_code *makeop(struct state *state, int op, int arg) {
	if ((op == OP_TRANSFER || op == OP_POPN) && arg == 0) return NULL;
	state->vm->code[state->codehead] = (struct vm_code){ .op = op, .arg = arg };
	//vm_print_opcode(state->vm->code[state->codehead]);
	return state->vm->code + state->codehead++;
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
	state->stacktop = state->scopes[state->currscope].stackbase;
	state->scopetop = state->scopes[state->currscope].scopebase;
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
	//if (!global) state->stacktop += get_size(state, vardef->inner);
	state->scopetop++;
	return loc;
}

static void add_argument(struct state *state, struct ast_node *vardef, int param_idx) {
	struct scope *scope = state->scopes + state->currscope;
	state->scopebuf[state->scopetop] = (struct scopevar){
		.name = vardef->token,
		.type = vardef->inner,
		// Add 1 because of return value
		.absloc = scope->stackbase - (param_idx + 1),
		.dataloc = -1,
	};
	state->scopetop++;
}

static int get_podtype(struct state *state, struct ast_node *type) {
	if (!type) return VAR_VOID;
	if (type->type == AST_IDENT) {
		printf("IMPLEMENT TYPEDEFS (and structs)!!");
		return -1;
	}
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

// Returns location of variable on the stack
// If value is a global, it will load it onto the stack
static int get_value(struct state *state, struct scoperef *val, enum vm_varty finalty) {
	if (val->isglobal) {
		makeop(state, OP_PUSH0, val->podtype);
		makeop(state, OP_LOAD, val->absloc);
		if (finalty != VAR_VOID && finalty != val->podtype) makeop(state, OP_CAST, finalty);
		return ++state->stacktop;
	} else if (val->absloc != state->stacktop || val->lvalue) {
		makeop(state, OP_REPUSH, state->stacktop - val->absloc);
		if (finalty != VAR_VOID && finalty != val->podtype) makeop(state, OP_CAST, finalty);
		return ++state->stacktop;
	}

	return val->absloc;
}

static struct scoperef compile_expr(struct state *state, struct ast_node *expr, enum vm_varty finalty) {
	if (finalty == VAR_CHAR) finalty = VAR_INT;
	struct scoperef eval = {
		.absloc = 0,
		.isglobal = false,
		.podtype = finalty,
		.lvalue = false,
		.isvoid = false,
	};

	if (expr->type == AST_BINARY) {
		struct scoperef left = compile_expr(state, expr->alt1, finalty);
		if (expr->token.type != TOK_EQ) get_value(state, &left, finalty);
		struct scoperef right = compile_expr(state, expr->alt2, finalty);
		get_value(state, &right, finalty);
		switch (expr->token.type) {
		case TOK_PLUS: makeop(state, OP_ADD, 0); break;
		case TOK_DASH: makeop(state, OP_SUB, 0); break;
		case TOK_STAR: makeop(state, OP_MUL, 0); break;
		case TOK_SLASH: makeop(state, OP_DIV, 0); break;
		case TOK_BIT_AND: makeop(state, OP_AND, 0); break;
		case TOK_BIT_OR: makeop(state, OP_OR, 0); break;
		case TOK_BIT_XOR: makeop(state, OP_EOR, 0); break;
		case TOK_LSHIFT: makeop(state, OP_SHL, 0); break;
		case TOK_RSHIFT: makeop(state, OP_SHR, 0); break;
		case TOK_AND: makeop(state, OP_TAND, 0); break;
		case TOK_OR: makeop(state, OP_TOR, 0); break;
		case TOK_EQEQ: makeop(state, OP_TEQ, 0); break;
		case TOK_NOTEQ: makeop(state, OP_TNE, 0); break;
		case TOK_GT: makeop(state, OP_TGT, 0); break;
		case TOK_GE: makeop(state, OP_TGE, 0); break;
		case TOK_LT: makeop(state, OP_TLT, 0); break;
		case TOK_LE: makeop(state, OP_TLE, 0); break;
		case TOK_EQ:
			if (!left.lvalue) {
				eval.isvoid = true;
				printf("left must be lvalue dipshit\n");
				return eval;
			}
			
			if (left.isglobal) {
				makeop(state, OP_STORE, left.absloc);
				makeop(state, OP_POPN, 1);
				state->stacktop--;
				eval.absloc = left.absloc;
				eval.isglobal = true;
			} else {
				makeop(state, OP_TRANSFER, state->stacktop - left.absloc);
				makeop(state, OP_POPN, 1);
				eval.absloc = --state->stacktop;
			}
			break;
		default: break;
		}
		if (expr->token.type != TOK_EQ) eval.absloc = --state->stacktop;
	}
	if (expr->type == AST_LITERAL) {
		char tmp[32];
		strncpy(tmp, expr->token.str, 32);
		tmp[31] = '\0';
		bool iszero = false;
		if (finalty == VAR_FLOAT) {
			int f = atof(tmp);
			iszero = f == 0.0f;
			if (!iszero) state->vm->data[state->globalhead].f = f;
			eval.podtype = VAR_FLOAT;
			makeop(state, OP_PUSH0, VAR_FLOAT);
		} else if (finalty == VAR_UINT) {
			int u = (unsigned int)atoll(tmp);
			iszero = !u;
			if (!iszero) state->vm->data[state->globalhead].u = u;
			eval.podtype = VAR_UINT;
			makeop(state, OP_PUSH0, VAR_UINT);
		} else {
			int i = atoi(tmp);
			iszero = !i;
			if (!iszero) state->vm->data[state->globalhead].i = i;
			eval.podtype = VAR_INT;
			makeop(state, OP_PUSH0, VAR_INT);
		}
		if (!iszero) {
			makeop(state, OP_LOAD, state->globalhead++);
		}
		eval.absloc = ++state->stacktop;
	}
	if (expr->type == AST_IDENT) {
		struct scopevar *ident = NULL;
		for (int i = state->scopetop; i > -1; i--) {
			struct scopevar *curr = &state->scopebuf[i];
			if (curr->name.strlen != expr->token.strlen
				|| memcmp(curr->name.str, expr->token.str, curr->name.strlen) != 0)
				continue;
			ident = curr;
			break;
		}
		if (!ident) {
			printf("DO ident error here :)\n");
			printf("line: %d\n", expr->token.line);
			eval.isvoid = true;
			return eval;
		}
		eval.podtype = get_podtype(state, ident->type);
		if (ident->dataloc != -1) {
			eval.isglobal = true;
			eval.absloc = ident->dataloc;
		} else {
			eval.absloc = ident->absloc;
		}
		eval.lvalue = true;
	}
	if (expr->type == AST_CALL) {
		struct scopevar *func = NULL;
		int end = state->currscope ? state->scopes[1].scopebase : state->scopetop + 1;
		for (int i = 0; i < end; i++) {
			struct scopevar *curr = &state->scopebuf[i];
			if (curr->name.strlen != expr->token.strlen
				|| memcmp(curr->name.str, expr->token.str, curr->name.strlen) != 0)
				continue;
			func = curr;
			break;
		}
		if (!func) {
			printf("DO error here :)\n");
			printf("line: %d\n", expr->token.line);
			eval.isvoid = true;
			return eval;
		}

		// Push parameters onto stack
		struct ast_node *arg = expr->inner, *param = func->type->alt1;
		int stacktop = state->stacktop;
		while (arg) {
			enum vm_varty pty = get_podtype(state, param->inner);
			struct scoperef ref = compile_expr(state, arg, pty);
			get_value(state, &ref, pty);
			arg = arg->next;
			param = param->next;
			if (!!param != !!arg) {
				printf("bruh wrong num of args xDDD\n");
				return eval;
			}
		}
		if (func->is_externfn) {
			makeop(state, OP_PUSH0, VAR_PTR);
			makeop(state, OP_LOAD, func->dataloc);
			makeop(state, OP_EXTERN_CALL, 0);
		} else {
			makeop(state, OP_CALL, func->dataloc);
		}
		// Transfer the return variable
		if (func->type->alt2 && func->type->alt2->token.type != TOK_TYPE_VOID) {
			state->stacktop += 1;
			stacktop++;
			makeop(state, OP_TRANSFER, state->stacktop - stacktop);
			eval.absloc = stacktop;
			eval.lvalue = false;
			eval.isglobal = false;
			eval.podtype = get_podtype(state, func->type->alt2);
		} else {
			eval.isvoid = true;
		}
		
		makeop(state, OP_POPN, state->stacktop - stacktop);
		state->stacktop = stacktop;
	}

	return eval;
}

static void compile_stmt_list(struct state *state,
				struct ast_node *stmt,
				struct ast_node *fn) {
	struct scope *scope = state->scopes + state->currscope;

	while (stmt) {
		switch (stmt->type) {
		case AST_LET:
			{
				struct scoperef ref = compile_expr(state, stmt->alt1, get_podtype(state, stmt->inner));
				enum vm_varty ty = get_podtype(state, stmt->inner);
				if (ref.podtype != ty) makeop(state, OP_CAST, ty);
				//implicit_convert(state, &ref, get_podtype(state, stmt->inner));
				add_variable(state, stmt, 0);
				//makeop(state, OP_PUSH0, get_podtype(state, stmt->inner));
			}
			break;
		case AST_EXPR:
			{
				int stacktop = state->stacktop;
				compile_expr(state, stmt->inner, VAR_VOID);
				makeop(state, OP_POPN, state->stacktop - stacktop);
				state->stacktop = stacktop;
			}
			break;
		case AST_IF:
			{
				compile_expr(state, stmt->inner, VAR_VOID);
				struct vm_code *branch = makeop(state, OP_BEQ, 0);
				state->stacktop--;

				push_scope(state);
				int stacktop = state->stacktop;
				compile_stmt_list(state, stmt->alt1->inner, fn);
				makeop(state, OP_POPN, state->stacktop - stacktop);
				state->stacktop = stacktop;
				pop_scope(state);

				branch->arg = state->codehead;
			}
			break;
		case AST_WHILE:
			{
				int start = state->codehead;
				compile_expr(state, stmt->inner, VAR_VOID);
				struct vm_code *branch = makeop(state, OP_BEQ, 0);
				state->stacktop--;

				push_scope(state);
				int stacktop = state->stacktop;
				compile_stmt_list(state, stmt->alt1->inner, fn);
				makeop(state, OP_POPN, state->stacktop - stacktop);
				state->stacktop = stacktop;
				pop_scope(state);

				makeop(state, OP_JMP, start);
				branch->arg = state->codehead;
			}
			break;
		case AST_RETURN:
			{
				if (stmt->inner && stmt->inner->token.type != TOK_TYPE_VOID) {
					compile_expr(state, stmt->inner, get_podtype(state, fn->alt2));
					makeop(state, OP_TRANSFER, state->stacktop - scope->stackbase - 1);
					state->stacktop--;
					makeop(state, OP_POPN, state->stacktop - scope->stackbase);
					makeop(state, OP_RET, 1);
					state->stacktop = scope->stackbase;
				} else {
					makeop(state, OP_POPN, state->stacktop - scope->stackbase);
					makeop(state, OP_RET, 0);
					state->stacktop = scope->stackbase;
				}
			}
			break;
		default: break;
		}

		stmt = stmt->next;
	}
}

static void compile_fn(struct state *state, struct ast_node *fn) {
	int fnloc = state->codehead;

	push_scope(state);
	struct scope *scope = state->scopes + state->currscope;
	// Add parameters to the scope
	{
		scope->nparams = 0;
		struct ast_node *param = fn->alt1;
		while (param) {
			param = param->next;
			scope->nparams++;
		}

		// We do this so that arguments can be pushed onto the stack
		// from left to right.
		int i = scope->nparams - 1;
		param = fn->alt1;
		while (param) {
			add_argument(state, param, i);
			param = param->next;
			i--;
		}
	}

	// Compile the function
	compile_stmt_list(state, fn->inner->inner, fn);

	// Compile a implicit return
	makeop(state, OP_POPN, state->stacktop - scope->stackbase);
	state->stacktop = scope->stackbase;
	makeop(state, OP_RET, 0);

	vm_addfn(state->vm, fn->token.str, fn->token.strlen, fnloc);
	pop_scope(state);

	assert(state->currscope == 0);

	state->scopebuf[state->scopetop++] = (struct scopevar){
		.name = fn->token,
		.type = fn,
		.is_externfn = 0,
		.absloc = -1,
		.dataloc = fnloc,
	};
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

static void compile_extern_fn_sig(struct state *state,
				struct ast_node *fn) {
	struct scopevar *func = NULL;
	int end = state->currscope ? state->scopes[1].scopebase : state->scopetop + 1;
	for (int i = 0; i < end; i++) {
		struct scopevar *curr = &state->scopebuf[i];
		if (curr->name.strlen != fn->token.strlen
			|| memcmp(curr->name.str, fn->token.str, curr->name.strlen) != 0)
			continue;
		func = curr;
		break;
	}
	if (!func) {
		printf("extern function not declared.... :)\n");
		return;
	}
	func->type = fn;
}

void compile_extern_fn(struct state *state,
		const char *name,
		vm_extern_pfn pfn) {
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
		if (curr->type == AST_EXTERN_OF) {
			compile_extern_fn_sig(state, curr->inner);
		}

		curr = curr->next;
	}
}

