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
	[tok_ident] = (parse_rule_t){ .prefix = parse_ident, .infix = NULL, .prec = prec_null },
	[tok_error] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_undefined] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
};

void parser_err(
	parser_t *const state,
	err_t err
) {
	if (state->nerrs == state->buflen - 1) {
		state->errs[state->nerrs++] = (err_t){ .ty = err_toomany, .msg = "Too many errors, stopping.", .line = err.line, .chr = err.chr };
	} else if (state->nerrs < state->buflen) {
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

static void parser_eat(parser_t *const state, const tokty_t tokty, const char *const expect_msg) {
	if (state->lexer.curr.ty == tokty) {
		lexer_next(&state->lexer);
	} else {
		parser_err(state, (err_t){ .ty = err_expected, .msg = expect_msg, .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
	}
}

int parse_unary(parser_t *const state) {
	const tok_t tok = state->lexer.curr;
	int val = AST_SENTINAL;
	lexer_next(&state->lexer);
	switch (tok.ty) {
	case tok_lparen:
		val = parse_expression(state, prec_assign);
		parser_eat(state, tok_rparen, "right paren");
		break;
	default:
		val = parse_expression(state, prec_grouping);
		break;
	}
	return parser_add(state, (ast_t){
		.ty = ast_unary,
		.tok = tok,
		.inner = val,
	});
}

int parse_binary(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	const int right = parse_expression(state, prec - 1);
	return parser_add(
		state,
		(ast_t){
			.ty = ast_binary,
			.tok = tok,
			.left = left,
			.right = right,
		}
	);
}

parser_t parse(const char *src, ast_t *astbuf, size_t buflen) {
	parser_t state = (parser_t){
		.buflen = buflen,
		.buf = astbuf,
		.nnodes = 0,
		.lexer = lexer_init(src),
		.nerrs = 0,
	};
	lexer_next(&state.lexer);
	state.root = parse_expression(&state, prec_full);
	return state;
}

int parse_expression(
	parser_t *const state,
	const prec_t prec
) {
	const parse_unary_pfn prefix = _rules[state->lexer.curr.ty].prefix;
	if (!prefix) {
		parser_err(state, (err_t){ .ty = err_expected, .msg = "unary expression", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return AST_SENTINAL;
	}
	int left = prefix(state);

	while (_rules[state->lexer.curr.ty].prec <= prec) {
		const parse_binary_pfn infix = _rules[state->lexer.curr.ty].infix;
		left = infix(state, _rules[state->lexer.curr.ty].prec, left);
	}
	return left;
}

int parser_add(parser_t *const state, const ast_t newnode) {
	if (state->nnodes == state->buflen) {
		parser_err(state, (err_t){ .ty = err_limit, .msg = "max ast nodes reached", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return AST_SENTINAL;
	}
	state->buf[state->nnodes] = newnode;
	return state->nnodes++;
}
int parse_number(parser_t *const state) {
	// TODO: bigger numbers and postfixes for numbers
	char numbuf[25];
	const tok_t tok = state->lexer.curr;
	if (tok.len > 24) {
		parser_err(state, (err_t){ .ty = err_limit, .msg = "exceeded max literal integer size", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return AST_SENTINAL;
	}
	memcpy(numbuf, tok.lit, tok.len);
	numbuf[tok.len] = '\0';
	lexer_next(&state->lexer);
	int64_t i = atoll(numbuf);
	if (i <= INT32_MAX) {
		return parser_add(state, (ast_t){
			.ty = ast_literal,
			.tok = tok,
			.val = (typed_unit_t){
				.ty = unit_i32,
				.unit = (unit_t){ .i32 = i },
			},
		});
	} else if (i <= UINT32_MAX) {
		return parser_add(state, (ast_t){
			.ty = ast_literal,
			.tok = tok,
			.val = (typed_unit_t){
				.ty = unit_u32,
				.unit = (unit_t){ .u32 = i },
			},
		});
	} else if (i <= INT64_MAX) {
		return parser_add(state, (ast_t){
			.ty = ast_literal,
			.tok = tok,
			.val = (typed_unit_t){
				.ty = unit_i64,
				.unit = (unit_t){ .i64 = i },
			},
		});
	} else {
		return parser_add(state, (ast_t){
			.ty = ast_literal,
			.tok = tok,
			.val = (typed_unit_t){
				.ty = unit_u64,
				.unit = (unit_t){ .u64 = i },
			},
		});
	}
}
int parse_ident(parser_t *const state) {
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_ident,
		.tok = state->lexer.prev[2],
	});
}

static void _dbg_ast_print(const ast_t *const ast, int idx, int tabs) {
#define TAB for (int i = 0; i < tabs; i++) printf("\t");
	TAB printf("node: %s\n", tokty_to_str(ast[idx].tok.ty));
	switch (ast[idx].ty) {
	case ast_binary:
		_dbg_ast_print(ast, ast[idx].left, tabs+1);
		_dbg_ast_print(ast, ast[idx].right, tabs+1);
		break;
	case ast_unary:
		_dbg_ast_print(ast, ast[idx].inner, tabs+1);
		break;
	case ast_literal:
		TAB dbg_typed_unit_print(ast[idx].val);
		break;
	case ast_ident:
		{
			TAB
			printf("name: ");
			for (int i = 0; i < ast[idx].tok.line; i++) printf("%c", ast[idx].tok.lit[i]);
			printf("\n");
		}
		break;
	default: break;
	}
#undef TAB
}
void dbg_ast_print(const ast_t *const astbuf, int root) {
	_dbg_ast_print(astbuf, root, 0);
}

