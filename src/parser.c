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
	[tok_eqeq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_bitand] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitand },
	[tok_and] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_and },
	[tok_bitor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitor },
	[tok_or] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_or },
	[tok_bitnot] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_noteq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_not] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_xor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitxor },
	[tok_lparen] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_rparen] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrack] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrach] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_gt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_ge] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shr] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_lt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_le] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
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

static valref_t push_cast_to_bool(
	compiler_t *const state,
	opcode_t test,
	opcodety_t branch
) {
	// Cast a conditional to a 1 or 0
	*(state->code++) = (codeunit_t){ .op = test };
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = branch,
		.op1ty = unit_i16,
		.imm16 = 2,
	}};
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode_pushimm,
		.op1ty = unit_i16,
		.imm16 = 0,
	}};
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode_bra,
		.op1ty = unit_i16,
		.imm16 = 1,
	}};
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = opcode_pushimm,
		.op1ty = unit_i16,
		.imm16 = 1,
	}};
	return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };
}

valref_t parse_unary(compiler_t *const state, parser_ctx_t ctx) {
	valref_t val = (valref_t){ .ty = unit_undefined, .abs_idx = 0 };
	switch (state->lexer.curr.ty) {
	case tok_lparen:
		lexer_next(&state->lexer);
		val = parse_expression(state, prec_assign, ctx);
		eat(state, tok_rparen, "right paren");
		break;
	case tok_minus:
		lexer_next(&state->lexer);
		val = parse_expression(state, prec_grouping, ctx);
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_neg,
			.op1ty = unit_u16,
			.op1 = state->stacktop - val.abs_idx,
			.op2ty = val.ty,
		}};
		break;
	case tok_bitnot:
		lexer_next(&state->lexer);
		val = parse_expression(state, prec_grouping, ctx);
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_not,
			.op1ty = val.ty,
			.op1 = state->stacktop - val.abs_idx,
		}};
		break;
	case tok_not:
		lexer_next(&state->lexer);
		val = parse_expression(state, prec_grouping, ctx);
		val = push_cast_to_bool(
			state,
			(opcode_t){
				.ty = opcode_tst,
				.op1ty = val.ty,
				.op1 = state->stacktop - val.abs_idx,
			},
			opcode_beq
		);
		break;
	default: break;
	}

	return val;
}

valref_t parse_binary(
	compiler_t *const state,
	const prec_t prec,
	valref_t left,
	parser_ctx_t ctx
) {
	opcodety_t opcode;
	const tokty_t tokty = state->lexer.curr.ty;
	switch (tokty) {
	case tok_plus: opcode = opcode_add; break;
	case tok_minus: opcode = opcode_sub; break;
	case tok_star: opcode = opcode_mul; break;
	case tok_slash: opcode = opcode_div; break;
	case tok_percent: opcode = opcode_mod; break;
	case tok_bitand: opcode = opcode_and; break;
	case tok_bitor: opcode = opcode_or; break;
	case tok_xor: opcode = opcode_eor; break;
	case tok_shl: opcode = opcode_shl; break;
	case tok_shr: opcode = opcode_shr; break;
	case tok_gt: opcode = opcode_bgt; break;
	case tok_ge: opcode = opcode_bge; break;
	case tok_lt: opcode = opcode_blt; break;
	case tok_le: opcode = opcode_ble; break;
	case tok_eqeq: opcode = opcode_beq; break;
	default: opcode = opcode_exit; break;
	}
	lexer_next(&state->lexer);

	if (tokty == tok_and) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_tst,
			.op1ty = left.ty,
			.op1 = state->stacktop - left.abs_idx,
		}};
		*state->code = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_beq,
			.op1ty = unit_i16,
			.imm16 = 2,
		}};
		codeunit_t *const branch1 = state->code++;

		const valref_t right = parse_expression(state, prec - 1, ctx);
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_tst,
			.op1ty = right.ty,
			.op1 = state->stacktop - right.abs_idx,
		}};
		*state->code = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_beq,
			.op1ty = unit_i16,
			.imm16 = 2,
		}};
		codeunit_t *const branch2 = state->code++;

		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i16,
			.imm16 = 1,
		}};
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_bra,
			.op1ty = unit_i16,
			.imm16 = 1,
		}};
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i16,
			.imm16 = 0,
		}};
		// Set the branches
		branch1->op.imm16 = state->code - branch1;
		branch2->op.imm16 = state->code - branch2;
		return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };
	} else if (tokty == tok_or) {
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_tst,
			.op1ty = left.ty,
			.op1 = state->stacktop - left.abs_idx,
		}};
		*state->code = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_bne,
			.op1ty = unit_i16,
			.imm16 = 2,
		}};
		codeunit_t *const branch1 = state->code++;

		const valref_t right = parse_expression(state, prec - 1, ctx);
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_tst,
			.op1ty = right.ty,
			.op1 = state->stacktop - right.abs_idx,
		}};
		*state->code = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_bne,
			.op1ty = unit_i16,
			.imm16 = 2,
		}};
		codeunit_t *const branch2 = state->code++;

		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i16,
			.imm16 = 0,
		}};
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_bra,
			.op1ty = unit_i16,
			.imm16 = 1,
		}};
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_i16,
			.imm16 = 1,
		}};
		// Set the branches
		branch1->op.imm16 = state->code - branch1;
		branch2->op.imm16 = state->code - branch2;
		return (valref_t){ .ty = unit_i32, .abs_idx = ++state->stacktop };

	} else if (_rules[tokty].prec == prec_eqeq
		|| _rules[tokty].prec == prec_relational) {
		const valref_t right = parse_expression(state, prec - 1, ctx);
		return push_cast_to_bool(
			state,
			(opcode_t){
				.ty = opcode_cmp,
				.op1ty = left.ty,
				.op1 = state->stacktop - left.abs_idx,
				.op2 = state->stacktop - right.abs_idx,
			},
			opcode
		);
	}
	const valref_t right = parse_expression(state, prec - 1, ctx);

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
	const prec_t prec,
	parser_ctx_t ctx
) {
	const parse_unary_pfn prefix = _rules[state->lexer.curr.ty].prefix;
	if (!prefix) {
		emit_error(state, (err_t){ .ty = err_expected, .msg = "unary expression", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return (valref_t){ .ty = unit_undefined, .abs_idx = 0 };
	}
	valref_t left = prefix(state, ctx);

	while (_rules[state->lexer.curr.ty].prec <= prec) {
		const parse_binary_pfn infix = _rules[state->lexer.curr.ty].infix;
		left = infix(state, _rules[state->lexer.curr.ty].prec, left, ctx);
	}
	return left;
}

valref_t parse_number(compiler_t *const state, parser_ctx_t ctx) {
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

