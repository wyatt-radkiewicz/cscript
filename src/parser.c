#include "parser.h"

static valref_t expr_multiply(compiler_t *const state, const valref_t left) {
	dbgprintf("in multiplication\n");
	const tokty_t operator = state->lexer.curr.ty;
	lexer_next(&state->lexer);
	const valref_t right = expression(state, prec_grouping);
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = operator == tok_star ? opcode_mul : operator == tok_slash ? opcode_div : opcode_mod,
		.op1ty = left.ty,
		.op1 = state->stacktop - left.abs_idx,
		.op2ty = right.ty,
		.op2 = state->stacktop - right.abs_idx,
	}};
	return (valref_t){ .ty = left.ty, .abs_idx = ++state->stacktop };
}

static valref_t expr_additive(compiler_t *const state, const valref_t left) {
	dbgprintf("in addition\n");
	const tokty_t operator = state->lexer.curr.ty;
	lexer_next(&state->lexer);
	const valref_t right = expression(state, prec_multiplicative);
	*(state->code++) = (codeunit_t){ .op = (opcode_t){
		.ty = operator == tok_plus ? opcode_add : opcode_sub,
		.op1ty = left.ty,
		.op1 = state->stacktop - left.abs_idx,
		.op2ty = right.ty,
		.op2 = state->stacktop - right.abs_idx,
	}};
	return (valref_t){ .ty = left.ty, .abs_idx = ++state->stacktop };
}

valref_t expression(compiler_t *const state, prec_t prec) {
	dbgprintf("in expression with precedence %d\n", prec);
	// Prefix stuff
	valref_t left;
	switch (state->lexer.curr.ty) {
	case tok_minus:
		lexer_next(&state->lexer);
		left = expression(state, prec_grouping);
		*(state->code++) = (codeunit_t){ .op = (opcode_t){
			.ty = opcode_neg,
			.op1ty = left.ty,
			.op1 = state->stacktop - left.abs_idx,
		}};
		break;
	case tok_integer:
		left = number(state);
		lexer_next(&state->lexer);
		break;
	case tok_lparen:
		lexer_next(&state->lexer);
		left = expression(state, prec_expr);
		eat(state, tok_rparen, "right paren");
		break;
	default:
		emit_error(state, (err_t){ .ty = err_expected, "unary or number" });
		break;
	}

	// Binary stuff
	switch (state->lexer.curr.ty) {
	case tok_plus:
	case tok_minus:
		if (prec < prec_additive) break;
		return expr_additive(state, left);
	case tok_star:
	case tok_slash:
	case tok_percent:
		if (prec < prec_multiplicative) break;
		return expr_multiply(state, left);
	default:
	//	emit_error(state, (err_t){ .ty = err_expected, .msg = "binary" });
		break;
	}

	return left;
}

