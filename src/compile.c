#include <stdlib.h>
#include <string.h>

#include "compile.h"
#include "parser.h"

void compile_err(compiler_t *const state, int ast_node, err_t err) {
	if (state->nerrs == arrlen(state->errs) - 1) {
		state->errs[state->nerrs++] = (err_t){ .ty = err_toomany, .msg = "Too many errors, stopping.", .line = err.line, .chr = err.chr };
	} else if (state->nerrs < arrlen(state->errs)) {
		state->errs[state->nerrs++] = err;
	}
}

compiler_t compile(
	const parser_t *parser,
	codeunit_t *code,
	size_t codelen,
	uint8_t *dataseg,
	size_t datalen
) {
	compiler_t compiler = (compiler_t){
		.parser = *parser,
		.code = code,
		.code_end = code + codelen - 1,
		.data = dataseg,
		.data_end = dataseg + datalen - 1,
		.nerrs = 0,
		.stacktop = -1,
		.brefs_top = -1,
		.btargets_top = -1,
 	};

	for (size_t i = 0; i < arrlen(compiler.branch_refs); i++) {
		compiler.branch_refs[i] = (branchref_t){ .branch = NULL, .branch_target = -1 };
	}
	for (size_t i = 0; i < arrlen(compiler.branch_targets); i++) {
		compiler.branch_targets[i] = NULL;
	}

	return compiler;
}

