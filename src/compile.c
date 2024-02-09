#include <stdlib.h>
#include <string.h>

#include "compile.h"
#include "parser.h"

void emit_error(compiler_t *const state, err_t err) {
	if (state->nerrs == arrlen(state->errs) - 1) {
		state->errs[state->nerrs++] = (err_t){ .ty = err_toomany, .msg = "Too many errors, stopping." };
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
	if (state->lexer.curr.ty != tokty) {
		emit_error(state, (err_t){ .ty = err_expected, .msg = expect_msg });
	} else {
		lexer_next(&state->lexer);
	}
}

valref_t number(compiler_t *const state) {
	// TODO: bigger numbers and postfixes for numbers
	char numbuf[25];
	if (state->lexer.curr.len > 24) {
		emit_error(state, (err_t){ .ty = err_limit, .msg = "exceeded max literal integer size" });
		return (valref_t){ .ty = unit_undefined };
	}
	memcpy(numbuf, state->lexer.curr.lit, state->lexer.curr.len);
	numbuf[state->lexer.curr.len] = '\0';
	int64_t i = atoll(numbuf);
	dbgprintf("DBG: creating num: %lld\n", i);
	if (i <= INT16_MAX) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i16,
			.imm16 = i,
		}};
		return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };
	} else if (i <= UINT16_MAX) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_u16,
			.umm16 = i,
		}};
		return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };
	} else if (i <= INT32_MAX) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i32,
		}};
		*(state->code++) = (codeunit_t){ .i32 = i };
		return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };
	} else if (i <= UINT32_MAX) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_u32,
		}};
		*(state->code++) = (codeunit_t){ .u32 = i };
		return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };
	} else {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = i <= INT64_MAX ? unit_i64 : unit_u64,
		}};
		*(state->code++) = (codeunit_t){ .u32 = ((uint32_t *)&i)[0] };
		*(state->code++) = (codeunit_t){ .u32 = ((uint32_t *)&i)[1] };
		return (valref_t){ .ty = i <= INT64_MAX ? unit_i64 : unit_u64, .abs_idx = ++state->stacktop };
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
	expression(state, prec_expr);

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

