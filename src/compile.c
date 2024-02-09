#include <stdlib.h>
#include <string.h>

#include "compile.h"
#include "parser.h"

void emit_error(compiler_t *const state, err_t err) {
	if (state->nerrs == arrlen(state->errs) - 1) {
		state->errs[state->nerrs++] = (err_t){ .ty = err_toomany, .msg = "Too many errors, stopping.", .line = err.line, .chr = err.chr };
	} else if (state->nerrs < arrlen(state->errs)) {
		state->errs[state->nerrs++] = err;
	}

	// TODO: Continue to next statement I guess, it depends
	while (state->lexer.curr.ty != tok_semicol
		&& state->lexer.curr.ty != tok_eof
		&& state->lexer.curr.ty != tok_error) {
		lexer_next(&state->lexer);
	}
	lexer_next(&state->lexer);
}

void eat(compiler_t *const state, const tokty_t tokty, const char *const expect_msg) {
	if (state->lexer.curr.ty == tokty) {
		lexer_next(&state->lexer);
	} else {
		emit_error(state, (err_t){ .ty = err_expected, .msg = expect_msg, .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
	}
}

compiler_t compiler_init(
	const char *src,
	codeunit_t *code,
	size_t codelen,
	uint8_t *dataseg,
	size_t datalen
) {
	return (compiler_t){
		.lexer = lexer_init(src),
		.code = code,
		.code_end = code + codelen - 1,
		.data = dataseg,
		.data_end = dataseg + datalen - 1,
		.nerrs = 0,
		.stacktop = -1,
 	};
}

void compile(compiler_t *const state) {
	// Initialize the lexer
	if (!lexer_next(&state->lexer)) goto add_exit;
	parse_expression(state, prec_expr);

add_exit:
	// Safety exit
	if (state->stacktop < 0) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i8,
			.op1 = -1,
		}};
	}
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode_exit,
		.op1ty = unit_i32,
		.op1 = 0,
	}};
}

