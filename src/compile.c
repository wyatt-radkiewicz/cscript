#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "compile.h"
#include "parser.h"

static unitty_t compile_rvalue_type(compiler_t *compiler, ast_t *node) {
	switch (node->info.type.ty) {
	case decl_pointer_to: return unit_ptr;
	case decl_pod: return node->info.type.pod;
	default: return unit_i32;
	}
}
static int compile_toplvl_decl(compiler_t *compiler, ast_t *node, strview_t name) {
	func_t *func = compiler->funcs + compiler->func_names->len;
	ident_map_add(
		compiler->func_names,
		name.str,
		name.len,
		compiler->func_names->len
	);

	func->def = NULL;
	func->ret = compile_rvalue_type(compiler, node->info.type.inner);
	ast_t *curr = node->info.type.funcparams;
	while (curr) {
		func->params[func->nparams] = compile_rvalue_type(compiler, curr->info.decldef.type);
		func->param_names[func->nparams] = (strview_t){
			.str = curr->tok.lit,
			.len = curr->tok.len,
		};
		curr = curr->next;
		func->nparams++;
	}

	return compiler->func_names->len - 1;
}
static valref_t push_to_scope(
	compiler_t *const compiler,
	unitty_t ty,
	const char *str,
	size_t strlen
) {
	valref_t *ref = compiler->scope->vars + compiler->scope->names->len;
	ident_map_add(
		compiler->scope->names,
		str,
		strlen,
		compiler->scope->names->len
	);
	ref->ty = ty;
	ref->abs_idx = ++compiler->stacktop;
	return *ref;
}
static void clear_scope(
	compiler_t *compiler,
	scope_t *scope
) {
	*scope->names = ident_map_init(MAX_VARS);
	scope->stacktop = compiler->stacktop;
}
static void pop_scope(compiler_t *compiler, bool emit_code) {
	if (emit_code) {
		*(compiler->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_add_stack,
			.op1ty = unit_u16,
			.imm16 = compiler->stacktop - (compiler->scope - 1)->stacktop,
		}};
	}
	compiler->stacktop = (--compiler->scope)->stacktop;
}
static void compile_expr(compiler_t *compiler, ast_t *node) {
	
}
static void compile_stmt(compiler_t *compiler, ast_t *node) {
	
}
static void compile_toplvl_def(compiler_t *compiler, ast_t *node, func_t *decl) {
	// Add the parameters to the scope
	for (int i = 0; i < decl->nparams; i++) {
		push_to_scope(
			compiler,
			decl->params[i],
			decl->param_names[i].str,
			decl->param_names[i].len
		);
	}
	clear_scope(compiler, ++compiler->scope);
	compile_stmt(compiler, node);
	pop_scope(compiler, false);
}

static void traverse(compiler_t *compiler, ast_t *node, bool toplvl) {
	if (!node) return;

	switch (node->ty) {
	case ast_decldef:
		compile_toplvl_def(
			compiler,
			node->info.decldef.def,
			compiler->funcs + compile_toplvl_decl(
				compiler,
				node->info.decldef.type,
				(strview_t){
					.str = node->tok.lit,
					.len = node->tok.len,
				}
			)
		);
		break;
	default: break;
	}

	traverse(compiler, node->next, toplvl);
}

compiler_t *compile(
	ast_t *ast,
	codeunit_t *code,
	size_t codelen,
	uint8_t *dataseg,
	size_t datalen
) {
	compiler_t *compiler = arena_alloc(sizeof(*compiler));
	*compiler = (compiler_t){
		.code = code,
		.code_end = code + codelen,
		.data = dataseg,
		.data_end = dataseg + datalen,
		.errs = {{0}},
		.nerrs = 0,
		.stacktop = -1,
		.func_names = arena_alloc(sizeof(ident_map_t) + sizeof(ident_ent_t) * MAX_FUNCS),
	};

	*compiler->func_names = ident_map_init(MAX_FUNCS);
	compiler->scopes = arena_alloc(sizeof(*compiler->scopes) * MAX_SCOPES);
	for (int i = 0; i < MAX_SCOPES; i++) {
		compiler->scopes[i].names = arena_alloc(sizeof(ident_map_t) + sizeof(ident_ent_t) * MAX_VARS),
		clear_scope(compiler, compiler->scopes + i);
	}
	compiler->scope = compiler->scopes;

	traverse(compiler, ast, true);

	// Saftey exit code
	*(compiler->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode_pushimm,
		.op1ty = unit_i16,
		.imm16 = -1,
	}};
	*(compiler->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode_exit,
		.op1ty = unit_i32,
		.op1 = 0,
	}};

	return compiler;
}
