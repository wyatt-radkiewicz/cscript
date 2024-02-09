#include <string.h>
#include <stdlib.h>

#include "parser.h"

static const parse_rule_t _rules[] = {
	// Keywords
	[tok_auto] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_bool] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_break] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_case] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_char] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_const] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_continue] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_default] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_do] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_double] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_else] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_enum] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_extern] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_false] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_float] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_for] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_goto] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_if] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_int] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_long] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_nullptr] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_restrict] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_return] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_short] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_signed] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_sizeof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_static] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_static_assert] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_struct] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_switch] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_true] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_typedef] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_typeof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_unsigned] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_void] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_volatile] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_while] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },

	// New keywords
	[tok_import] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	
	// Tokens (first and second chars)
	[tok_eof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_plus] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_additive },
	[tok_pluseq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_minus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_additive },
	[tok_minuseq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_star] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_dot] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_eq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_eqeq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_bitand] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_and] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_bitor] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_or] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_bitnot] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_not] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_xor] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lparen] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_rparen] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrack] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrach] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_gt] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_ge] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_shr] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_lt] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_le] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_shl] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_comma] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_char_lit] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_semicol] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_slash] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_percent] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },

	// Misc tokens
	[tok_string] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_integer] = (parse_rule_t){ .prefix = parse_number, .infix = NULL, .prec = prec_null },
	[tok_floating] = (parse_rule_t){ .prefix = parse_number, .infix = NULL, .prec = prec_null },
	[tok_ident] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_error] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_undefined] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
};

valref_t parse_unary(compiler_t *const state) {
	valref_t val = (valref_t){ .ty = unit_undefined, .abs_idx = 0 };
	switch (state->lexer.curr.ty) {
	case tok_lparen:
		lexer_next(&state->lexer);
		val = parse_expression(state, prec_expr);
		eat(state, tok_rparen, "right paren");
		break;
	case tok_minus:
		lexer_next(&state->lexer);
		val = parse_expression(state, prec_grouping);
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_neg,
			.op1ty = val.ty,
			.op1 = state->stacktop - val.abs_idx,
		}};
		break;
	default: break;
	}

	return val;
}
valref_t parse_binary(compiler_t *const state, const prec_t prec, valref_t left) {
	opcodety_t opcode;
	switch (state->lexer.curr.ty) {
	case tok_plus: opcode = opcode_add; break;
	case tok_minus: opcode = opcode_sub; break;
	case tok_star: opcode = opcode_mul; break;
	case tok_slash: opcode = opcode_div; break;
	case tok_percent: opcode = opcode_mod; break;
	case tok_shl: opcode = opcode_shl; break;
	case tok_shr: opcode = opcode_shr; break;
	default: opcode = opcode_exit; break;
	}
	lexer_next(&state->lexer);

	const valref_t right = parse_expression(state, prec - 1);
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode,
		.op1ty = left.ty,
		.op1 = state->stacktop - left.abs_idx,
		.op2ty = right.ty,
		.op2 = state->stacktop - right.abs_idx,
	}};

	return (valref_t){ .ty = left.ty, .abs_idx = ++state->stacktop };
}

valref_t parse_expression(
	compiler_t *const state,
	const prec_t prec
) {
	const parse_unary_pfn prefix = _rules[state->lexer.curr.ty].prefix;
	if (!prefix) {
		emit_error(state, (err_t){ .ty = err_expected, .msg = "unary expression", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return (valref_t){ .ty = unit_undefined, .abs_idx = 0 };
	}
	valref_t left = prefix(state);

	while (_rules[state->lexer.curr.ty].prec <= prec) {
		const parse_binary_pfn infix = _rules[state->lexer.curr.ty].infix;
		left = infix(state, _rules[state->lexer.curr.ty].prec, left);
	}
	return left;
}

valref_t parse_number(compiler_t *const state) {
	// TODO: bigger numbers and postfixes for numbers
	char numbuf[25];
	if (state->lexer.curr.len > 24) {
		emit_error(state, (err_t){ .ty = err_limit, .msg = "exceeded max literal integer size", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return (valref_t){ .ty = unit_undefined };
	}
	memcpy(numbuf, state->lexer.curr.lit, state->lexer.curr.len);
	numbuf[state->lexer.curr.len] = '\0';
	lexer_next(&state->lexer);
	int64_t i = atoll(numbuf);
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

