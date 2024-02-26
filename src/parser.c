#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"

// Will parse the type and either return
// just the ast_type with type info
// or the ast_decl with type info and identifier
static int parse_statement(parser_t *const state);
static int parse_initializer(parser_t *const state, const bool allow_desginator);
static int parse_type(parser_t *const state, const bool multi_decl, const bool allow_idents);
static typed_unit_t parse_number(parser_t *const state);
static void parse_toplevels(parser_t *const state);
static int parse_expression(parser_t *const state, const prec_t prec);
static parse_unary_fn parse_access_prefix;
static parse_binary_fn parse_access_post;
static parse_unary_fn parse_ident;
static parse_unary_fn parse_number_lit;
static parse_unary_fn parse_string_lit;
static parse_unary_fn parse_unary;
static parse_binary_fn parse_binary;
static parse_binary_fn parse_call;
static parse_binary_fn parse_comma;
static parse_binary_fn parse_eq;

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
	[tok_static] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_static_assert] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_struct] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_switch] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_true] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_typedef] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_typeof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_union] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_unsigned] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_void] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_volatile] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_while] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	
	// Tokens (first and second chars)
	[tok_eof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_plus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_additive },
	[tok_plusplus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_2 },
	[tok_minus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_additive },
	[tok_minusminus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_2 },
	[tok_star] = (parse_rule_t){ .prefix = parse_access_prefix, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_dot] = (parse_rule_t){ .prefix = NULL, .infix = parse_access_post, .prec = prec_grouping },
	[tok_arrow] = (parse_rule_t){ .prefix = NULL, .infix = parse_access_post, .prec = prec_grouping },
	[tok_eq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_pluseq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_minuseq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_stareq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_slasheq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_bitandeq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_bitoreq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_xoreq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_shreq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_shleq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_eqeq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_bitand] = (parse_rule_t){ .prefix = parse_access_prefix, .infix = parse_binary, .prec = prec_bitand },
	[tok_and] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_and },
	[tok_bitor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitor },
	[tok_or] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_or },
	[tok_bitnot] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_noteq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_not] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_xor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitxor },
	[tok_lparen] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_call, .prec = prec_grouping },
	[tok_rparen] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrack] = (parse_rule_t){ .prefix = parse_access_prefix, .infix = parse_access_post, .prec = prec_grouping },
	[tok_rbrack] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_gt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_ge] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shr] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_lt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_le] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shl] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_comma] = (parse_rule_t){ .prefix = NULL, .infix = parse_comma, .prec = prec_comma },
	[tok_semicol] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_slash] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_percent] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },

	// Misc tokens
	[tok_string] = (parse_rule_t){ .prefix = parse_string_lit, .infix = NULL, .prec = prec_null },
	[tok_char_lit] = (parse_rule_t){ .prefix = parse_string_lit, .infix = NULL, .prec = prec_null },
	[tok_integer] = (parse_rule_t){ .prefix = parse_number_lit, .infix = NULL, .prec = prec_null },
	[tok_floating] = (parse_rule_t){ .prefix = parse_number_lit, .infix = NULL, .prec = prec_null },
	[tok_ident] = (parse_rule_t){ .prefix = parse_ident, .infix = NULL, .prec = prec_null },
	[tok_error] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_undefined] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_sizeof] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_2 },
	[tok_qmark] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_colon] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_doublehash] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_hash] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
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

static bool parser_eat(parser_t *const state, const tokty_t tokty, const char *const expect_msg) {
	if (state->lexer.curr.ty == tokty) {
		lexer_next(&state->lexer);
		return true;
	} else {
		const err_t err = (err_t){ .ty = err_expected, .msg = expect_msg, .line = state->lexer.curr.line, .chr = state->lexer.curr.chr };
		parser_err(state, err);
		return false;
	}
}

static int _parse_cast(parser_t *const state, const int type) {
	parser_eat(state, tok_rparen, "\")\"");
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	int init = state->lexer.curr.ty == tok_lbrace ? parse_initializer(state, false) : AST_SENTINAL;
	if (init == AST_SENTINAL) {
		const int expr = parse_expression(state, prec_2);
		if (expr != AST_SENTINAL) {
			return parser_add(state, (ast_t){
				.ty = ast_cast,
				.tok = (tok_t){ .ty = tok_undefined },
				.next = AST_SENTINAL,
				.info.cast = {
					.type = type,
					.expr = expr,
				},
				.line = line,
				.chr = chr,
			});
		} else {
			return type;
		}
	} else {
		return parser_add(state, (ast_t){
			.ty = ast_literal,
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.info.literal = {
				.ty = lit_compound,
				.val.compound = {
					.type = type,
					.init = init,
				},
			},
			.line = line,
			.chr = chr,
		});
	}
}

static int parse_unary(parser_t *const state) {
	const tok_t tok = state->lexer.curr;
	int val = AST_SENTINAL;
	lexer_next(&state->lexer);
	switch (tok.ty) {
	case tok_lparen:
		{
			const int type = parse_type(state, false, false);
			if (type != AST_SENTINAL) return _parse_cast(state, type);
			val = parse_expression(state, prec_assign);
			parser_eat(state, tok_rparen, "right paren");
			break;
		}
	default:
		val = parse_expression(state, prec_grouping);
		break;
	}
	return parser_add(state, (ast_t){
		.ty = ast_unary,
		.tok = tok,
		.info.inner = val,
		.next = AST_SENTINAL,
		.line = tok.line,
		.chr = tok.chr,
	});
}

static int parse_comma(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_comma,
		.tok = tok,
		.next = AST_SENTINAL,
		.info.comma.expr = left,
		.info.comma.next = parse_expression(state, prec_comma-1),
		.line = tok.line,
		.chr = tok.chr,
	});
}

static int parse_call(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	lexer_next(&state->lexer);
	int params = AST_SENTINAL;
	int *last = &params;
	while (state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		const int param = parse_expression(state, prec_ternary);
		*last = param;
		last = &state->buf[param].next;
		if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
		else break;
	}
	parser_eat(state, tok_rparen, "\")\"");
	return parser_add(
		state,
		(ast_t){
			.ty = ast_func_call,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.func_call.func = left,
			.info.func_call.params = params,
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		}
	);
}

static int parse_eq(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	const int right = parse_expression(state, prec);
	return parser_add(
		state,
		(ast_t){
			.ty = ast_eq,
			.tok = tok,
			.info.binary.left = left,
			.info.binary.right = right,
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		}
	);
}

static int parse_binary(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	const int right = parse_expression(state, (tok.ty == tok_minusminus || tok.ty == tok_plusplus) ? prec_lowest : prec - 1);
	return parser_add(
		state,
		(ast_t){
			.ty = ast_binary,
			.tok = tok,
			.info.binary.left = left,
			.info.binary.right = right,
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		}
	);
}

static int parse_access_prefix(parser_t *const state) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);

	if (tok.ty == tok_lbrack) {
		const int arridx = parse_expression(state, prec_full);
		parser_eat(state, tok_rbrack, "\"]\"");
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.accessor = {
				.access_from = AST_SENTINAL,
				.accessing = arridx,
			},
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		});
	} else {
		const int inner = parse_expression(state, prec_2);
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.inner = inner,
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		});
	}
}
static int parse_access_post(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	if (tok.ty == tok_dot || tok.ty == tok_arrow) {
		const int right = parse_expression(state, prec_lowest);
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.accessor = {
				.access_from = left,
				.accessing = right,
			},
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		});
	} else {
		const int arridx = parse_expression(state, prec_full);
		parser_eat(state, tok_rbrack, "\"]\"");
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.accessor = {
				.access_from = left,
				.accessing = arridx,
			},
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		});
	}
}
static int parse_ident(parser_t *const state) {
	const tok_t name = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_ident,
		.tok = name,
		.next = AST_SENTINAL,
		.line = name.line,
		.chr = name.chr,
	});
}

parser_t parse(const char *src, ast_t *astbuf, size_t buflen) {
	parser_t state = (parser_t){
		.buflen = buflen,
		.buf = astbuf,
		.nnodes = 0,
		.nerrs = 0,
		.root = AST_SENTINAL,
		.lexer = lexer_init(src),
	};
	state.tymap = ident_map_init(arrlen(state.tymap_ents));
	lexer_next(&state.lexer);
	parse_toplevels(&state);
	return state;
}

static cvrflags_t parse_cvrflags(parser_t *const state) {
	cvrflags_t cvrs = cvr_none;
	while (true) {
		switch (state->lexer.curr.ty) {
		case tok_static: cvrs |= cvr_static; break;
		case tok_const: cvrs |= cvr_const; break;
		case tok_restrict: cvrs |= cvr_restrict; break;
		case tok_volatile: cvrs |= cvr_volatile; break;
		case tok_extern: cvrs |= cvr_extern; break;
		default: return cvrs;
		}
		lexer_next(&state->lexer);
	}
}

static int parse_type_not_arr_ptr_func(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	cvrflags_t cvrs = parse_cvrflags(state);
	switch (state->lexer.curr.ty) {
	case tok_union:
	case tok_struct:
		{
			const bool is_struct = state->lexer.curr.ty == tok_struct;
			lexer_next(&state->lexer);
			const int tyname = parse_ident(state);
			int members = AST_SENTINAL;
			if (state->lexer.curr.ty == tok_lbrace) { // structure definition
				lexer_next(&state->lexer); // parse members
				int *last = &members;
				while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
					int member = parse_type(state, true, true);
					*last = member;
					while (state->buf[member].next != AST_SENTINAL) member = state->buf[member].next;
					last = &state->buf[member].next;
					parser_eat(state, tok_semicol, "\";\"");
				}
				lexer_next(&state->lexer);
			}
			return parser_add(state, (ast_t){
				.ty = ast_type,
				.tok = (tok_t){ .ty = tok_undefined },
				.next = AST_SENTINAL,
				.info.type = {
					.ty = is_struct ? decl_struct : decl_union,
					.tyname = tyname,
					.cvrs = cvrs,
					.inner = members,
				},
				.line = line,
				.chr = chr,
			});
		}
		break;
	case tok_enum:
		{
			lexer_next(&state->lexer);
			const int tyname = parse_ident(state);

			int fields = AST_SENTINAL;
			if (state->lexer.curr.ty == tok_lbrace) { // enum definition
				lexer_next(&state->lexer); // parse fields
				int *last = &fields;
				while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
					const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
					const tok_t field_name = state->lexer.curr;
					int value = AST_SENTINAL;

					lexer_next(&state->lexer);
					if (state->lexer.curr.ty == tok_eq) {
						lexer_next(&state->lexer);
						value = parse_expression(state, prec_ternary);
					}
					if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);

					const int field = parser_add(state, (ast_t){
						.ty = ast_enum_field,
						.next = AST_SENTINAL,
						.tok = field_name,
						.info.inner = value,
						.line = line,
						.chr = chr,
					});
					*last = field;
					last = &state->buf[field].next;
				}
				lexer_next(&state->lexer);
			}
			return parser_add(state, (ast_t){
				.ty = ast_type,
				.tok = (tok_t){ .ty = tok_undefined },
				.next = AST_SENTINAL,
				.info.type = {
					.ty = decl_enum,
					.tyname = tyname,
					.cvrs = cvrs,
					.inner = fields,
				},
				.line = line,
				.chr = chr,
			});
		}
		break;
	case tok_ident: // Using an already made typedef
		{
			if (!ident_map_get(&state->tymap, state->lexer.curr.lit, state->lexer.curr.len, NULL)) return AST_SENTINAL;

			const int tyname = parse_ident(state);
			return parser_add(state, (ast_t){
				.ty = ast_type,
				.tok = (tok_t){ .ty = tok_undefined },
				.next = AST_SENTINAL,
				.info.type = {
					.ty = decl_typedef,
					.cvrs = cvrs,
					.tyname = tyname,
				},
				.line = line,
				.chr = chr,
			});
		}
		break;
	case tok_void:
		lexer_next(&state->lexer);
		return parser_add(state, (ast_t){
			.ty = ast_type,
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.info.type = {
				.ty = decl_void,
				.cvrs = cvrs,
			},
			.line = line,
			.chr = chr,
		});
	default: break; // we are parsing a POD
	}

	int bits = 32;
	int f = 0, i = 0, u = 0, l = 0;
	while (true) {
		switch (state->lexer.curr.ty) {
		case tok_unsigned: u++; break;
		case tok_char: i++; bits = 8; break;
		case tok_short: i++; bits = 16; break;
		case tok_int: i++; bits = 32; break;
		case tok_long: l++; bits = 64; break;
		case tok_float: f++; bits = 32; break;
		case tok_double: f++; bits = 64; break;
		default: goto construct_pod;
		}
		lexer_next(&state->lexer);
	}
construct_pod:
	if ((u > 1 || f > 1 || l > 2)
		|| (u && f)
		|| (i && f)
		|| (l && f)) {
		parser_err(state, (err_t){ .ty = err_unexpected, .msg = "incorrect type spec info", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
	}
	unitty_t pod;
	switch (bits) {
	case 8: pod = u ? unit_u8 : unit_i8; break;
	case 16: pod = u ? unit_u16 : unit_i16; break;
	case 32: pod = f ? unit_f32 : (u ? unit_u32 : unit_i32); break;
	case 64: pod = f ? unit_f64 : (u ? unit_u64 : unit_i64); break;
	default: pod = unit_undefined; break;
	}

	return parser_add(state, (ast_t){
		.ty = ast_type,
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.info.type = {
			.ty = decl_pod,
			.cvrs = cvrs,
			.pod = pod,
		},
		.line = line,
		.chr = chr,
	});
}

// Parse initializer
// Could return either expression, or ast_init_list
static int parse_initializer(parser_t *const state, const bool allow_desginator) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	if ((state->lexer.curr.ty == tok_dot || state->lexer.curr.ty == tok_lbrack) && allow_desginator) {
		if (state->lexer.curr.ty == tok_dot) lexer_next(&state->lexer);
		const int accessor = parse_expression(state, prec_2);
		parser_eat(state, tok_eq, "\"=\"");
		const int init = parse_initializer(state, false);
		return parser_add(state, (ast_t){
			.ty = ast_init_designator,
			.next = AST_SENTINAL,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.designator = {
				.accessor = accessor,
				.init = init,
			},
			.line = line,
			.chr = chr,
		});
	}
	if (state->lexer.curr.ty != tok_lbrace) return parse_expression(state, prec_assign);
	lexer_next(&state->lexer);

	int list = AST_SENTINAL;
	int *last = &list;
	while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		const int node = parse_initializer(state, true);
		*last = node;
		last = &state->buf[node].next;
		if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
		else break;
	}

	parser_eat(state, tok_rbrace, "\"}\"");
	return parser_add(state, (ast_t){
		.ty = ast_init_list,
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.info.inner = list,
		.line = line,
		.chr = chr,
	});
}

// Multi decl's are for stuff like "int a, b, c;" (not including semicolon)
// Obviously, function parameters are a situation where you would not want
// multi_decl
static int _parse_type(parser_t *const state, const bool multi_decl, const bool allow_idents, const int tyspec) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	int curr_node = tyspec; // currnode is the child
							// tyspec is always child of curr_node
	int root = tyspec;		// root of declaration
	int *parent = &root;	// parent child pointer of curr_node
	
	// Collect tokens before the ident, then store ident for later?
	tok_t before[32];
	int btop = -1, bparens = 0;
	tok_t ident = (tok_t){ .ty = tok_undefined };
	while (state->lexer.curr.ty == tok_star // Check for anything for pointer wise
		|| state->lexer.curr.ty == tok_lparen
		|| state->lexer.curr.ty == tok_const
		|| state->lexer.curr.ty == tok_static
		|| state->lexer.curr.ty == tok_extern
		|| state->lexer.curr.ty == tok_restrict
		|| state->lexer.curr.ty == tok_volatile
		|| state->lexer.curr.ty == tok_rparen) {
		bparens += state->lexer.curr.ty == tok_lparen;
		if (state->lexer.curr.ty == tok_rparen && --bparens <= 0) break;
		before[++btop] = state->lexer.curr;
		lexer_next(&state->lexer);
	}
	if (state->lexer.curr.ty == tok_ident) { // Get identifier if we can
		if (!allow_idents) {
			parser_err(state, (err_t){ .ty = err_unexpected, .msg = "unexpected identifier", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			return AST_SENTINAL;
		}
		ident = state->lexer.curr;
		lexer_next(&state->lexer);
	} // If not its undefined identifier

	// Now parse functions, pointers, and arrays
	while (true) {
		// If function/array is here then do it!
		while (state->lexer.curr.ty == tok_lbrack
			|| state->lexer.curr.ty == tok_lparen) {
			if (state->lexer.curr.ty == tok_lbrack) {
				lexer_next(&state->lexer);
				const cvrflags_t cvrs = parse_cvrflags(state);
				curr_node = parser_add(state, (ast_t){
					.ty = ast_type,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.type = {
						.ty = decl_array_of,
						.cvrs = cvrs,
						.arrlen = parse_expression(state, prec_full),
						.inner = tyspec,
					},
					.next = AST_SENTINAL,
					.line = line,
					.chr = chr,
				});
				parser_eat(state, tok_rbrack, "\"]\"");

				*parent = curr_node;
				parent = &state->buf[curr_node].info.type.inner;
			} else { // function
				lexer_next(&state->lexer);
				int params = AST_SENTINAL;
				int *last = &params;
				while (state->lexer.curr.ty != tok_rparen) {
					int param = parse_type(state, false, true);
					*last = param;
					last = &state->buf[param].next;
					if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
					else break;
				}
				parser_eat(state, tok_rparen, "\")\"");
				curr_node = parser_add(state, (ast_t){
					.ty = ast_type,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.type = {
						.ty = decl_function,
						.cvrs = state->buf[root].info.type.cvrs,
						.inner = tyspec,
						.funcparams = params,
					},
					.next = AST_SENTINAL,
					.line = line,
					.chr = chr,
				});

				*parent = curr_node;
				parent = &state->buf[curr_node].info.type.inner;
			}
		}

		bool parened = state->lexer.curr.ty == tok_rparen && btop >= 0;

		// Get pointers and remove parenthases
		cvrflags_t cvrs = cvr_none;
		while (btop >= 0 && before[btop].ty != tok_lparen) {
			switch (before[btop].ty) {
			case tok_static: cvrs |= cvr_static; break;
			case tok_const: cvrs |= cvr_const; break;
			case tok_restrict: cvrs |= cvr_restrict; break;
			case tok_volatile: cvrs |= cvr_volatile; break;
			case tok_extern: cvrs |= cvr_extern; break;
			case tok_star:
				curr_node = parser_add(state, (ast_t){
					.ty = ast_type,
					.next = AST_SENTINAL,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.type = {
						.ty = decl_pointer_to,
						.cvrs = cvrs,
						.inner = tyspec,
					},
				});

				*parent = curr_node;
				parent = &state->buf[curr_node].info.type.inner;
				cvrs = cvr_none;
				break;
			default: assert(false && "implement error handling");
			}
			btop--;
		}
		if (parened && before[btop].ty == tok_lparen) lexer_next(&state->lexer);
		else parened = false;
		btop--;

		if (!parened) break;
	}

	// Add the definition onto it now
	int def = AST_SENTINAL;
	if (state->lexer.curr.ty == tok_eq && allow_idents) {
		lexer_next(&state->lexer);
		def = parse_initializer(state, false);
	}
	if (allow_idents && ident.ty != tok_undefined) {
		root = parser_add(state, (ast_t){
			.ty = ast_decldef,
			.info.decldef = {
				.type = root,
				.def = def,
			},
			.tok = ident,
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		});
	}

	if (multi_decl && state->lexer.curr.ty == tok_comma && allow_idents) {
		lexer_next(&state->lexer);
		int shallow_copy = parser_add(state, state->buf[tyspec]);
		state->buf[shallow_copy].info.type.inner = AST_SENTINAL;
		state->buf[shallow_copy].next = AST_SENTINAL;
		state->buf[root].next = _parse_type(state, true, true, shallow_copy);
	}

	return root;
}

static int parse_type(parser_t *const state, const bool multi_decl, const bool allow_idents) {
	bool is_typedef = false;
	if (state->lexer.curr.ty == tok_typedef) {
		is_typedef = true;
		lexer_next(&state->lexer);
	}
	// Type specifier (can be a whole struct definition!!!!!!)
	const int tyspec = parse_type_not_arr_ptr_func(state);
	if (tyspec == AST_SENTINAL) return AST_SENTINAL;
	// Now parse pointers, functions, and arrays (and get identifier)
	const int decl = _parse_type(state, multi_decl, allow_idents, tyspec);

	if (is_typedef) {
		// Add the typedef to the list
		ident_map_add(&state->tymap, state->buf[decl].tok.lit, state->buf[decl].tok.len, 0);

		return parser_add(state, (ast_t){
			.ty = ast_typedef,
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.info.inner = decl,
			.line = state->buf[decl].line,
			.chr = state->buf[decl].chr,
		});
	} else {
		return decl;
	}
}

static int parse_compound_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	int stmts = AST_SENTINAL;
	int *last = &stmts;
	while (last && state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		int stmt = parse_statement(state);
		*last = stmt;
		last = stmt == AST_SENTINAL ? NULL : &state->buf[stmt].next;
	}
	parser_eat(state, tok_rbrace, "\"}\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_compound,
			.inner = stmts,
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}

static void parse_label(parser_t *const state, labelty_t *const ty, int *const label) {
	// get the label
	if (state->lexer.curr.ty == tok_ident && state->lexer.peek.ty == tok_colon) {
		*ty = label_norm;
		*label = parse_ident(state);
		lexer_next(&state->lexer);
	} else if (state->lexer.curr.ty == tok_case) {
		*ty = label_case;
		lexer_next(&state->lexer);
		*label = parse_ident(state);
		parser_eat(state, tok_colon, "\":\"");
	} else if (state->lexer.curr.ty == tok_default) {
		*ty = label_default;
		*label = AST_SENTINAL;
		lexer_next(&state->lexer);
		parser_eat(state, tok_colon, "\":\"");
	} else {
		*ty = label_none;
		*label = AST_SENTINAL;
	}
}

static int parse_ifelse_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	const int cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	const int on_true = parse_statement(state);
	int on_false = AST_SENTINAL;
	if (state->lexer.curr.ty == tok_else) {
		lexer_next(&state->lexer);
		on_false = parse_statement(state);
	}
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_ifelse,
			.ifelse = {
				.cond = cond,
				.on_true = on_true,
				.on_false = on_false,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_switch_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	const int cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	const int inner = parse_statement(state);
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_switch,
			.cond = {
				.cond = cond,
				.inner = inner,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_while_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	const int cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	const int inner = parse_statement(state);
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_while,
			.cond = {
				.cond = cond,
				.inner = inner,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_dowhile_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	const int inner = parse_statement(state);
	parser_eat(state, tok_while, "\"while\"");
	parser_eat(state, tok_lparen, "\"(\"");
	const int cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	parser_eat(state, tok_semicol, "\";\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_while,
			.cond = {
				.cond = cond,
				.inner = inner,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_for_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	int init_clause = parse_expression(state, prec_full);
	if (init_clause == AST_SENTINAL) {
		init_clause = parse_type(state, true, true);
	}
	parser_eat(state, tok_semicol, "\";\"");
	const int cond_expr = parse_expression(state, prec_full);
	parser_eat(state, tok_semicol, "\";\"");
	const int iter_expr = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	const int stmt = parse_statement(state);
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_for,
			.forinfo = {
				.init_clause = init_clause,
				.cond_expr = cond_expr,
				.iter_expr = iter_expr,
				.stmt = stmt,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_ret_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	const int expr = parse_expression(state, prec_full);
	parser_eat(state, tok_semicol, "\";\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_return,
			.inner = expr,
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_goto_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	const int label = parse_ident(state);
	parser_eat(state, tok_semicol, "\";\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_goto,
			.inner = label,
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.line = line,
		.chr = chr,
	});
}
static int parse_expr_decldef_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	const int decl = parse_type(state, true, true);
	if (decl != AST_SENTINAL) {
		parser_eat(state, tok_semicol, "\";\"");
		return parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = {
				.ty = stmt_decldef,
				.inner = decl,
			},
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		});
	}
	const int expr = parse_expression(state, prec_full);
	if (expr != AST_SENTINAL) {
		parser_eat(state, tok_semicol, "\";\"");
		return parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = {
				.ty = stmt_expr,
				.inner = expr,
			},
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		});
	}
	return AST_SENTINAL;
}

static int parse_statement(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	labelty_t labelty;
	int label;
	parse_label(state, &labelty, &label);

	int stmt = AST_SENTINAL;
	switch (state->lexer.curr.ty) {
	case tok_lbrace: stmt = parse_compound_stmt(state); break;
	case tok_if: stmt = parse_ifelse_stmt(state); break;
	case tok_switch: stmt = parse_switch_stmt(state); break;
	case tok_while: stmt = parse_while_stmt(state); break;
	case tok_do: stmt = parse_dowhile_stmt(state); break;
	case tok_for: stmt = parse_for_stmt(state); break;
	case tok_break:
		lexer_next(&state->lexer);
		stmt = parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = { .ty = stmt_break, },
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		});
		break;
	case tok_continue:
		lexer_next(&state->lexer);
		stmt = parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = { .ty = stmt_cont, },
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		});
		break;
	case tok_return: stmt = parse_ret_stmt(state); break;
	case tok_goto: stmt = parse_goto_stmt(state); break;
	case tok_semicol:
		lexer_next(&state->lexer);
		stmt = parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = { .ty = stmt_null, },
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.line = line,
			.chr = chr,
		});
		break;
	default: stmt = parse_expr_decldef_stmt(state); break;
	}

	if (stmt != AST_SENTINAL) {
		state->buf[stmt].info.stmt.label = label;
		state->buf[stmt].info.stmt.labelty = labelty;
	}
	return stmt;
}

static void parse_decl_toplvl(parser_t *const state, int *const first_def, int *const last_def) {
	int node = parse_type(state, true, true);

	if (node != AST_SENTINAL
		&& state->buf[node].ty == ast_decldef
		&& state->buf[node].info.decldef.def == AST_SENTINAL
		&& state->lexer.curr.ty == tok_lbrace) {
		state->buf[node].info.decldef.def = parse_statement(state);
	} else {
		parser_eat(state, tok_semicol, "\";\"");
	}

	if (first_def) *first_def = node;
	while (node != AST_SENTINAL && state->buf[node].next != AST_SENTINAL) node = state->buf[node].next;
	if (last_def) *last_def = node;
}

static void parse_toplevels(parser_t *const state) {
	int *last = &state->root;
	while (state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		int node = AST_SENTINAL;
		parse_decl_toplvl(state, last, &node);
		last = node == AST_SENTINAL ? NULL : &state->buf[node].next;
	}
}

static int parse_expression(
	parser_t *const state,
	const prec_t prec
) {
	const parse_unary_pfn prefix = _rules[state->lexer.curr.ty].prefix;
	if (!prefix) return AST_SENTINAL;
	int left = prefix(state);

	// Support for (ONLY ternay operator)
	if (state->lexer.curr.ty == tok_qmark && prec >= prec_ternary) {
		const tok_t tok = state->lexer.curr;
		lexer_next(&state->lexer);
		const int middle = parse_expression(state, prec_ternary);
		parser_eat(state, tok_colon, "\":\"");
		const int right = parse_expression(state, prec_ternary);
		left = parser_add(state, (ast_t){
			.ty = ast_ternary,
			.tok = tok,
			.info.ternary = {
				.cond = left,
				.ontrue = middle,
				.onfalse = right,
			},
			.next = AST_SENTINAL,
			.line = tok.line,
			.chr = tok.chr,
		});
	} else if (state->lexer.curr.ty == tok_colon) {
		return left;
	}

	while (prec >= _rules[state->lexer.curr.ty].prec) {
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
static typed_unit_t parse_number(parser_t *const state) {
	// TODO: bigger numbers and postfixes for numbers
	char numbuf[25];
	const tok_t tok = state->lexer.curr;
	if (tok.len > 24) {
		parser_err(state, (err_t){ .ty = err_limit, .msg = "exceeded max literal integer size", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return (typed_unit_t){ .ty = unit_undefined };
	}
	memcpy(numbuf, tok.lit, tok.len);
	numbuf[tok.len] = '\0';
	lexer_next(&state->lexer);
	int64_t i = atoll(numbuf);
	if (i <= INT32_MAX) {
		return (typed_unit_t){
				.ty = unit_i32,
				.unit = (unit_t){ .i32 = i },
		};
	} else if (i <= UINT32_MAX) {
		return (typed_unit_t){
				.ty = unit_u32,
				.unit = (unit_t){ .u32 = i },
		};
	} else if (i <= INT64_MAX) {
		return (typed_unit_t){
				.ty = unit_i64,
				.unit = (unit_t){ .i64 = i },
		};
	} else {
		return (typed_unit_t){
				.ty = unit_u64,
				.unit = (unit_t){ .u64 = i },
		};
	}
}
static int parse_string_lit(parser_t *const state) {
	tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_literal,
		.tok = tok,
		.info.literal = {
			.ty = tok.ty == tok_string ? lit_str : lit_char,
			.val.lit = tok,
		},
		.next = AST_SENTINAL,
		.line = tok.line,
		.chr = tok.chr,
	});
}
static int parse_number_lit(parser_t *const state) {
	tok_t tok = state->lexer.curr;
	return parser_add(state, (ast_t){
		.ty = ast_literal,
		.tok = tok,
		.info.literal = {
			.ty = lit_pod,
			.val.pod = parse_number(state),
		},
		.next = AST_SENTINAL,
		.line = tok.line,
		.chr = tok.chr,
	});
}

static void _dbg_cvr_print(cvrflags_t cvrs) {
	if (cvrs & cvr_extern) printf("extern ");
	if (cvrs & cvr_volatile) printf("volatile ");
	if (cvrs & cvr_const) printf("const ");
	if (cvrs & cvr_static) printf("static ");
	if (cvrs & cvr_restrict) printf("restrict ");
}
static void _dbg_ast_print(const ast_t *const ast, int idx, int tabs) {
#define TAB for (int i = 0; i < tabs; i++) printf("  ");
#define LENPRINT(lit, len) for (int _i = 0; _i < (len); _i++) printf("%c", (lit)[_i]);
	if (idx == AST_SENTINAL) return;
	switch (ast[idx].ty) {
	case ast_binary: TAB printf("binary");
	case ast_eq:
		if (ast[idx].ty == ast_eq) {
			TAB printf("equal");
		}
		printf(": %s\n", tokty_to_str(ast[idx].tok.ty));
		_dbg_ast_print(ast, ast[idx].info.binary.left, tabs+1);
		_dbg_ast_print(ast, ast[idx].info.binary.right, tabs+1);
		break;
	case ast_comma:
		TAB printf("comma expr: \n");
		_dbg_ast_print(ast, ast[idx].info.comma.expr, tabs+1);
		TAB printf("comma next: \n");
		_dbg_ast_print(ast, ast[idx].info.comma.next, tabs+1);
		break;
	case ast_ternary:
		TAB printf("ternary cond: \n");
		_dbg_ast_print(ast, ast[idx].info.ternary.cond, tabs+1);
		TAB printf("ternary on true: \n");
		_dbg_ast_print(ast, ast[idx].info.ternary.ontrue, tabs+1);
		TAB printf("ternary on false: \n");
		_dbg_ast_print(ast, ast[idx].info.ternary.onfalse, tabs+1);
		break;
	case ast_func_call:
		TAB printf("calling function:\n");
		_dbg_ast_print(ast, ast[idx].info.func_call.func, tabs+1);
		TAB printf("with params:\n");
		_dbg_ast_print(ast, ast[idx].info.func_call.params, tabs+1);
		break;
	case ast_unary:
		TAB printf("unary: %s\n", tokty_to_str(ast[idx].tok.ty));
		_dbg_ast_print(ast, ast[idx].info.inner, tabs+1);
		break;
	case ast_literal:
		switch (ast[idx].info.literal.ty) {
		case lit_pod:
			TAB printf("pod literal:\n");
			TAB printf("  "); dbg_typed_unit_print(ast[idx].info.literal.val.pod);
			break;
		case lit_str:
			TAB printf("string literal:\n");
			TAB printf("  "); LENPRINT(ast[idx].info.literal.val.lit.lit, ast[idx].info.literal.val.lit.len) printf("\n");
			break;
		case lit_char:
			TAB printf("character literal:\n");
			TAB printf("  "); LENPRINT(ast[idx].info.literal.val.lit.lit, ast[idx].info.literal.val.lit.len) printf("\n");
			break;
		case lit_compound:
			TAB printf("compound literal:\n");
			TAB printf("  type:\n");
			_dbg_ast_print(ast, ast[idx].info.literal.val.compound.type, tabs+2);
			TAB printf("  initialized to:\n");
			_dbg_ast_print(ast, ast[idx].info.literal.val.compound.init, tabs+2);
			break;
		}
		break;
	case ast_ident:
		TAB printf("ident: ");
		LENPRINT(ast[idx].tok.lit, ast[idx].tok.len)
		printf("\n");
		break;
	case ast_cast:
		TAB printf("cast: \n");
		_dbg_ast_print(ast, ast[idx].info.cast.expr, tabs+1);
		TAB printf("to type: \n");
		_dbg_ast_print(ast, ast[idx].info.cast.type, tabs+1);
		break;
	case ast_accessor:
		TAB printf("accessor: ");
		switch (ast[idx].tok.ty) {
		case tok_arrow: printf("member pointer access\n");
		case tok_dot:
		case tok_lbrack:
			if (ast[idx].tok.ty == tok_lbrack) printf("subscript\n");
			if (ast[idx].tok.ty == tok_dot) printf("member access\n");
			TAB printf("accessing from: \n");
			_dbg_ast_print(ast, ast[idx].info.accessor.access_from, tabs+1);
			TAB printf("accessing: \n");
			_dbg_ast_print(ast, ast[idx].info.accessor.accessing, tabs+1);
			break;
		case tok_star: printf("derefrence");
		case tok_bitand:
			if (ast[idx].tok.ty == tok_bitand) printf("refrence");
			printf(" of \n");
			_dbg_ast_print(ast, ast[idx].info.inner, tabs+1);
			break;
		default: break;
		}
		break;
	case ast_typedef:
		TAB printf("decl typedef\n");
		_dbg_ast_print(ast, ast[idx].info.inner, tabs+1);
		break;
	case ast_enum_field:
		TAB LENPRINT(ast[idx].tok.lit, ast[idx].tok.len)
		if (ast[idx].info.inner != AST_SENTINAL) {
			printf(" = \n");
			_dbg_ast_print(ast, ast[idx].info.inner, tabs+1);
		} else {
			printf(",\n");
		}
		break;
	case ast_error:
		printf("ERR");
		break;
	case ast_type:
		{
			TAB
			const ast_t *node = ast + idx;
			printf("type ");
			_dbg_cvr_print(node->info.type.cvrs);
			switch (node->info.type.ty) {
			case decl_typedef:
				printf("typedef:\n");
				_dbg_ast_print(ast, node->info.type.tyname, tabs+1);
				break;
			case decl_void: printf("void\n"); break;
			case decl_function:
				printf("function returns\n");
				_dbg_ast_print(ast, node->info.type.inner, tabs+1);
				TAB printf("and takes\n");
				_dbg_ast_print(ast, node->info.type.funcparams, tabs+1);
				break;
			case decl_pointer_to:
				printf("pointer to\n");
				_dbg_ast_print(ast, node->info.type.inner, tabs + 1);
				break;
			case decl_array_of: 
				if (node->info.type.arrlen != AST_SENTINAL) {
					printf("array [\n");
					_dbg_ast_print(ast, node->info.type.arrlen, tabs+1);
					TAB printf("] of \n");
				} else {
					printf("array of \n");
				}
				_dbg_ast_print(ast, node->info.type.inner, tabs + 1); 
				break;
			case decl_pod: printf("%s\n", unitty_to_str(node->info.type.pod)); break;
			case decl_enum:
			case decl_struct:
			case decl_union:
				if (node->info.type.ty == decl_struct) printf("struct:\n");
				else if (node->info.type.ty == decl_enum) printf("enum:\n");
				else printf("union:\n");
				if (node->info.type.inner != AST_SENTINAL) {
					_dbg_ast_print(ast, node->info.type.tyname, tabs+1);
					TAB printf(" {\n");
					if (node->info.type.inner != AST_SENTINAL) {
						_dbg_ast_print(ast, node->info.type.inner, tabs+1);
					}
					TAB printf("}\n");
				} else {
					_dbg_ast_print(ast, node->info.type.tyname, tabs+1);
					//TAB printf("\n");
				}
				break;
			default: printf("undefined\n"); break;
			}
			break;
		}
	case ast_decldef:
		TAB
		if (ast[idx].info.decldef.def != AST_SENTINAL) printf("define ");
		else printf("declare ");
		LENPRINT(ast[idx].tok.lit, ast[idx].tok.len);
		printf(": \n");
		_dbg_ast_print(ast, ast[idx].info.decldef.type, tabs + 1);
		if (ast[idx].info.decldef.def != AST_SENTINAL) {
			TAB printf("as: \n");
			_dbg_ast_print(ast, ast[idx].info.decldef.def, tabs + 1);
		}
		break;
	case ast_init_list:
		TAB printf("{\n");
		_dbg_ast_print(ast, ast[idx].info.inner, tabs+1);
		TAB printf("}\n");
		break;
	case ast_init_designator:
		TAB printf("desginator: \n");
		_dbg_ast_print(ast, ast[idx].info.designator.accessor, tabs+1);
		TAB printf("initialized to: \n");
		_dbg_ast_print(ast, ast[idx].info.designator.init, tabs+1);
		break;
	case ast_stmt:
		{
			const ast_t *node = ast + idx;
			TAB
			switch (node->info.stmt.labelty) {
			case label_none: break;
			case label_default: printf("label default:\n"); break;
			case label_case: printf("label case ident:\n"); break;
			case label_norm: printf("label ident:\n"); break;
			}
			_dbg_ast_print(ast, node->info.stmt.label, tabs+1);
			if (node->info.stmt.labelty != label_none) TAB
			switch (node->info.stmt.ty) {
			case stmt_decldef:
				printf("decl/def stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.inner, tabs+1);
				break;
			case stmt_expr:
				printf("expression stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.inner, tabs+1);
				break;
			case stmt_break: printf("break stmt\n"); break;
			case stmt_cont: printf("continue stmt\n"); break;
			case stmt_return:
				printf("return stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.inner, tabs+1);
				break;
			case stmt_goto:
				printf("goto stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.inner, tabs+1);
				break;
			case stmt_compound:
				printf("compound stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.inner, tabs+1);
				break;
			case stmt_dowhile:
				printf("dowhile stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.cond.inner, tabs+1);
				TAB printf("dowhile cond:\n");
				_dbg_ast_print(ast, node->info.stmt.cond.cond, tabs+1);
				break;
			case stmt_while:
				printf("while cond:\n");
				_dbg_ast_print(ast, node->info.stmt.cond.cond, tabs+1);
				TAB printf("while stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.cond.inner, tabs+1);
				break;
			case stmt_switch:
				printf("switch cond:\n");
				_dbg_ast_print(ast, node->info.stmt.cond.cond, tabs+1);
				TAB printf("switch stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.cond.inner, tabs+1);
				break;
			case stmt_ifelse:
				printf("if cond:\n");
				_dbg_ast_print(ast, node->info.stmt.ifelse.cond, tabs+1);
				TAB printf("ontrue:\n");
				_dbg_ast_print(ast, node->info.stmt.ifelse.on_true, tabs+1);
				TAB printf("onfalse:\n");
				_dbg_ast_print(ast, node->info.stmt.ifelse.on_false, tabs+1);
				break;
			case stmt_for:
				printf("for init:\n");
				_dbg_ast_print(ast, node->info.stmt.forinfo.init_clause, tabs+1);
				TAB printf("for cond:\n");
				_dbg_ast_print(ast, node->info.stmt.forinfo.cond_expr, tabs+1);
				TAB printf("for iter:\n");
				_dbg_ast_print(ast, node->info.stmt.forinfo.iter_expr, tabs+1);
				TAB printf("for stmt:\n");
				_dbg_ast_print(ast, node->info.stmt.forinfo.stmt, tabs+1);
				break;
			case stmt_null: printf("null stmt:\n"); break;
			default: printf("stmt undefined\n"); break;
			}
		}
		break;
	default: break;
	}
	_dbg_ast_print(ast, ast[idx].next, tabs);
#undef TAB
}
void dbg_ast_print(const ast_t *const astbuf, int root) {
	_dbg_ast_print(astbuf, root, 0);
}

